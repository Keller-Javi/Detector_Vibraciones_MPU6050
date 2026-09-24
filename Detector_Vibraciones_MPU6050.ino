#include <WiFi.h>
#include <Wire.h>
#include <arduinoFFT.h>
#include "webserver.h" 

// ============================================================
// Doble Buffer
// ============================================================

struct Data{
    float Ax, Ay, Az;
};

Data rawBuffer[2][N];
volatile uint8_t writeBufferIdx = 0;

// Buffers dedicados para el cálculo de FFT (Eje Z como ejemplo de vibración)
float vReal[N];
float vImag[N];

// Buffer final listo para ser consumido por el servidor web
Data finalSignal[N];
Data finalFFT[N / 2];        // La FFT simétrica solo requiere N/2 bins útiles
volatile bool webDataReady = false;

// Sincronización FreeRTOS
SemaphoreHandle_t semStartFFT;   // Notifica a la tarea FFT que hay un buffer lleno
SemaphoreHandle_t webMutex;      // Protege la lectura/escritura hacia el servidor web

// Instancia del motor FFT (Sintaxis arduinoFFT v2.x)
ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, N, SAMPLING_FREQ);

// ============================================================
// PROTOTIPOS
// ============================================================
void readMPU6050(float &Ax, float &Ay, float &Az);
void computeAxisFFT(float* targetOutput, int offsetData, uint8_t bufferIdx);

