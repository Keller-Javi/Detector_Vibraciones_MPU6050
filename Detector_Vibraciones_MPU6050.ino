#include <WiFi.h>
#include <Wire.h>

#include "webserver.h" 

// ============================================================
// DEFINICIONES
// ============================================================

#define SDA_PIN 4
#define SCL_PIN 5

#define MPU6050_ADDR 0x68

#define N 1024

// ============================================================
// Doble Buffer
// ============================================================

struct Data{
    float Ax, Ay, Az;
};

Data buffer[2][N];
volatile uint8_t writeBufferIdx = 0;
volatile uint8_t readBufferIdx  = 1;
volatile bool bufferReady = false;

int sampleIndex = 0;

// Mutex para el intercambio seguro del puntero de buffer
SemaphoreHandle_t bufferMutex;

// ============================================================
// TAREA NÚCLEO 0: Wi-Fi y Servidor Web
// ============================================================
void taskServerCore0(void *pvParameters) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Conectando al WiFi en Núcleo 0");

    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }

    Serial.println("\nWiFi conectado!");
    Serial.print("IP del ESP32: ");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", INDEX_HTML);
    });

    // Devuelve la muestra representativa más reciente disponible
    server.on("/data", HTTP_GET, []() {
    uint8_t readyIdx = 0;
    bool hasData = false;

    if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        hasData = bufferReady;
        readyIdx = readBufferIdx;
        bufferReady = false; // Marcar como consumido hasta el siguiente swap
        xSemaphoreGive(bufferMutex);
    }

    if (hasData) {
        // Envía el bloque crudo de 1024 structs Data (12288 bytes)
        server.send_P(200, "application/octet-stream", (const char*)buffer[readyIdx], sizeof(buffer[readyIdx]));
    } else {
        // El buffer todavía se está llenando
        server.send(204); 
    }
});

    server.begin();
    Serial.println("Servidor HTTP iniciado en Núcleo 0");

    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2)); 
    }
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
// SETUP
// ============================================================

void setup() {

    Serial.begin(115200);

    delay(1000);

    // Inicialización del Mutex
    bufferMutex = xSemaphoreCreateMutex();

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
}


// ============================================================
// LOOP (Núcleo 1 - Adquisición de datos)
// ============================================================
void loop() {
    float Ax, Ay, Az;
    readMPU6050(Ax, Ay, Az);

    // Escribir en el buffer actual
    buffer[writeBufferIdx][sampleIndex] = {Ax, Ay, Az};
    sampleIndex++;

    // Si se llena el lote de 1024 muestras, intercambiar buffers
    if (sampleIndex >= N) {
        sampleIndex = 0;

        if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            readBufferIdx = writeBufferIdx;            // El buffer lleno pasa a ser de lectura
            writeBufferIdx = 1 - writeBufferIdx;       // Alternar entre 0 y 1 para escribir
            bufferReady = true;
            xSemaphoreGive(bufferMutex);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1)); // Muestreo cada 10 ms (100 Hz)
}