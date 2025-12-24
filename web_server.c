/*
 * web_server.c
 *
 * Simple HTTP server for streaming FFT data (no external dependencies)
 * Serves static files and JSON API endpoints
 */

#include "web_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define close closesocket
    #define usleep(x) Sleep((x)/1000)
    typedef int socklen_t;
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
#endif

/*===========================================================================
 * Global Data
 *===========================================================================*/

static fft_data_t g_current_data = {0};
static bool g_data_available = false;

/*===========================================================================
 * Embedded HTML Content
 *===========================================================================*/

// We'll embed the HTML directly to avoid file I/O
// (In production, you'd serve from files)
static const char* HTML_CONTENT =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"  <meta charset='utf-8'>\n"
"  <meta name='viewport' content='width=device-width, initial-scale=1'>\n"
"  <title>FFT Analyzer v1.0.0 - DE10-Nano</title>\n"
"  <script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js'></script>\n"
"  <style>\n"
"    * { margin: 0; padding: 0; box-sizing: border-box; }\n"
"    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #1a1a1a; color: #fff; padding: 20px; }\n"
"    .container { max-width: 1400px; margin: 0 auto; }\n"
"    h1 { text-align: center; margin-bottom: 10px; color: #00d9ff; }\n"
"    .version { font-size: 0.5em; color: #888; font-weight: normal; }\n"
"    .subtitle { text-align: center; color: #888; margin-bottom: 10px; font-size: 14px; }\n"
"    .zulu-time { text-align: center; color: #00d9ff; font-size: 20px; font-weight: bold; margin-bottom: 30px; font-family: 'Courier New', monospace; }\n"
"    \n"
"    /* Status Bar */\n"
"    .status-bar { background: linear-gradient(135deg, #2a2a2a 0%, #1f1f1f 100%); padding: 20px; border-radius: 12px; margin-bottom: 30px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); display: flex; justify-content: space-around; align-items: center; flex-wrap: wrap; border: 1px solid #333; }\n"
"    .status-item { text-align: center; padding: 10px 20px; }\n"
"    .status-label { font-size: 11px; color: #888; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 5px; }\n"
"    .status-value { font-size: 20px; font-weight: bold; color: #00d9ff; }\n"
"    .status-value.running { color: #51cf66; }\n"
"    .status-value.paused { color: #ff9800; }\n"
"    \n"
"    /* Charts */\n"
"    .chart-container { background: #2a2a2a; padding: 20px; border-radius: 12px; margin-bottom: 30px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }\n"
"    canvas { max-height: 400px; }\n"
"    \n"
"    /* Controls */\n"
"    .controls { text-align: center; margin-top: 30px; }\n"
"    .control-group { margin-bottom: 20px; }\n"
"    .btn { background: #00d9ff; color: #000; border: none; padding: 12px 24px; border-radius: 6px; cursor: pointer; font-weight: bold; margin: 5px; font-size: 14px; transition: all 0.3s ease; }\n"
"    .btn:hover { background: #00a8cc; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,217,255,0.3); }\n"
"    .btn:disabled { opacity: 0.4; cursor: not-allowed; transform: none; }\n"
"    .btn-secondary { background: #444; color: #fff; }\n"
"    .btn-secondary:hover { background: #555; }\n"
"    \n"
"    /* Record Button */\n"
"    .record-btn { background: #4CAF50; color: white; padding: 12px 24px; border: none; border-radius: 6px; cursor: pointer; font-weight: bold; font-size: 14px; display: inline-flex; align-items: center; gap: 8px; transition: all 0.3s ease; }\n"
"    .record-btn:hover { background: #45a049; transform: translateY(-2px); box-shadow: 0 4px 8px rgba(76,175,80,0.3); }\n"
"    .record-btn.recording { background: #f44336; }\n"
"    .record-btn.recording:hover { background: #da190b; }\n"
"    \n"
"    /* Advanced Settings Collapsible */\n"
"    .advanced-settings { background: #2a2a2a; border-radius: 8px; margin: 20px auto; max-width: 800px; overflow: hidden; border: 1px solid #333; }\n"
"    .advanced-header { padding: 15px 20px; cursor: pointer; display: flex; justify-content: space-between; align-items: center; user-select: none; transition: background 0.3s; }\n"
"    .advanced-header:hover { background: #333; }\n"
"    .advanced-header h3 { margin: 0; color: #00d9ff; font-size: 16px; }\n"
"    .toggle-icon { transition: transform 0.3s; color: #00d9ff; font-weight: bold; font-size: 18px; }\n"
"    .toggle-icon.open { transform: rotate(180deg); }\n"
"    .advanced-content { max-height: 0; overflow: hidden; transition: max-height 0.3s ease-out; }\n"
"    .advanced-content.open { max-height: 500px; }\n"
"    .advanced-body { padding: 20px; border-top: 1px solid #333; }\n"
"    .setting-row { margin-bottom: 15px; display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }\n"
"    .setting-row label { color: #ccc; }\n"
"    .setting-row input[type='text'], .setting-row input[type='number'] { background: #1a1a1a; border: 1px solid #444; color: #fff; padding: 8px 12px; border-radius: 4px; }\n"
"    .setting-row input[type='checkbox'] { width: 18px; height: 18px; cursor: pointer; }\n"
"    \n"
"    /* Toast Notifications */\n"
"    .toast-container { position: fixed; top: 20px; right: 20px; z-index: 9999; max-width: 350px; }\n"
"    .toast { background: #333; color: #fff; padding: 16px 20px; border-radius: 8px; margin-bottom: 10px; box-shadow: 0 4px 12px rgba(0,0,0,0.5); display: flex; align-items: center; gap: 12px; animation: slideIn 0.3s ease-out; border-left: 4px solid #00d9ff; }\n"
"    .toast.success { border-left-color: #4CAF50; }\n"
"    .toast.error { border-left-color: #f44336; }\n"
"    .toast.warning { border-left-color: #ff9800; }\n"
"    .toast.info { border-left-color: #2196F3; }\n"
"    .toast-icon { font-size: 20px; }\n"
"    .toast-message { flex: 1; font-size: 14px; }\n"
"    @keyframes slideIn { from { transform: translateX(400px); opacity: 0; } to { transform: translateX(0); opacity: 1; } }\n"
"    @keyframes slideOut { from { transform: translateX(0); opacity: 1; } to { transform: translateX(400px); opacity: 0; } }\n"
"    \n"
"    /* Recording Status */\n"
"    .recording-status { display: none; background: #4CAF50; color: white; padding: 10px 20px; border-radius: 6px; margin: 10px auto; max-width: 600px; font-size: 14px; text-align: center; }\n"
"    .recording-status.active { display: block; }\n"
"    \n"
"    .error { background: #ff3333; color: white; padding: 15px; border-radius: 8px; margin: 20px 0; display: none; }\n"
"    .footer { text-align: center; margin-top: 40px; color: #666; font-size: 12px; }\n"
"    \n"
"    select { background: #2a2a2a; color: #fff; border: 1px solid #444; padding: 10px 15px; border-radius: 6px; cursor: pointer; font-size: 14px; }\n"
"    select:disabled { opacity: 0.4; cursor: not-allowed; }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <div class='toast-container' id='toastContainer'></div>\n"
"  \n"
"  <div class='container'>\n"
"    <h1>FFT Signal Analyzer <span class='version'>v1.0.0</span></h1>\n"
"    <div class='subtitle'>Real-Time Spectrum Display</div>\n"
"    <div class='zulu-time' id='zuluTime'>--:--:-- Z</div>\n"
"    \n"
"    <div class='error' id='error'>Connection lost. Retrying...</div>\n"
"    \n"
"    <!-- Consolidated Status Bar -->\n"
"    <div class='status-bar'>\n"
"      <div class='status-item'>\n"
"        <div class='status-label'>Mode</div>\n"
"        <div class='status-value' id='mode'>--</div>\n"
"      </div>\n"
"      <div class='status-item'>\n"
"        <div class='status-label'>Status</div>\n"
"        <div class='status-value' id='status'>--</div>\n"
"      </div>\n"
"      <div class='status-item'>\n"
"        <div class='status-label'>FFT Size</div>\n"
"        <div class='status-value' id='fftSize'>--</div>\n"
"      </div>\n"
"      <div class='status-item'>\n"
"        <div class='status-label'>Sample Rate</div>\n"
"        <div class='status-value' id='sampleRate'>--</div>\n"
"      </div>\n"
"      <div class='status-item'>\n"
"        <div class='status-label'>Update Rate</div>\n"
"        <div class='status-value' id='updateRate'>--</div>\n"
"      </div>\n"
"    </div>\n"
"    \n"
"    <!-- Charts -->\n"
"    <div style='display:flex; gap:20px; margin-bottom:30px; flex-wrap: wrap;'>\n"
"      <div class='chart-container' style='flex:1; min-width: 300px;'>\n"
"        <canvas id='psdChart'></canvas>\n"
"      </div>\n"
"      <div class='chart-container' style='flex:1; min-width: 300px;'>\n"
"        <canvas id='spectrogramCanvas' width='800' height='400' style='width:100%; height:400px; background:#000;'></canvas>\n"
"      </div>\n"
"    </div>\n"
"    \n"
"    <!-- Main Controls -->\n"
"    <div class='controls'>\n"
"      <div class='control-group'>\n"
"        <label for='modeSelect' style='color:#fff; margin-right:10px; font-size:14px;'>Waveform Mode:</label>\n"
"        <select id='modeSelect' onchange='changeMode()' style='margin-right:15px;'>\n"
"          <option value='0'>0: Silence</option>\n"
"          <option value='1'>1: 440 Hz Sine</option>\n"
"          <option value='2'>2: 1000 Hz Sine</option>\n"
"          <option value='3'>3: 2000 Hz Sine</option>\n"
"          <option value='4'>4: Mixed Tones</option>\n"
"          <option value='5'>5: Frequency Sweep</option>\n"
"          <option value='6'>6: White Noise</option>\n"
"          <option value='7'>7: Impulse Train</option>\n"
"          <option value='8'>8: LFM Chirp</option>\n"
"          <option value='9'>9: Sinc Function</option>\n"
"          <option value='10'>10: IQ LFM Chirp</option>\n"
"          <option value='11'>11: Signal + Noise</option>\n"
"        </select>\n"
"        <button class='btn btn-secondary' onclick='togglePause()'>Pause/Resume</button>\n"
"        <button class='btn btn-secondary' onclick='resetView()'>Reset View</button>\n"
"      </div>\n"
"      \n"
"      <!-- Recording Controls -->\n"
"      <div class='control-group'>\n"
"        <label for='formatSelect' style='color:#fff; margin-right:10px; font-size:14px;'>Recording Format:</label>\n"
"        <select id='formatSelect' onchange='selectFormat()' style='margin-right:15px;'>\n"
"          <option value='binary'>Binary (.bin)</option>\n"
"          <option value='csv'>CSV (.csv)</option>\n"
"          <option value='hdf5'>HDF5 (.h5)</option>\n"
"        </select>\n"
"        <button class='record-btn' id='recordBtn' onclick='toggleRecording()'>\n"
"          <span id='recordIcon'>&#9679;</span>\n"
"          <span id='recordText'>Record</span>\n"
"        </button>\n"
"      </div>\n"
"      \n"
"      <div class='recording-status' id='recordingStatus'>\n"
"        Recording: <span id='recordingFile'>--</span> (<span id='recordingFormat'>--</span>)\n"
"      </div>\n"
"      \n"
"      <!-- Advanced Settings Collapsible -->\n"
"      <div class='advanced-settings'>\n"
"        <div class='advanced-header' onclick='toggleAdvanced()'>\n"
"          <h3>Advanced Settings</h3>\n"
"          <span class='toggle-icon' id='toggleIcon'>&#9660;</span>\n"
"        </div>\n"
"        <div class='advanced-content' id='advancedContent'>\n"
"          <div class='advanced-body'>\n"
"            <div class='setting-row'>\n"
"              <input type='checkbox' id='autoRecordCheck' onchange='toggleAutoRecord()'>\n"
"              <label for='autoRecordCheck'>Auto-Record on SNR Threshold</label>\n"
"              <label style='margin-left:20px;'>Threshold (dB):</label>\n"
"              <input type='number' id='snrThreshold' value='10' min='-40' max='60' step='1' style='width:80px;' onchange='updateAutoRecordThreshold()'>\n"
"              <span id='autoRecordStatus' style='margin-left:15px; color:#888; font-size:12px;'>Disabled</span>\n"
"            </div>\n"
"            <div class='setting-row'>\n"
"              <label>Log Directory:</label>\n"
"              <input type='text' id='logDirectory' value='.' style='flex:1; max-width:300px;' placeholder='e.g., ./logs or C:/data'>\n"
"              <button class='btn' onclick='setLogDirectory()' style='background:#ff9800; padding:8px 16px;'>Set Directory</button>\n"
"              <span id='dirStatus' style='color:#888; font-size:12px;'>Current: .</span>\n"
"            </div>\n"
"          </div>\n"
"        </div>\n"
"      </div>\n"
"    </div>\n"
"    \n"
"    <div class='footer'>\n"
"      FFT Signal Analyzer v1.0.0 | DE10-Nano FPGA Board | Real-Time Spectrum Analysis\n"
"    </div>\n"
"  </div>\n"
"  \n"
"  <script>\n"
"    // Toast Notification System\n"
"    function showToast(message, type = 'info') {\n"
"      const container = document.getElementById('toastContainer');\n"
"      const toast = document.createElement('div');\n"
"      toast.className = 'toast ' + type;\n"
"      \n"
"      const icons = { success: '✓', error: '✕', warning: '⚠', info: 'ℹ' };\n"
"      const icon = icons[type] || 'ℹ';\n"
"      \n"
"      toast.innerHTML = `<span class='toast-icon'>${icon}</span><span class='toast-message'>${message}</span>`;\n"
"      container.appendChild(toast);\n"
"      \n"
"      setTimeout(() => {\n"
"        toast.style.animation = 'slideOut 0.3s ease-out';\n"
"        setTimeout(() => container.removeChild(toast), 300);\n"
"      }, 3000);\n"
"    }\n"
"    \n"
"    // Toggle Advanced Settings\n"
"    function toggleAdvanced() {\n"
"      const content = document.getElementById('advancedContent');\n"
"      const icon = document.getElementById('toggleIcon');\n"
"      content.classList.toggle('open');\n"
"      icon.classList.toggle('open');\n"
"    }\n"
"    \n"
"    // Recording state\n"
"    let isRecording = false;\n"
"    let selectedFormat = 'binary';\n"
"    \n"
"    function selectFormat() {\n"
"      const select = document.getElementById('formatSelect');\n"
"      selectedFormat = select.value;\n"
"      const formatNames = { binary: 'Binary', csv: 'CSV', hdf5: 'HDF5' };\n"
"      showToast(`Recording format: ${formatNames[selectedFormat]}`, 'info');\n"
"    }\n"
"    \n"
"    function toggleRecording() {\n"
"      const endpoint = isRecording ? '/api/log/stop' : `/api/log/start?format=${selectedFormat}`;\n"
"      fetch(endpoint, { method: 'POST' })\n"
"        .then(r => r.json())\n"
"        .then(data => {\n"
"          isRecording = data.logging;\n"
"          const btn = document.getElementById('recordBtn');\n"
"          const icon = document.getElementById('recordIcon');\n"
"          const text = document.getElementById('recordText');\n"
"          const status = document.getElementById('recordingStatus');\n"
"          const fileSpan = document.getElementById('recordingFile');\n"
"          const formatSpan = document.getElementById('recordingFormat');\n"
"          \n"
"          if (isRecording) {\n"
"            btn.classList.add('recording');\n"
"            icon.textContent = '■';\n"
"            text.textContent = 'Stop Recording';\n"
"            status.classList.add('active');\n"
"            fileSpan.textContent = data.filepath || 'recording...';\n"
"            formatSpan.textContent = (data.format || selectedFormat).toUpperCase();\n"
"            showToast(`Recording started (${data.format || selectedFormat})`, 'success');\n"
"          } else {\n"
"            btn.classList.remove('recording');\n"
"            icon.textContent = '●';\n"
"            text.textContent = 'Record';\n"
"            status.classList.remove('active');\n"
"            showToast('Recording stopped', 'info');\n"
"          }\n"
"        })\n"
"        .catch(error => {\n"
"          console.error('Error toggling recording:', error);\n"
"          showToast('Error: Failed to toggle recording', 'error');\n"
"        });\n"
"    }\n"
"    \n"
"    // Chart.js setup\n"
"    const psdCtx = document.getElementById('psdChart').getContext('2d');\n"
"    const psdChart = new Chart(psdCtx, {\n"
"      type: 'line',\n"
"      data: {\n"
"        labels: [],\n"
"        datasets: [{\n"
"          label: 'PSD (dB/Hz)',\n"
"          data: [],\n"
"          borderColor: '#ffaa00',\n"
"          backgroundColor: 'rgba(255, 170, 0, 0.1)',\n"
"          borderWidth: 2,\n"
"          fill: true,\n"
"          tension: 0.4\n"
"        }]\n"
"      },\n"
"      options: {\n"
"        responsive: true,\n"
"        maintainAspectRatio: true,\n"
"        plugins: {\n"
"          title: { display: true, text: 'Power Spectral Density (PSD)', color: '#fff', font: { size: 16 } },\n"
"          legend: { labels: { color: '#fff' } }\n"
"        },\n"
"        scales: {\n"
"          x: { title: { display: true, text: 'Frequency (Hz)', color: '#fff' }, ticks: { color: '#888' }, grid: { color: '#333' } },\n"
"          y: { title: { display: true, text: 'PSD (dB/Hz)', color: '#fff' }, ticks: { color: '#888' }, grid: { color: '#333' } }\n"
"        },\n"
"        animation: { duration: 0 }\n"
"      }\n"
"    });\n"
"    \n"
"    // Spectrogram setup\n"
"    const spectrogramCanvas = document.getElementById('spectrogramCanvas');\n"
"    const spectrogramCtx = spectrogramCanvas.getContext('2d');\n"
"    const spectrogramHistory = [];  // Rolling buffer of PSD frames\n"
"    const maxHistory = 100;          // Keep last 100 frames (10 seconds at 10 Hz)\n"
"    \n"
"    // Hot colormap (black -> red -> yellow -> white)\n"
"    function getHotColor(value) {\n"
"      // Input: value in dB (typically -80 to 0)\n"
"      // Normalize to 0-1 range\n"
"      const normalized = Math.max(0, Math.min(1, (value + 80) / 80));\n"
"      \n"
"      let r, g, b;\n"
"      if (normalized < 0.33) {\n"
"        // Black -> Red\n"
"        const t = normalized / 0.33;\n"
"        r = Math.floor(255 * t);\n"
"        g = 0;\n"
"        b = 0;\n"
"      } else if (normalized < 0.66) {\n"
"        // Red -> Yellow\n"
"        const t = (normalized - 0.33) / 0.33;\n"
"        r = 255;\n"
"        g = Math.floor(255 * t);\n"
"        b = 0;\n"
"      } else {\n"
"        // Yellow -> White\n"
"        const t = (normalized - 0.66) / 0.34;\n"
"        r = 255;\n"
"        g = 255;\n"
"        b = Math.floor(255 * t);\n"
"      }\n"
"      return `rgb(${r},${g},${b})`;\n"
"    }\n"
"    \n"
"    function renderSpectrogram() {\n"
"      const width = spectrogramCanvas.width;\n"
"      const height = spectrogramCanvas.height;\n"
"      const numFrames = spectrogramHistory.length;\n"
"      \n"
"      if (numFrames === 0 || !spectrogramHistory[0]) return;\n"
"      \n"
"      // Clear canvas\n"
"      spectrogramCtx.fillStyle = '#000';\n"
"      spectrogramCtx.fillRect(0, 0, width, height);\n"
"      \n"
"      const frameWidth = width / maxHistory;\n"
"      const numBins = spectrogramHistory[0].length;\n"
"      if (numBins === 0) return;\n"
"      const binHeight = height / numBins;\n"
"      \n"
"      // Draw each frame\n"
"      for (let f = 0; f < numFrames; f++) {\n"
"        const frame = spectrogramHistory[f];\n"
"        if (!frame || frame.length === 0) continue;\n"
"        const x = f * frameWidth;\n"
"        \n"
"        // Draw each frequency bin (flip Y-axis so low freq at bottom)\n"
"        for (let bin = 0; bin < numBins; bin++) {\n"
"          const y = height - (bin + 1) * binHeight;  // Flip Y\n"
"          const value = frame[bin] || 0;\n"
"          \n"
"          spectrogramCtx.fillStyle = getHotColor(value);\n"
"          spectrogramCtx.fillRect(x, y, Math.ceil(frameWidth) + 1, Math.ceil(binHeight) + 1);\n"
"        }\n"
"      }\n"
"      \n"
"      // Draw frequency axis labels\n"
"      spectrogramCtx.fillStyle = '#fff';\n"
"      spectrogramCtx.font = '12px monospace';\n"
"      spectrogramCtx.textAlign = 'right';\n"
"      const freqStep = 1000;  // Label every 1000 Hz\n"
"      const maxFreq = 4000;   // Max frequency\n"
"      for (let freq = 0; freq <= maxFreq; freq += freqStep) {\n"
"        const y = height - (freq / maxFreq) * height;\n"
"        spectrogramCtx.fillText(freq + ' Hz', width - 5, y + 4);\n"
"      }\n"
"      \n"
"      // Draw title\n"
"      spectrogramCtx.textAlign = 'left';\n"
"      spectrogramCtx.fillText('Spectrogram (Time vs Frequency)', 10, 20);\n"
"      spectrogramCtx.fillText('← Older | Newer →', 10, height - 10);\n"
"    }\n"
"    \n"
"    let lastUpdate = 0;\n"
"    let updateCount = 0;\n"
"    \n"
"    // Update Zulu time display\n"
"    function updateZuluTime() {\n"
"      const now = new Date();\n"
"      const hours = String(now.getUTCHours()).padStart(2, '0');\n"
"      const minutes = String(now.getUTCMinutes()).padStart(2, '0');\n"
"      const seconds = String(now.getUTCSeconds()).padStart(2, '0');\n"
"      document.getElementById('zuluTime').textContent = `${hours}:${minutes}:${seconds} Z`;\n"
"    }\n"
"    \n"
"    // Update Zulu time every second\n"
"    setInterval(updateZuluTime, 1000);\n"
"    updateZuluTime(); // Initial update\n"
"    \n"
"    // Fetch FFT data and update charts\n"
"    async function updateData() {\n"
"      try {\n"
"        const response = await fetch('/api/fft');\n"
"        if (!response.ok) throw new Error('Network error');\n"
"        \n"
"        const data = await response.json();\n"
"        document.getElementById('error').style.display = 'none';\n"
"        \n"
"        // Update status\n"
"        document.getElementById('mode').textContent = data.mode || 'Unknown';\n"
"        const statusEl = document.getElementById('status');\n"
"        statusEl.textContent = data.paused ? 'PAUSED' : 'RUNNING';\n"
"        statusEl.className = 'status-value ' + (data.paused ? 'paused' : 'running');\n"
"        document.getElementById('fftSize').textContent = data.fft_size;\n"
"        document.getElementById('sampleRate').textContent = data.sample_rate + ' Hz';\n"
"        \n"
"        // Calculate update rate\n"
"        const now = Date.now();\n"
"        if (lastUpdate > 0) {\n"
"          const fps = 1000 / (now - lastUpdate);\n"
"          document.getElementById('updateRate').textContent = fps.toFixed(1) + ' Hz';\n"
"        }\n"
"        lastUpdate = now;\n"
"        \n"
"        // Update PSD chart\n"
"        const freqs = data.frequencies || [];\n"
"        const psd = data.psd || [];\n"
"        psdChart.data.labels = freqs;\n"
"        psdChart.data.datasets[0].data = psd;\n"
"        psdChart.update();\n"
"        \n"
"        // Update spectrogram history\n"
"        if (psd.length > 0) {\n"
"          spectrogramHistory.push([...psd]);  // Clone the array\n"
"          if (spectrogramHistory.length > maxHistory) {\n"
"            spectrogramHistory.shift();  // Remove oldest frame\n"
"          }\n"
"          renderSpectrogram();\n"
"        }\n"
"        \n"