// ============================================================
// TAREA NÚCLEO 0: Wi-Fi y Servidor Web
// ============================================================
void taskServerCore0(void *pvParameters) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[Core 0] Conectando a WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    Serial.println("\n[Core 0] Conectado! IP: " + WiFi.localIP().toString());

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", INDEX_HTML);
    });

    // Endpoint unificado: Envía la señal temporal y los bins de magnitud de la FFT
    server.on("/data", HTTP_GET, []() {
        bool available = false;

        // Estructuras locales en stack pequeño o directo por stream
        if (xSemaphoreTake(webMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (webDataReady) {
                available = true;
                webDataReady = false; // Consumido
            }
            xSemaphoreGive(webMutex);
        }

        if (available) {
            // Se envía un paquete binario continuo:
            // [1024 * sizeof(Data)] + [(1024 / 2) * sizeof(float)] = 12288 + 2048 = 14336 bytes
            size_t totalBytes = sizeof(finalSignal) + sizeof(finalFFT);
            
            server.setContentLength(totalBytes);
            server.send(200, "application/octet-stream", "");
            
            WiFiClient client = server.client();
            // Envío en dos tramos contiguos sin saturar la RAM
            client.write((const uint8_t*)finalSignal, sizeof(finalSignal));
            client.write((const uint8_t*)finalFFT, sizeof(finalFFT));
        } else {
            server.send(204); // No Content si aún se está calculando
        }
    });

    server.begin();

    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// ============================================================
// CORE 1: Tarea 1 - Muestreo MPU6050 (Prioridad Alta: 3)
// ============================================================
void taskSampleCore1(void *pvParameters) {
    int sampleIndex = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLING_TS);

    for (;;) {
        float Ax, Ay, Az;
        readMPU6050(Ax, Ay, Az);

        rawBuffer[writeBufferIdx][sampleIndex] = {Ax, Ay, Az};
        sampleIndex++;

        // Si se completaron las muestras
        if (sampleIndex >= N) {
            sampleIndex = 0;
            writeBufferIdx = 1 - writeBufferIdx; // Cambia al otro buffer de inmediato

            // Despierta a la tarea de FFT
            xSemaphoreGive(semStartFFT);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// ============================================================
// CORE 1: Cálculo de FFT para Ax, Ay y Az (Prioridad 2)
// ============================================================
void taskFFTCore1(void *pvParameters) {
    for (;;) {
        if (xSemaphoreTake(semStartFFT, portMAX_DELAY) == pdTRUE) {
            uint8_t processIdx = 1 - writeBufferIdx;

            // Calcular FFT para cada componente secuencialmente
            computeAxisFFT(nullptr, 0, processIdx); // Ax
            computeAxisFFT(nullptr, 1, processIdx); // Ay
            computeAxisFFT(nullptr, 2, processIdx); // Az

            // Copiar la señal temporal al buffer final
            if (xSemaphoreTake(webMutex, portMAX_DELAY) == pdTRUE) {
                memcpy(finalSignal, (void*)rawBuffer[processIdx], sizeof(finalSignal));
                webDataReady = true;
                xSemaphoreGive(webMutex);
            }
        }
    }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Semáforos
    semStartFFT = xSemaphoreCreateBinary();
    webMutex    = xSemaphoreCreateMutex();

    // --------------------------------------------------------
    // I2C
    // --------------------------------------------------------
    Wire.begin(SDA_PIN, SCL_PIN);

    Serial.println();
    Serial.println("Iniciando MPU6050...");

    // --------------------------------------------------------
    // WAKE UP
    // --------------------------------------------------------

    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x6B);
    Wire.write(0x00);
    byte error = Wire.endTransmission();

    if (error == 0) {
        Serial.println("MPU6050 encontrado!" );
    } else { Serial.println("Error comunicando con MPU6050");}


    // --------------------------------------------------------
    // DLPF MPU6050
    // --------------------------------------------------------
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x1A);
    // DLPF ~260 Hz
    Wire.write(0x00);
    Wire.endTransmission();

    // --------------------------------------------------------
    // WIFI y HTTP Core 0
    // --------------------------------------------------------

    // Crear la tarea para el Servidor Web en Núcleo 0
    xTaskCreatePinnedToCore(
        taskServerCore0,    // Función que ejecuta la tarea
        "TaskServerCore0",  // Nombre
        4096,               // Tamaño del stack (bytes/palabras según config)
        NULL,               // Parámetros
        1,                  // Prioridad
        NULL,               // Manejador
        0                   // Núcleo 0
    );

    // --------------------------------------------------------
    // Tarea de adquisición Core 1
    // --------------------------------------------------------
    xTaskCreatePinnedToCore(
        taskSampleCore1,    // Función que ejecuta la tarea
        "TaskSampleCore1",  // Nombre
        8192,               // Tamaño del stack (bytes/palabras según config)
        NULL,               // Parámetros
        2,                  // Prioridad
        NULL,               // Manejador
        1                   // Núcleo 0
    );

    // --------------------------------------------------------
    // Tarea de adquisición Core 1
    // --------------------------------------------------------
    xTaskCreatePinnedToCore(
        taskFFTCore1,    // Función que ejecuta la tarea
        "TaskFFTCore1",  // Nombre
        4096,               // Tamaño del stack (bytes/palabras según config)
        NULL,               // Parámetros
        3,                  // Prioridad
        NULL,               // Manejador
        1                   // Núcleo 0
    );
}


// ============================================================
// LOOP
// ============================================================
void loop() {
    // Toda la ejecución se delegó a tareas FreeRTOS dedicadas
    vTaskDelete(NULL); // Elimina la tarea loop() para liberar memoria de stack
}

// ============================================================
// LEER MPU6050
// ============================================================

void readMPU6050(float &Ax, float &Ay, float &Az) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6050_ADDR, 6, true);

    int16_t AcX = Wire.read() << 8 | Wire.read();
    int16_t AcY = Wire.read() << 8 | Wire.read();
    int16_t AcZ = Wire.read() << 8 | Wire.read();

    Ax = AcX / 16384.0;
    Ay = AcY / 16384.0;
    Az = AcZ / 16384.0;
}

// ============================================================
// FUNCIÓN AXILIAR DE CALCULO DE FFT
// ============================================================
void computeAxisFFT(float* targetOutput, int offsetData, uint8_t bufferIdx) {
    for (int i = 0; i < N; i++) {
        // Offset: 0 para Ax, 1 para Ay, 2 para Az
        float* samplePtr = (float*)&rawBuffer[bufferIdx][i];
        vReal[i] = *(samplePtr + offsetData);
        vImag[i] = 0.0f;
    }

    FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();

    // Guardar las primeras N/2 magnitudes en el arreglo final
    for (int i = 0; i < N / 2; i++) {
        float* outPtr = (float*)&finalFFT[i];
        *(outPtr + offsetData) = vReal[i];
    }
}