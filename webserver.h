#ifndef WEBSERVER
#define WEBSERVER

#include <WebServer.h>
#include "config.h"

// Macros para stringificar definiciones en tiempo de compilación
#define STR(x) #x
#define XSTR(x) STR(x)

WebServer server(80);

const char INDEX_HTML[] PROGMEM = 
"<!DOCTYPE html>"
"<html lang=\"es\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"<title>MPU6050 — 3-Axis FFT Analysis</title>"
"<style>"
"body {"
"    margin: 0;"
"    background: #121212;"
"    color: #e0e0e0;"
"    font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, sans-serif;"
"}"
".container {"
"    max-width: 1050px;"
"    margin: auto;"
"    padding: 20px;"
"}"
"h1 { margin: 0 0 15px 0; font-size: 22px; font-weight: 600; }"
"h2 { font-size: 16px; margin: 15px 0 8px 0; color: #aaa; font-weight: 500; }"
"canvas {"
"    width: 100%;"
"    height: 250px;"
"    background: #1a1a1a;"
"    border: 1px solid #2e2e2e;"
"    border-radius: 6px;"
"    display: block;"
"}"
".grid {"
"    display: grid;"
"    grid-template-columns: repeat(3, 1fr);"
"    gap: 12px;"
"    margin-top: 15px;"
"}"
".card {"
"    background: #1e1e1e;"
"    padding: 12px 16px;"
"    border-radius: 6px;"
"    border-left: 4px solid #444;"
"}"
".card-x { border-left-color: #ff5555; }"
".card-y { border-left-color: #55ff55; }"
".card-z { border-left-color: #5599ff; }"
".label { font-size: 12px; color: #888; text-transform: uppercase; letter-spacing: 0.5px; }"
".val { font-size: 20px; font-weight: bold; margin: 4px 0; }"
".peak { font-size: 13px; color: #bbb; }"
".controls { margin-top: 15px; }"
"button {"
"    padding: 8px 18px;"
"    background: #2a2a2a;"
"    color: #fff;"
"    border: 1px solid #444;"
"    border-radius: 4px;"
"    cursor: pointer;"
"}"
"button:hover { background: #383838; }"
".status { margin-top: 10px; font-size: 13px; color: #777; }"
"</style>"
"</head>"
"<body>"
"<div class=\"container\">"
"    <h1>Análisis Espectral Triaxial (MPU6050)</h1>"

"    <h2>Señal Temporal (" XSTR(N) " muestras @ " XSTR(SAMPLING_FREQ) " Hz)</h2>"
"    <canvas id=\"timeGraph\"></canvas>"

"    <h2>Espectro de Magnitud FFT (0 — <span id=\"nyquistLabel\"></span> Hz)</h2>"
"    <canvas id=\"fftGraph\"></canvas>"

"    <div class=\"grid\">"
"        <div class=\"card card-x\">"
"            <div class=\"label\">Eje X</div>"
"            <div class=\"val\" id=\"ax\" style=\"color:#ff5555\">0.000 g</div>"
"            <div class=\"peak\">Pico FFT: <b id=\"peakX\">0.0 Hz</b></div>"
"        </div>"
"        <div class=\"card card-y\">"
"            <div class=\"label\">Eje Y</div>"
"            <div class=\"val\" id=\"ay\" style=\"color:#55ff55\">0.000 g</div>"
"            <div class=\"peak\">Pico FFT: <b id=\"peakY\">0.0 Hz</b></div>"
"        </div>"
"        <div class=\"card card-z\">"
"            <div class=\"label\">Eje Z</div>"
"            <div class=\"val\" id=\"az\" style=\"color:#5599ff\">0.000 g</div>"
"            <div class=\"peak\">Pico FFT: <b id=\"peakZ\">0.0 Hz</b></div>"
"        </div>"
"    </div>"

"    <div class=\"controls\">"
"        <button onclick=\"togglePause()\">Pausar</button>"
"    </div>"

"    <div class=\"status\">Estado: <span id=\"status\">Conectando...</span></div>"
"</div>"