"        // Update mode selector to match current mode\n"
"        const modeSelect = document.getElementById('modeSelect');\n"
"        const currentModeIndex = Array.from(modeSelect.options).findIndex(opt => \n"
"          data.mode && opt.text.includes(data.mode));\n"
"        if (currentModeIndex >= 0 && modeSelect.selectedIndex !== currentModeIndex) {\n"
"          modeSelect.selectedIndex = currentModeIndex;\n"
"        }\n"
"      } catch (error) {\n"
"        console.error('Error fetching data:', error);\n"
"        document.getElementById('error').style.display = 'block';\n"
"      }\n"
"    }\n"
"    \n"
"    // Poll for updates\n"
"    setInterval(updateData, 100); // 10 Hz\n"
"    updateData(); // Initial fetch\n"
"    \n"
"    function changeMode() {\n"
"      const mode = document.getElementById('modeSelect').value;\n"
"      fetch('/api/mode?value=' + mode, { method: 'POST' })\n"
"        .then(response => response.json())\n"
"        .then(data => {\n"
"          console.log('Mode changed to:', data.mode);\n"
"          showToast('Waveform mode changed', 'success');\n"
"        })\n"
"        .catch(error => {\n"
"          console.error('Error changing mode:', error);\n"
"          showToast('Error changing mode', 'error');\n"
"        });\n"
"    }\n"
"    \n"
"    function togglePause() {\n"
"      fetch('/api/pause', { method: 'POST' })\n"
"        .then(response => response.json())\n"
"        .then(data => {\n"
"          console.log('Pause state:', data.paused);\n"
"          showToast(data.paused ? 'Paused' : 'Resumed', 'info');\n"
"        })\n"
"        .catch(error => {\n"
"          console.error('Error toggling pause:', error);\n"
"          showToast('Error toggling pause', 'error');\n"
"        });\n"
"    }\n"
"    \n"
"    function resetView() {\n"
"      spectrogramHistory.length = 0;\n"
"      console.log('View reset');\n"
"      showToast('Spectrogram view reset', 'info');\n"
"    }\n"
"    \n"
"    function toggleAutoRecord() {\n"
"      const enabled = document.getElementById('autoRecordCheck').checked;\n"
"      const threshold = document.getElementById('snrThreshold').value;\n"
"      fetch(`/api/auto-record?enabled=${enabled}&threshold=${threshold}`, { method: 'POST' })\n"
"        .then(r => r.json())\n"
"        .then(data => {\n"
"          const status = document.getElementById('autoRecordStatus');\n"
"          if (data.enabled) {\n"
"            status.textContent = `Enabled (≥${data.threshold} dB)`;\n"
"            status.style.color = '#4CAF50';\n"
"            showToast(`Auto-record enabled (threshold: ${data.threshold} dB)`, 'success');\n"
"          } else {\n"
"            status.textContent = 'Disabled';\n"
"            status.style.color = '#888';\n"
"            showToast('Auto-record disabled', 'info');\n"
"          }\n"
"        })\n"
"        .catch(error => {\n"
"          console.error('Error toggling auto-record:', error);\n"
"          showToast('Error toggling auto-record', 'error');\n"
"        });\n"
"    }\n"
"    \n"
"    function updateAutoRecordThreshold() {\n"
"      if (document.getElementById('autoRecordCheck').checked) {\n"
"        toggleAutoRecord();\n"
"      }\n"
"    }\n"
"    \n"
"    function setLogDirectory() {\n"
"      const directory = document.getElementById('logDirectory').value;\n"
"      fetch(`/api/log/directory?directory=${encodeURIComponent(directory)}`, { method: 'POST' })\n"
"        .then(r => r.json())\n"
"        .then(data => {\n"
"          const status = document.getElementById('dirStatus');\n"
"          if (data.status === 'ok') {\n"
"            status.textContent = `Current: ${data.directory}`;\n"
"            status.style.color = '#4CAF50';\n"
"            showToast('Log directory updated', 'success');\n"
"          } else {\n"
"            status.textContent = 'Error: ' + data.message;\n"
"            status.style.color = '#f44336';\n"
"            showToast('Error: ' + data.message, 'error');\n"
"          }\n"
"        })\n"
"        .catch(error => {\n"
"          console.error('Error setting directory:', error);\n"
"          showToast('Error setting directory', 'error');\n"
"        });\n"
"    }\n"
"    \n"
"    // Load current directory on page load\n"
"    window.addEventListener('load', function() {\n"
"      fetch('/api/log/directory')\n"
"        .then(r => r.json())\n"
"        .then(data => {\n"
"          if (data.status === 'ok') {\n"
"            document.getElementById('logDirectory').value = data.directory;\n"
"            document.getElementById('dirStatus').textContent = `Current: ${data.directory}`;\n"
"          }\n"
"        });\n"
"    });\n"
"  </script>\n"
"</body>\n"
"</html>\n";

