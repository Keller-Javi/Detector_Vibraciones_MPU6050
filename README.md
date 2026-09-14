# Monitoreo de Vibraciones Anómalas en Motores (PDS — ESP32)

Sistema embebido para la adquisición, procesamiento y visualización en tiempo real de vibraciones mecánicas en motores. El Proyecto implementa conceptos fundamentales de **Procesamiento Digital de Señales (PDS)** sobre una arquitectura multicore con ESP32 y un acelerómetro MPU6050.

### 1. Núcleo 1 — Adquisición y Doble Buffer
* **Tasa de muestreo ($F_s$):** $1000\text{ Hz}$ ($T_s = 1\text{ ms}$).
* **Filtro analógico/digital del sensor (DLPF):** Configurado internamente en el MPU6050 para atenuar ruido de alta frecuencia y prevenir aliasing.
* **Estrategia Ping-Pong Buffer:** Se emplean dos buffers de $N = 1024$ muestras cada uno. Mientras el **Core 1** adquiere muestras en el buffer de escritura sin interrupciones, el buffer lleno queda disponible para lectura/transmisión. Al completarse la ventana de 1024 puntos, los roles se alternan mediante exclusión mutua (`SemaphoreHandle_t`).

### 2. Núcleo 0 — Comunicación y Transferencia Eficiente
* **Aislamiento de tareas:** Las tareas de red (Wi-Fi) y el despacho HTTP corren en el **Core 0**, impidiendo que la latencia del protocolo TCP/IP genere *jitter* en el reloj de muestreo del acelerómetro.
* **Payload Binario (`application/octet-stream`):** Al completarse cada bloque, el ESP32 transfiere las 1024 muestras en un único paquete binario crudo de $12\text{ KB}$ ($1024 \times 3 \times 4\text{ bytes}$), minimizando la sobrecarga de CPU y memoria frente a serializaciones en texto (JSON o CSV).

### 3. Interfaz Web (Frontend)
* Tablero embebido en la memoria flash (`PROGMEM`) accesible desde el navegador mediante la IP local del microcontrolador.
* Decodificación en tiempo real mediante `Float32Array` sobre un lienzo HTML5 Canvas, permitiendo inspeccionar la ventana temporal de 1024 puntos sin degradar la memoria del navegador.

---

## Hardware y Pines

| Periférico / Señal | Pin ESP32 | Descripción |
| :--- | :--- | :--- |
| **MPU6050 SDA** | `GPIO 4` | Línea de datos I2C |
| **MPU6050 SCL** | `GPIO 5` | Línea de reloj I2C |
| **VCC** | `3.3V` | Alimentación del módulo |
| **GND** | `GND` | Referencia común |

---