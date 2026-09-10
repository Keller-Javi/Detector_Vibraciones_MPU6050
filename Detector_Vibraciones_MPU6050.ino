#include <WiFi.h>
#include <Wire.h>

#include "webserver.h" 

// ============================================================
// MPU6050
// ============================================================

#define SDA_PIN 4
#define SCL_PIN 5

#define MPU6050_ADDR 0x68

// ============================================================
// FILTRO
// ============================================================

float alpha = 0.10;

float Ax_f = 0.0;
float Ay_f = 0.0;
float Az_f = 0.0;

// Mutex para sincronización entre núcleos
SemaphoreHandle_t dataMutex;

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

    server.on("/data", HTTP_GET, []() {
        float x = 0.0, y = 0.0, z = 0.0;

        // Lectura protegida con Mutex
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            x = Ax_f;
            y = Ay_f;
            z = Az_f;
            xSemaphoreGive(dataMutex);
        }

        String data = String(x, 3) + "," + String(y, 3) + "," + String(z, 3);
        server.send(200, "text/plain", data);
    });

    server.begin();
    Serial.println("Servidor HTTP iniciado en Núcleo 0");

    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2)); // Cede tiempo a la pila Wi-Fi de FreeRTOS
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
    dataMutex = xSemaphoreCreateMutex();

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

    // DLPF ~44 Hz
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
// LOOP
// ============================================================

void loop() {
    float Ax, Ay, Az;
    readMPU6050(Ax, Ay, Az);

    // Actualización protegida con Mutex
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        Ax_f = Ax;//+= alpha * (Ax - Ax_f);
        Ay_f = Ay;//+= alpha * (Ay - Ay_f);
        Az_f = Az;//+= alpha * (Az - Az_f);
        xSemaphoreGive(dataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // Muestreo cada 10 ms (100 Hz)
}