"<script>"
"const timeCanvas = document.getElementById(\"timeGraph\");"
"const timeCtx = timeCanvas.getContext(\"2d\");"

"const fftCanvas = document.getElementById(\"fftGraph\");"
"const fftCtx = fftCanvas.getContext(\"2d\");"

"let paused = false;"

// INYECCIÓN DINÁMICA DESDE CONFIG.H
"const TOTAL_POINTS = " XSTR(N) ";"
"const FFT_BINS = " XSTR(N) " / 2;"
"const SAMPLING_RATE = " XSTR(SAMPLING_FREQ) ";"
"const NYQUIST_FREQ = SAMPLING_RATE / 2;"

"document.getElementById(\"nyquistLabel\").textContent = NYQUIST_FREQ.toFixed(0);"

// Buffers locales
"let dataAx = [], dataAy = [], dataAz = [];"
"let fftAx = [], fftAy = [], fftAz = [];"

"function resize() {"
"    timeCanvas.width = timeCanvas.clientWidth;"
"    timeCanvas.height = timeCanvas.clientHeight;"
"    fftCanvas.width = fftCanvas.clientWidth;"
"    fftCanvas.height = fftCanvas.clientHeight;"
"    render();"
"}"
"window.addEventListener(\"resize\", resize);"
"resize();"

"async function fetchBatch() {"
"    if (paused) return;"

"    try {"
"        const response = await fetch(\"/data\");"
"        if (response.status === 204) return;"
"        if (!response.ok) throw new Error(\"HTTP \" + response.status);"

"        const arrayBuffer = await response.arrayBuffer();"
        
"        const expectedFloats = (TOTAL_POINTS * 3) + (FFT_BINS * 3);"
"        const fArr = new Float32Array(arrayBuffer);"

"        if (fArr.length !== expectedFloats) return;"

"        dataAx = new Array(TOTAL_POINTS);"
"        dataAy = new Array(TOTAL_POINTS);"
"        dataAz = new Array(TOTAL_POINTS);"

"        for (let i = 0; i < TOTAL_POINTS; i++) {"
"            dataAx[i] = fArr[i * 3 + 0];"
"            dataAy[i] = fArr[i * 3 + 1];"
"            dataAz[i] = fArr[i * 3 + 2];"
"        }"

"        const offsetFFT = TOTAL_POINTS * 3;"
"        fftAx = new Array(FFT_BINS);"
"        fftAy = new Array(FFT_BINS);"
"        fftAz = new Array(FFT_BINS);"

"        let maxMagX = 0, peakIdxX = 0;"
"        let maxMagY = 0, peakIdxY = 0;"
"        let maxMagZ = 0, peakIdxZ = 0;"

"        for (let i = 0; i < FFT_BINS; i++) {"
"            const mx = fArr[offsetFFT + (i * 3 + 0)];"
"            const my = fArr[offsetFFT + (i * 3 + 1)];"
"            const mz = fArr[offsetFFT + (i * 3 + 2)];"

"            fftAx[i] = mx;"
"            fftAy[i] = my;"
"            fftAz[i] = mz;"

"            if (i > 1) {"
"                if (mx > maxMagX) { maxMagX = mx; peakIdxX = i; }"
"                if (my > maxMagY) { maxMagY = my; peakIdxY = i; }"
"                if (mz > maxMagZ) { maxMagZ = mz; peakIdxZ = i; }"
"            }"
"        }"

"        const binToHz = bin => (bin * (SAMPLING_RATE / TOTAL_POINTS)).toFixed(1);"

"        document.getElementById(\"peakX\").textContent = binToHz(peakIdxX) + \" Hz\";"
"        document.getElementById(\"peakY\").textContent = binToHz(peakIdxY) + \" Hz\";"
"        document.getElementById(\"peakZ\").textContent = binToHz(peakIdxZ) + \" Hz\";"

"        const last = TOTAL_POINTS - 1;"
"        document.getElementById(\"ax\").textContent = dataAx[last].toFixed(3) + \" g\";"
"        document.getElementById(\"ay\").textContent = dataAy[last].toFixed(3) + \" g\";"
"        document.getElementById(\"az\").textContent = dataAz[last].toFixed(3) + \" g\";"