/*===========================================================================
 * Callback Function Pointers
 *===========================================================================*/

static void (*g_mode_callback)(int mode) = NULL;
static void (*g_pause_callback)(void) = NULL;
static bool (*g_log_callback)(void) = NULL;
static bool (*g_log_status_callback)(char* filepath, size_t max_len) = NULL;
static bool (*g_log_start_callback)(const char* format) = NULL;
static void (*g_log_stop_callback)(void) = NULL;
static const char* (*g_log_format_callback)(void) = NULL;
static void (*g_auto_record_callback)(bool enabled, float threshold) = NULL;
static void (*g_log_directory_callback)(const char* directory) = NULL;
static const char* (*g_get_log_directory_callback)(void) = NULL;

void web_server_set_mode_callback(void (*callback)(int mode)) {
    g_mode_callback = callback;
}

void web_server_set_pause_callback(void (*callback)(void)) {
    g_pause_callback = callback;
}

void web_server_set_log_callback(bool (*callback)(void)) {
    g_log_callback = callback;
}

void web_server_set_log_status_callback(bool (*callback)(char* filepath, size_t max_len)) {
    g_log_status_callback = callback;
}

void web_server_set_log_start_callback(bool (*callback)(const char* format)) {
    g_log_start_callback = callback;
}

void web_server_set_log_stop_callback(void (*callback)(void)) {
    g_log_stop_callback = callback;
}

