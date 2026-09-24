#ifndef CONFIG
#define CONFIG

#include "secrets.h" // <- Datos del wifi 

#define SDA_PIN 4
#define SCL_PIN 5

#define MPU6050_ADDR 0x68

#define N 2048
#define SAMPLING_FREQ   500.0 // Frecuencia de muestreo en Hz
#define SAMPLING_TS     2.0

// Frecuencia de Nyquist (Límite superior del espectro FFT)
#define NYQUIST_FREQ    (SAMPLING_FREQ / 2.0)

#endif