"        render();"
"        document.getElementById(\"status\").textContent = \"Sincronizado\";"
"    } catch (e) {"
"        document.getElementById(\"status\").textContent = \"Error de enlace\";"
"    }"
"}"

"setInterval(fetchBatch, 150);"

"function render() {"
"    drawTime();"
"    drawFFT();"
"}"

"function drawTime() {"
"    const w = timeCanvas.width, h = timeCanvas.height;"
"    timeCtx.clearRect(0, 0, w, h);"

"    const minY = -2, maxY = 2;"
"    const yToPix = v => h - ((v - minY) / (maxY - minY)) * h;"
"    const xToPix = i => (i / (TOTAL_POINTS - 1)) * w;"

"    timeCtx.strokeStyle = \"#252525\";"
"    for (let g = -2; g <= 2; g += 0.5) {"
"        const y = yToPix(g);"
"        timeCtx.beginPath(); timeCtx.moveTo(0, y); timeCtx.lineTo(w, y); timeCtx.stroke();"
"    }"

"    const plot = (arr, color) => {"
"        if (!arr || arr.length === 0) return;"
"        timeCtx.strokeStyle = color;"
"        timeCtx.lineWidth = 1.2;"
"        timeCtx.beginPath();"
"        for (let i = 0; i < arr.length; i++) {"
"            const x = xToPix(i), y = yToPix(arr[i]);"
"            if (i === 0) timeCtx.moveTo(x, y); else timeCtx.lineTo(x, y);"
"        }"
"        timeCtx.stroke();"
"    };"

"    plot(dataAx, \"#ff5555\");"
"    plot(dataAy, \"#55ff55\");"
"    plot(dataAz, \"#5599ff\");"
"}"

"function drawFFT() {"
"    const w = fftCanvas.width, h = fftCanvas.height;"
"    fftCtx.clearRect(0, 0, w, h);"

"    if (!fftAx || fftAx.length === 0) return;"

"    let maxVal = 5;"
"    for (let i = 2; i < FFT_BINS; i++) {"
"        if (fftAx[i] > maxVal) maxVal = fftAx[i];"
"        if (fftAy[i] > maxVal) maxVal = fftAy[i];"
"        if (fftAz[i] > maxVal) maxVal = fftAz[i];"
"    }"

"    fftCtx.strokeStyle = \"#2a2a2a\";"
"    fftCtx.fillStyle = \"#666\";"
"    fftCtx.font = \"11px sans-serif\";"

     // División dinámica de la cuadrícula de frecuencias (5 marcas equidistantes)
"    const stepHz = NYQUIST_FREQ / 5;"
"    for (let freq = stepHz; freq <= NYQUIST_FREQ; freq += stepHz) {"
"        const bin = freq / (SAMPLING_RATE / TOTAL_POINTS);"
"        const x = (bin / FFT_BINS) * w;"
"        fftCtx.beginPath(); fftCtx.moveTo(x, 0); fftCtx.lineTo(x, h); fftCtx.stroke();"
"        fftCtx.fillText(freq.toFixed(0) + \" Hz\", x + 4, 14);"
"    }"

"    const plotFFT = (arr, color) => {"
"        fftCtx.strokeStyle = color;"
"        fftCtx.lineWidth = 1.5;"
"        fftCtx.beginPath();"
"        for (let i = 1; i < FFT_BINS; i++) {"
"            const x = (i / (FFT_BINS - 1)) * w;"
"            const y = h - ((arr[i] / maxVal) * (h - 25));"
"            if (i === 1) fftCtx.moveTo(x, y); else fftCtx.lineTo(x, y);"
"        }"
"        fftCtx.stroke();"
"    };"

"    plotFFT(fftAx, \"#ff5555\");"
"    plotFFT(fftAy, \"#55ff55\");"
"    plotFFT(fftAz, \"#5599ff\");"
"}"

"function togglePause() {"
"    paused = !paused;"
"    document.querySelector(\".controls button\").textContent = paused ? \"Continuar\" : \"Pausar\";"
"}"
"</script>"
"</body>"
"</html>";

#endif