void web_server_set_log_format_callback(const char* (*callback)(void)) {
    g_log_format_callback = callback;
}

void web_server_set_auto_record_callback(void (*callback)(bool enabled, float threshold)) {
    g_auto_record_callback = callback;
}

void web_server_set_log_directory_callback(void (*callback)(const char* directory)) {
    g_log_directory_callback = callback;
}

void web_server_set_get_log_directory_callback(const char* (*callback)(void)) {
    g_get_log_directory_callback = callback;
}

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

uint64_t get_timestamp_ms(void) {
#ifdef _WIN32
    return (uint64_t)(clock() * 1000 / CLOCKS_PER_SEC);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
#endif
}

static void send_response(int client_fd, const char* status, const char* content_type,
                         const char* body, int body_len) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        status, content_type, body_len);

    send(client_fd, header, header_len, 0);
    if (body && body_len > 0) {
        send(client_fd, body, body_len, 0);
    }
}

/*===========================================================================
 * Web Server Implementation
 *===========================================================================*/

int web_server_init(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return -1;
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

#ifdef SO_REUSEPORT
    // SO_REUSEPORT is not available on all systems
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    // Make socket non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(server_fd, FIONBIO, &mode);
#else
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
#endif

    // Bind socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }

    // Listen
    if (listen(server_fd, WEB_SERVER_MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    printf("[OK] Web server listening on port %d\n", port);
    printf("     Access at: http://<board-ip>:%d\n", port);

    return server_fd;
}

void web_server_update_data(const fft_data_t* data) {
    if (data) {
        g_current_data = *data;
        g_data_available = true;
    }
}

int web_server_handle_requests(int server_fd) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int client_fd;
    int requests_handled = 0;

    // Accept new connection (non-blocking)
    while ((client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen)) >= 0) {
        char buffer[WEB_SERVER_BUFFER_SIZE];
        int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';

            // Parse HTTP request
            char method[16], path[256];
            sscanf(buffer, "%s %s", method, path);

            // Route requests
            if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
                // Serve HTML page
                send_response(client_fd, "200 OK", "text/html",
                            HTML_CONTENT, strlen(HTML_CONTENT));
            }
            else if (strcmp(path, "/api/fft") == 0 && g_data_available) {
                // Serve FFT data as JSON
                char json[8192];
                int json_len = 0;

                json_len += snprintf(json + json_len, sizeof(json) - json_len,
                    "{\"fft_size\":%d,\"sample_rate\":%d,\"num_bands\":%d,"
                    "\"mode\":\"%s\",\"paused\":%s,\"web_control_active\":%s,\"led_pattern\":%d,\"timestamp\":%llu,",
                    g_current_data.fft_size, g_current_data.sample_rate,
                    g_current_data.num_bands, g_current_data.mode_name,
                    g_current_data.paused ? "true" : "false",
                    g_current_data.web_control_active ? "true" : "false",
                    g_current_data.led_pattern,
                    (unsigned long long)g_current_data.timestamp);

                // Add time-domain samples (downsampled)
                json_len += snprintf(json + json_len, sizeof(json) - json_len,
                    "\"time_domain\":[");
                for (int i = 0; i < g_current_data.fft_size; i += 4) { // Downsample by 4
                    json_len += snprintf(json + json_len, sizeof(json) - json_len,
                        "%.3f%s", g_current_data.time_domain[i], (i < g_current_data.fft_size - 4) ? "," : "");
                }
                json_len += snprintf(json + json_len, sizeof(json) - json_len, "],");

                // Add frequencies array
                json_len += snprintf(json + json_len, sizeof(json) - json_len,
                    "\"frequencies\":[");
                for (int i = 0; i < g_current_data.fft_size / 2; i += 4) { // Downsample for web
                    float freq = (float)i * g_current_data.sample_rate / g_current_data.fft_size;
                    json_len += snprintf(json + json_len, sizeof(json) - json_len,
                        "%.1f%s", freq, (i < g_current_data.fft_size / 2 - 4) ? "," : "");
                }
                json_len += snprintf(json + json_len, sizeof(json) - json_len, "],");

                // Add magnitudes array (in dB)
                json_len += snprintf(json + json_len, sizeof(json) - json_len,
                    "\"magnitudes\":[");
                for (int i = 0; i < g_current_data.fft_size / 2; i += 4) { // Downsample
                    float db = 20.0f * log10f(g_current_data.magnitude[i] + 1e-6f);
                    json_len += snprintf(json + json_len, sizeof(json) - json_len,
                        "%.1f%s", db, (i < g_current_data.fft_size / 2 - 4) ? "," : "");
                }
                json_len += snprintf(json + json_len, sizeof(json) - json_len, "],");

                // Add PSD array (in dB)
                // PSD uses Welch's method with 256-pt segments, so 128 bins
                json_len += snprintf(json + json_len, sizeof(json) - json_len,
                    "\"psd\":[");
                for (int i = 0; i < g_current_data.psd_size; i += 2) { // Downsample by 2 (128 -> 64 points)
                    json_len += snprintf(json + json_len, sizeof(json) - json_len,
                        "%.1f%s", g_current_data.psd[i], (i < g_current_data.psd_size - 2) ? "," : "");
                }
                json_len += snprintf(json + json_len, sizeof(json) - json_len, "],");

                // Add band energies (in dB)
                json_len += snprintf(json + json_len, sizeof(json) - json_len,
                    "\"band_energies\":[");
                for (int i = 0; i < g_current_data.num_bands; i++) {
                    float db = 20.0f * log10f(g_current_data.band_energies[i] + 1e-6f);
                    json_len += snprintf(json + json_len, sizeof(json) - json_len,
                        "%.1f%s", db, (i < g_current_data.num_bands - 1) ? "," : "");
                }
                json_len += snprintf(json + json_len, sizeof(json) - json_len, "]}");

                send_response(client_fd, "200 OK", "application/json", json, json_len);
            }
            else if (strcmp(path, "/api/pause") == 0) {
                // Handle pause toggle
                if (g_pause_callback) {
                    g_pause_callback();
                }
                char response[64];
                int len = snprintf(response, sizeof(response),
                    "{\"status\":\"ok\",\"paused\":%s}",
                    g_current_data.paused ? "true" : "false");
                send_response(client_fd, "200 OK", "application/json", response, len);
            }
            else if (strncmp(path, "/api/mode", 9) == 0) {
                // Handle mode change - parse query parameter
                int mode = -1;
                char* query = strchr(path, '?');
                if (query) {
                    char* value_param = strstr(query, "value=");
                    if (value_param) {
                        mode = atoi(value_param + 6);
                    }
                }

                if (mode >= 0 && mode <= 15 && g_mode_callback) {
                    g_mode_callback(mode);
                    char response[64];
                    int len = snprintf(response, sizeof(response),
                        "{\"status\":\"ok\",\"mode\":%d}", mode);
                    send_response(client_fd, "200 OK", "application/json", response, len);
                } else {
                    const char* msg = "{\"status\":\"error\",\"message\":\"Invalid mode\"}";
                    send_response(client_fd, "400 Bad Request", "application/json", msg, strlen(msg));
                }
            }
            else if (strncmp(path, "/api/log/start", 14) == 0) {
                // Handle logging start with format
                if (g_log_start_callback && g_log_status_callback && g_log_format_callback) {
                    // Parse format parameter
                    char format[16] = "binary";  // default
                    char* query = strchr(path, '?');
                    if (query) {
                        char* format_param = strstr(query, "format=");
                        if (format_param) {
                            char* format_value = format_param + 7;
                            char* end = strchr(format_value, '&');
                            int len = end ? (int)(end - format_value) : (int)strlen(format_value);
                            if (len > 0 && len < 16) {
                                strncpy(format, format_value, len);
                                format[len] = '\0';
                            }
                        }
                    }

                    bool success = g_log_start_callback(format);
                    char filepath[512] = {0};
                    g_log_status_callback(filepath, sizeof(filepath));
                    const char* current_format = g_log_format_callback();

                    char response[1024];
                    int len = snprintf(response, sizeof(response),
                        "{\"status\":\"ok\",\"logging\":%s,\"format\":\"%s\",\"filepath\":\"%s\"}",
                        success ? "true" : "false",
                        current_format ? current_format : "",
                        filepath[0] ? filepath : "");
                    send_response(client_fd, "200 OK", "application/json", response, len);
                } else {
                    const char* msg = "{\"status\":\"error\",\"message\":\"Logging not configured\"}";
                    send_response(client_fd, "500 Internal Server Error", "application/json", msg, strlen(msg));
                }
            }
            else if (strcmp(path, "/api/log/stop") == 0) {
                // Handle logging stop
                if (g_log_stop_callback && g_log_status_callback) {
                    g_log_stop_callback();
                    char filepath[512] = {0};
                    g_log_status_callback(filepath, sizeof(filepath));

                    char response[1024];
                    int len = snprintf(response, sizeof(response),
                        "{\"status\":\"ok\",\"logging\":false,\"format\":\"\",\"filepath\":\"\"}");
                    send_response(client_fd, "200 OK", "application/json", response, len);
                } else {
                    const char* msg = "{\"status\":\"error\",\"message\":\"Logging not configured\"}";
                    send_response(client_fd, "500 Internal Server Error", "application/json", msg, strlen(msg));
                }
            }
            else if (strcmp(path, "/api/log/toggle") == 0) {
                // Legacy endpoint - kept for backwards compatibility
                if (g_log_callback && g_log_status_callback) {
                    bool is_logging = g_log_callback();
                    char filepath[512] = {0};
                    g_log_status_callback(filepath, sizeof(filepath));

                    char response[1024];
                    int len = snprintf(response, sizeof(response),
                        "{\"status\":\"ok\",\"logging\":%s,\"filepath\":\"%s\"}",
                        is_logging ? "true" : "false",
                        filepath[0] ? filepath : "");
                    send_response(client_fd, "200 OK", "application/json", response, len);
                } else {
                    const char* msg = "{\"status\":\"error\",\"message\":\"Logging not configured\"}";
                    send_response(client_fd, "500 Internal Server Error", "application/json", msg, strlen(msg));
                }
            }
            else if (strncmp(path, "/api/auto-record", 16) == 0) {
                // Handle auto-record toggle
                if (g_auto_record_callback) {
                    bool enabled = false;
                    float threshold = 10.0f;

                    // Parse query parameters
                    char* query = strchr(path, '?');
                    if (query) {
                        char* enabled_param = strstr(query, "enabled=");
                        if (enabled_param) {
                            enabled = (strstr(enabled_param + 8, "true") != NULL);
                        }
                        char* threshold_param = strstr(query, "threshold=");
                        if (threshold_param) {
                            threshold = atof(threshold_param + 10);
                        }
                    }

                    g_auto_record_callback(enabled, threshold);

                    char response[256];
                    int len = snprintf(response, sizeof(response),
                        "{\"status\":\"ok\",\"enabled\":%s,\"threshold\":%.1f}",
                        enabled ? "true" : "false", threshold);
                    send_response(client_fd, "200 OK", "application/json", response, len);
                } else {
                    const char* msg = "{\"status\":\"error\",\"message\":\"Auto-record not configured\"}";
                    send_response(client_fd, "500 Internal Server Error", "application/json", msg, strlen(msg));
                }
            }
            else if (strncmp(path, "/api/log/directory", 18) == 0) {
                // Handle log directory configuration
                char* query = strchr(path, '?');
                if (query && g_log_directory_callback) {
                    // Set new directory (with query params)
                    query++;
                    char* dir_param = strstr(query, "directory=");
                    if (dir_param) {
                        char directory[256] = {0};
                        char* dir_value = dir_param + 10;
                        char* end = strchr(dir_value, '&');
                        int len = end ? (int)(end - dir_value) : (int)strlen(dir_value);
                        if (len > 0 && len < 256) {
                            strncpy(directory, dir_value, len);
                            directory[len] = '\0';

                            // URL decode (replace %20 with space, etc.)
                            for (int i = 0, j = 0; directory[i]; i++, j++) {
                                if (directory[i] == '%' && directory[i+1] && directory[i+2]) {
                                    char hex[3] = {directory[i+1], directory[i+2], '\0'};
                                    directory[j] = (char)strtol(hex, NULL, 16);
                                    i += 2;
                                } else if (directory[i] == '+') {
                                    directory[j] = ' ';
                                } else {
                                    directory[j] = directory[i];
                                }
                            }

                            g_log_directory_callback(directory);

                            char response[512];
                            int resp_len = snprintf(response, sizeof(response),
                                "{\"status\":\"ok\",\"directory\":\"%s\"}", directory);
                            send_response(client_fd, "200 OK", "application/json", response, resp_len);
                        } else {
                            const char* msg = "{\"status\":\"error\",\"message\":\"Invalid directory\"}";
                            send_response(client_fd, "400 Bad Request", "application/json", msg, strlen(msg));
                        }
                    } else {
                        const char* msg = "{\"status\":\"error\",\"message\":\"Missing directory parameter\"}";
                        send_response(client_fd, "400 Bad Request", "application/json", msg, strlen(msg));
                    }
                } else if (g_get_log_directory_callback) {
                    // Get current directory (no query params)
                    const char* directory = g_get_log_directory_callback();
                    char response[512];
                    int len = snprintf(response, sizeof(response),
                        "{\"status\":\"ok\",\"directory\":\"%s\"}", directory ? directory : ".");
                    send_response(client_fd, "200 OK", "application/json", response, len);
                } else {
                    const char* msg = "{\"status\":\"error\",\"message\":\"Directory configuration not available\"}";
                    send_response(client_fd, "500 Internal Server Error", "application/json", msg, strlen(msg));
                }
            }
            else {
                // 404 Not Found
                const char* msg = "404 Not Found";
                send_response(client_fd, "404 Not Found", "text/plain", msg, strlen(msg));
            }

            requests_handled++;
        }

        close(client_fd);
    }

    // EAGAIN/EWOULDBLOCK is expected for non-blocking accept
    if (errno != EAGAIN && errno != EWOULDBLOCK && requests_handled == 0) {
        // Real error
        return -1;
    }

    return requests_handled;
}

void web_server_cleanup(int server_fd) {
    if (server_fd >= 0) {
        close(server_fd);
        printf("[*] Web server closed\n");
    }
}
