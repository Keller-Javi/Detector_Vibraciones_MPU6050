#ifndef WEBSERVER
#define WEBSERVER

#include <WebServer.h>

// ============================================================
// WiFi
// ============================================================
const char* WIFI_SSID = "Keller";
const char* WIFI_PASSWORD = "Keller1922";

WebServer server(80);

// ============================================================
// Web
// ============================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 MPU6050 — Señal y Espectro FFT</title>
<style>
body {
    margin: 0;
    background: #111;
    color: #eee;
    font-family: Arial, sans-serif;
}
.container {
    max-width: 1000px;
    margin: auto;
    padding: 20px;
}
h1 { margin-top: 0; font-size: 24px; }
h2 { font-size: 18px; margin: 15px 0 5px 0; color: #bbb; }
canvas {
    width: 100%;
    height: 250px;
    background: #181818;
    border: 1px solid #333;
    border-radius: 8px;
    display: block;
}
.values {
    display: flex;
    gap: 15px;
    margin-top: 15px;
}
.value {
    flex: 1;
    background: #1c1c1c;
    padding: 12px;
    border-radius: 8px;
    border-left: 4px solid #444;
}
.value.fft-highlight {
    border-left-color: #ffaa00;
}
.value span {
    display: block;
    font-size: 22px;
    margin-top: 5px;
    font-weight: bold;
}
.controls {
    margin-top: 15px;
    display: flex;
    gap: 10px;
}
button {
    padding: 10px 18px;
    background: #2a2a2a;
    color: white;
    border: 1px solid #555;
    border-radius: 5px;
    cursor: pointer;
}
button:hover { background: #3a3a3a; }
.status { margin-top: 12px; font-size: 14px; color: #888; }
</style>
</head>

<body>
<div class="container">
    <h1>Monitor MPU6050 — Adquisición y FFT en Tiempo Real</h1>

    <h2>Señal Temporal (1024 muestras @ 1000 Hz)</h2>
    <canvas id="timeGraph"></canvas>

    <h2>Espectro de Frecuencia FFT (Eje Az: 0 - 500 Hz)</h2>
    <canvas id="fftGraph"></canvas>

    <div class="values">
        <div class="value">Ax<span id="ax" style="color:#ff5555">0.000 g</span></div>
        <div class="value">Ay<span id="ay" style="color:#55ff55">0.000 g</span></div>
        <div class="value">Az<span id="az" style="color:#5599ff">0.000 g</span></div>
        <div class="value fft-highlight">Pico Frecuencia<span id="peakFreq" style="color:#ffaa00">0.0 Hz</span></div>
    </div>

    <div class="controls">
        <button onclick="togglePause()">Pausar</button>
    </div>

    <div class="status">
        Estado: <span id="status">Conectando...</span>
    </div>
</div>

<script>
const timeCanvas = document.getElementById("timeGraph");
const timeCtx = timeCanvas.getContext("2d");

const fftCanvas = document.getElementById("fftGraph");
const fftCtx = fftCanvas.getContext("2d");

let paused = false;
const TOTAL_POINTS = 1024;
const FFT_BINS = 512;
const SAMPLING_RATE = 1000; // Hz

let dataAx = [];
let dataAy = [];
let dataAz = [];
let dataFFT = [];

function resizeCanvases() {
    timeCanvas.width = timeCanvas.clientWidth;
    timeCanvas.height = timeCanvas.clientHeight;
    fftCanvas.width = fftCanvas.clientWidth;
    fftCanvas.height = fftCanvas.clientHeight;
    renderAll();
}
window.addEventListener("resize", resizeCanvases);
resizeCanvases();

// ============================================================
// OBTENER LOTE DE SEÑAL + FFT
// ============================================================
async function fetchBatch() {
    if (paused) return;

    try {
        const response = await fetch("/data");
        if (response.status === 204) return;
        if (!response.ok) throw new Error("HTTP " + response.status);

        const arrayBuffer = await response.arrayBuffer();
        
        // 1024 * 3 (Ax, Ay, Az) + 512 (FFT) = 3584 floats
        const expectedFloats = (TOTAL_POINTS * 3) + FFT_BINS;
        const floatArray = new Float32Array(arrayBuffer);

        if (floatArray.length !== expectedFloats) return;

        // 1. Extraer Señal Temporal
        dataAx = new Array(TOTAL_POINTS);
        dataAy = new Array(TOTAL_POINTS);
        dataAz = new Array(TOTAL_POINTS);

        for (let i = 0; i < TOTAL_POINTS; i++) {
            dataAx[i] = floatArray[i * 3 + 0];
            dataAy[i] = floatArray[i * 3 + 1];
            dataAz[i] = floatArray[i * 3 + 2];
        }

        // 2. Extraer Espectro FFT
        dataFFT = new Array(FFT_BINS);
        const fftOffset = TOTAL_POINTS * 3;
        let maxMag = 0;
        let peakIdx = 0;

        for (let i = 0; i < FFT_BINS; i++) {
            const mag = floatArray[fftOffset + i];
            dataFFT[i] = mag;
            // Ignorar bin 0 (componente continua / DC) al buscar el pico
            if (i > 1 && mag > maxMag) {
                maxMag = mag;
                peakIdx = i;
            }
        }

        const peakFreqHz = (peakIdx * (SAMPLING_RATE / TOTAL_POINTS)).toFixed(1);

        // Actualizar métricas
        const lastIdx = TOTAL_POINTS - 1;
        document.getElementById("ax").textContent = dataAx[lastIdx].toFixed(3) + " g";
        document.getElementById("ay").textContent = dataAy[lastIdx].toFixed(3) + " g";
        document.getElementById("az").textContent = dataAz[lastIdx].toFixed(3) + " g";
        document.getElementById("peakFreq").textContent = peakFreqHz + " Hz";

        renderAll();
        document.getElementById("status").textContent = "Conectado — Datos y FFT sincronizados";
    } catch (e) {
        document.getElementById("status").textContent = "Error de enlace";
    }
}

// Consultar cada 150 ms
setInterval(fetchBatch, 150);

// ============================================================
// DIBUJAR SEÑAL Y FFT
// ============================================================
function renderAll() {
    drawTimeGraph();
    drawFFTGraph();
}

function drawTimeGraph() {
    const w = timeCanvas.width;
    const h = timeCanvas.height;
    timeCtx.clearRect(0, 0, w, h);

    const minY = -2;
    const maxY = 2;

    const yToPix = val => h - ((val - minY) / (maxY - minY)) * h;
    const xToPix = i => (i / (TOTAL_POINTS - 1)) * w;

    // Grid
    timeCtx.strokeStyle = "#242424";
    for (let g = -2; g <= 2; g += 0.5) {
        const y = yToPix(g);
        timeCtx.beginPath();
        timeCtx.moveTo(0, y);
        timeCtx.lineTo(w, y);
        timeCtx.stroke();
    }

    // Cero
    timeCtx.strokeStyle = "#444";
    timeCtx.beginPath();
    timeCtx.moveTo(0, yToPix(0));
    timeCtx.lineTo(w, yToPix(0));
    timeCtx.stroke();

    function drawLine(data, color) {
        if (!data || data.length === 0) return;
        timeCtx.strokeStyle = color;
        timeCtx.lineWidth = 1.2;
        timeCtx.beginPath();
        for (let i = 0; i < data.length; i++) {
            const x = xToPix(i);
            const y = yToPix(data[i]);
            if (i === 0) timeCtx.moveTo(x, y);
            else timeCtx.lineTo(x, y);
        }
        timeCtx.stroke();
    }

    drawLine(dataAx, "#ff5555");
    drawLine(dataAy, "#55ff55");
    drawLine(dataAz, "#5599ff");
}

function drawFFTGraph() {
    const w = fftCanvas.width;
    const h = fftCanvas.height;
    fftCtx.clearRect(0, 0, w, h);

    if (!dataFFT || dataFFT.length === 0) return;

    // Escalar dinámicamente según la magnitud máxima (ignorando componente DC bin 0)
    let maxVal = 5;
    for (let i = 1; i < dataFFT.length; i++) {
        if (dataFFT[i] > maxVal) maxVal = dataFFT[i];
    }

    // Barras de espectro
    fftCtx.fillStyle = "#ffaa00";
    const barWidth = w / FFT_BINS;

    // Dibujamos omitiendo la componente DC (índice 0)
    for (let i = 1; i < FFT_BINS; i++) {
        const barHeight = (dataFFT[i] / maxVal) * (h - 20);
        const x = i * barWidth;
        const y = h - barHeight;
        fftCtx.fillRect(x, y, Math.max(barWidth, 1), barHeight);
    }

    // Grid vertical de frecuencias (cada 100 Hz: 100, 200, 300, 400, 500 Hz)
    fftCtx.strokeStyle = "#333";
    fftCtx.fillStyle = "#888";
    fftCtx.font = "11px Arial";

    for (let freq = 100; freq <= 500; freq += 100) {
        const bin = freq / (SAMPLING_RATE / TOTAL_POINTS);
        const x = bin * barWidth;

        fftCtx.beginPath();
        fftCtx.moveTo(x, 0);
        fftCtx.lineTo(x, h);
        fftCtx.stroke();
        fftCtx.fillText(freq + " Hz", x + 3, 14);
    }
}

function togglePause() {
    paused = !paused;
    const btn = document.querySelector(".controls button");
    btn.textContent = paused ? "Continuar" : "Pausar";
}
</script>
</body>
</html>
)rawliteral";

#endif