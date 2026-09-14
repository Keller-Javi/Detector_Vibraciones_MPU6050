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
<title>ESP32 MPU6050 — Doble Buffer (1024 Muestras)</title>
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
h1 { margin-top: 0; }
canvas {
    width: 100%;
    height: 450px;
    background: #181818;
    border: 1px solid #333;
    border-radius: 8px;
}
.values {
    display: flex;
    gap: 20px;
    margin-top: 20px;
}
.value {
    flex: 1;
    background: #1c1c1c;
    padding: 15px;
    border-radius: 8px;
}
.value span {
    display: block;
    font-size: 28px;
    margin-top: 5px;
}
.controls {
    margin-top: 20px;
    display: flex;
    gap: 10px;
}
button {
    padding: 10px 20px;
    background: #333;
    color: white;
    border: 1px solid #555;
    border-radius: 5px;
    cursor: pointer;
}
button:hover { background: #444; }
.status { margin-top: 15px; }
</style>
</head>

<body>
<div class="container">
    <h1>MPU6050 — Bloque de 1024 muestras</h1>
    <canvas id="graph"></canvas>

    <div class="values">
        <div class="value">Ax (último)<span id="ax">0.000 g</span></div>
        <div class="value">Ay (último)<span id="ay">0.000 g</span></div>
        <div class="value">Az (último)<span id="az">0.000 g</span></div>
    </div>

    <div class="controls">
        <button onclick="clearGraph()">Limpiar</button>
        <button onclick="togglePause()">Pausar</button>
    </div>

    <div class="status">
        Estado: <span id="status">Conectando...</span>
    </div>
</div>

<script>
const canvas = document.getElementById("graph");
const ctx = canvas.getContext("2d");

let paused = false;
let dataAx = [];
let dataAy = [];
let dataAz = [];
const TOTAL_POINTS = 1024;

function resizeCanvas() {
    canvas.width = canvas.clientWidth;
    canvas.height = canvas.clientHeight;
    drawGraph();
}
window.addEventListener("resize", resizeCanvas);
resizeCanvas();

// ============================================================
// OBTENER LOTE DE 1024 MUESTRAS EN BINARIO
// ============================================================
async function fetchBatch() {
    if (paused) return;

    try {
        const response = await fetch("/data");
        
        // 204 indica que el ESP32 aún no llenó un buffer nuevo
        if (response.status === 204) return;
        if (!response.ok) throw new Error("HTTP error " + response.status);

        const arrayBuffer = await response.arrayBuffer();
        const floatArray = new Float32Array(arrayBuffer);

        // Cada punto contiene 3 floats: [Ax, Ay, Az]
        if (floatArray.length !== TOTAL_POINTS * 3) return;

        dataAx = new Array(TOTAL_POINTS);
        dataAy = new Array(TOTAL_POINTS);
        dataAz = new Array(TOTAL_POINTS);

        for (let i = 0; i < TOTAL_POINTS; i++) {
            dataAx[i] = floatArray[i * 3 + 0];
            dataAy[i] = floatArray[i * 3 + 1];
            dataAz[i] = floatArray[i * 3 + 2];
        }

        // Mostrar el último valor del lote
        const lastIdx = TOTAL_POINTS - 1;
        document.getElementById("ax").textContent = dataAx[lastIdx].toFixed(3) + " g";
        document.getElementById("ay").textContent = dataAy[lastIdx].toFixed(3) + " g";
        document.getElementById("az").textContent = dataAz[lastIdx].toFixed(3) + " g";

        drawGraph();
        document.getElementById("status").textContent = "Conectado — Lote sincronizado";
    } catch (error) {
        document.getElementById("status").textContent = "Error de conexión";
    }
}

// Consultar cada 200 ms (a 100 Hz, 1024 muestras tardan ~10.2 segundos en llenarse)
setInterval(fetchBatch, 200);

// ============================================================
// DIBUJAR TRAZADO
// ============================================================
function drawGraph() {
    const w = canvas.width;
    const h = canvas.height;
    ctx.clearRect(0, 0, w, h);

    const minY = -2;
    const maxY = 2;

    function yToPixel(val) {
        return h - ((val - minY) / (maxY - minY)) * h;
    }

    function xToPixel(i) {
        return (i / (TOTAL_POINTS - 1)) * w;
    }

    // Grid
    ctx.strokeStyle = "#333";
    ctx.lineWidth = 1;
    for (let g = -2; g <= 2; g += 0.5) {
        const y = yToPixel(g);
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
    }

    // Línea central 0g
    ctx.strokeStyle = "#666";
    const zeroY = yToPixel(0);
    ctx.beginPath();
    ctx.moveTo(0, zeroY);
    ctx.lineTo(w, zeroY);
    ctx.stroke();

    // Dibujar curvas completas de 1024 muestras
    function drawSeries(data, color) {
        if (!data || data.length < 2) return;
        ctx.strokeStyle = color;
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        for (let i = 0; i < data.length; i++) {
            const x = xToPixel(i);
            const y = yToPixel(data[i]);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
    }

    drawSeries(dataAx, "#ff5555");
    drawSeries(dataAy, "#55ff55");
    drawSeries(dataAz, "#5599ff");

    // Escala
    ctx.fillStyle = "#aaa";
    ctx.font = "12px Arial";
    for (let g = -2; g <= 2; g += 0.5) {
        const y = yToPixel(g);
        ctx.fillText(g.toFixed(1) + " g", 5, y - 4);
    }

    // Leyenda
    ctx.font = "14px Arial";
    ctx.fillStyle = "#ff5555"; ctx.fillText("Ax", 70, 25);
    ctx.fillStyle = "#55ff55"; ctx.fillText("Ay", 110, 25);
    ctx.fillStyle = "#5599ff"; ctx.fillText("Az", 150, 25);
}

function clearGraph() {
    dataAx = []; dataAy = []; dataAz = [];
    drawGraph();
}

function togglePause() {
    paused = !paused;
    const button = document.querySelector(".controls button:nth-child(2)");
    button.textContent = paused ? "Continuar" : "Pausar";
}
</script>
</body>
</html>
)rawliteral";

#endif