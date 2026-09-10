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

<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<title>ESP32 MPU6050</title>

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

h1 {
    margin-top: 0;
}

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

button:hover {
    background: #444;
}

.status {
    margin-top: 15px;
}

</style>

</head>


<body>

<div class="container">

<h1>MPU6050 — ESP32</h1>

<canvas id="graph"></canvas>


<div class="values">

    <div class="value">
        Ax
        <span id="ax">0.000 g</span>
    </div>

    <div class="value">
        Ay
        <span id="ay">0.000 g</span>
    </div>

    <div class="value">
        Az
        <span id="az">0.000 g</span>
    </div>

</div>


<div class="controls">

    <button onclick="clearGraph()">
        Limpiar
    </button>

    <button onclick="togglePause()">
        Pausar
    </button>

</div>


<div class="status">

    Estado:
    <span id="status">
        Conectando...
    </span>

</div>

</div>


<script>

// ============================================================
// CONFIGURACIÓN
// ============================================================

const canvas = document.getElementById("graph");
const ctx = canvas.getContext("2d");

let paused = false;

let dataAx = [];
let dataAy = [];
let dataAz = [];

const MAX_POINTS = 300;


// ============================================================
// CANVAS
// ============================================================

function resizeCanvas() {

    canvas.width = canvas.clientWidth;
    canvas.height = canvas.clientHeight;

    drawGraph();
}

window.addEventListener("resize", resizeCanvas);

resizeCanvas();


// ============================================================
// OBTENER DATOS DEL ESP32
// ============================================================

async function getData() {

    if (paused)
        return;

    try {

        const response = await fetch("/data");

        if (!response.ok)
            throw new Error("HTTP error");

        const text = await response.text();

        const values = text.trim().split(",");

        if (values.length < 3)
            return;

        const ax = parseFloat(values[0]);
        const ay = parseFloat(values[1]);
        const az = parseFloat(values[2]);

        if (
            !Number.isFinite(ax) ||
            !Number.isFinite(ay) ||
            !Number.isFinite(az)
        ) {
            return;
        }


        // ----------------------------------------------------
        // MOSTRAR VALORES
        // ----------------------------------------------------

        document.getElementById("ax").textContent =
            ax.toFixed(3) + " g";

        document.getElementById("ay").textContent =
            ay.toFixed(3) + " g";

        document.getElementById("az").textContent =
            az.toFixed(3) + " g";


        // ----------------------------------------------------
        // GUARDAR DATOS
        // ----------------------------------------------------

        dataAx.push(ax);
        dataAy.push(ay);
        dataAz.push(az);


        if (dataAx.length > MAX_POINTS) {

            dataAx.shift();
            dataAy.shift();
            dataAz.shift();

        }


        drawGraph();


        document.getElementById("status").textContent =
            "Conectado";

    }

    catch (error) {

        document.getElementById("status").textContent =
            "Error de conexión";

        console.log(error);

    }

}


// ============================================================
// ACTUALIZAR CADA 50 ms
// ============================================================

setInterval(getData, 50);


// ============================================================
// GRAFICAR
// ============================================================

function drawGraph() {

    const w = canvas.width;
    const h = canvas.height;

    ctx.clearRect(0, 0, w, h);


    // --------------------------------------------------------
    // RANGO
    // --------------------------------------------------------

    const minY = -2;
    const maxY = 2;


    function yToPixel(value) {

        return h -
            ((value - minY) /
            (maxY - minY)) * h;

    }


    function xToPixel(index) {

        if (MAX_POINTS <= 1)
            return 0;

        return index /
            (MAX_POINTS - 1) * w;

    }


    // --------------------------------------------------------
    // GRID
    // --------------------------------------------------------

    ctx.strokeStyle = "#333";
    ctx.lineWidth = 1;

    for (
        let g = -2;
        g <= 2;
        g += 0.5
    ) {

        const y = yToPixel(g);

        ctx.beginPath();

        ctx.moveTo(0, y);
        ctx.lineTo(w, y);

        ctx.stroke();

    }


    // --------------------------------------------------------
    // LÍNEA 0 g
    // --------------------------------------------------------

    ctx.strokeStyle = "#666";

    const zeroY = yToPixel(0);

    ctx.beginPath();

    ctx.moveTo(0, zeroY);
    ctx.lineTo(w, zeroY);

    ctx.stroke();


    // --------------------------------------------------------
    // DIBUJAR SERIE
    // --------------------------------------------------------

    function drawSeries(data, lineStyle) {

        if (data.length < 2)
            return;

        ctx.strokeStyle = lineStyle;
        ctx.lineWidth = 2;

        ctx.beginPath();


        for (
            let i = 0;
            i < data.length;
            i++
        ) {

            const x = xToPixel(
                MAX_POINTS -
                data.length +
                i
            );

            const y = yToPixel(data[i]);


            if (i === 0)
                ctx.moveTo(x, y);
            else
                ctx.lineTo(x, y);

        }


        ctx.stroke();

    }


    // --------------------------------------------------------
    // AX AY AZ
    // --------------------------------------------------------

    drawSeries(dataAx, "#ff5555");
    drawSeries(dataAy, "#55ff55");
    drawSeries(dataAz, "#5599ff");


    // --------------------------------------------------------
    // ESCALA
    // --------------------------------------------------------

    ctx.fillStyle = "#aaa";
    ctx.font = "12px Arial";


    for (
        let g = -2;
        g <= 2;
        g += 0.5
    ) {

        const y = yToPixel(g);

        ctx.fillText(
            g.toFixed(1) + " g",
            5,
            y - 4
        );

    }


    // --------------------------------------------------------
    // LEYENDA
    // --------------------------------------------------------

    ctx.font = "14px Arial";

    ctx.fillStyle = "#ff5555";
    ctx.fillText("Ax", 70, 25);

    ctx.fillStyle = "#55ff55";
    ctx.fillText("Ay", 110, 25);

    ctx.fillStyle = "#5599ff";
    ctx.fillText("Az", 150, 25);

}


// ============================================================
// LIMPIAR
// ============================================================

function clearGraph() {

    dataAx = [];
    dataAy = [];
    dataAz = [];

    drawGraph();

}


// ============================================================
// PAUSA
// ============================================================

function togglePause() {

    paused = !paused;

    const button =
        document.querySelector(
            ".controls button:nth-child(2)"
        );

    button.textContent =
        paused ? "Continuar" : "Pausar";

}

</script>

</body>

</html>

)rawliteral";

#endif