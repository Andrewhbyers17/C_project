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
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

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
"  <title>FFT Analyzer - DE10-Nano</title>\n"
"  <script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js'></script>\n"
"  <style>\n"
"    * { margin: 0; padding: 0; box-sizing: border-box; }\n"
"    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #1a1a1a; color: #fff; padding: 20px; }\n"
"    .container { max-width: 1400px; margin: 0 auto; }\n"
"    h1 { text-align: center; margin-bottom: 10px; color: #00d9ff; }\n"
"    .subtitle { text-align: center; color: #888; margin-bottom: 30px; font-size: 14px; }\n"
"    .status { display: flex; justify-content: space-around; margin-bottom: 20px; flex-wrap: wrap; }\n"
"    .status-card { background: #2a2a2a; padding: 15px 25px; border-radius: 8px; margin: 5px; min-width: 150px; text-align: center; border: 2px solid #333; }\n"
"    .status-label { font-size: 12px; color: #888; text-transform: uppercase; }\n"
"    .status-value { font-size: 24px; font-weight: bold; color: #00d9ff; margin-top: 5px; }\n"
"    .chart-container { background: #2a2a2a; padding: 20px; border-radius: 12px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }\n"
"    canvas { max-height: 400px; }\n"
"    .led-display { display: flex; justify-content: center; gap: 10px; margin: 20px 0; }\n"
"    .led { width: 50px; height: 50px; border-radius: 50%; background: #333; border: 3px solid #555; transition: all 0.2s; }\n"
"    .led.active { background: #00ff00; box-shadow: 0 0 20px #00ff00; border-color: #00ff00; }\n"
"    .controls { text-align: center; margin-top: 20px; }\n"
"    .btn { background: #00d9ff; color: #000; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-weight: bold; margin: 5px; }\n"
"    .btn:hover { background: #00a8cc; }\n"
"    .error { background: #ff3333; color: white; padding: 15px; border-radius: 8px; margin: 20px 0; display: none; }\n"
"    .footer { text-align: center; margin-top: 40px; color: #666; font-size: 12px; }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <div class='container'>\n"
"    <h1>FFT Signal Analyzer</h1>\n"
"    <div class='subtitle'>DE10-Nano Real-Time Spectrum Display</div>\n"
"    \n"
"    <div class='error' id='error'>Connection lost. Retrying...</div>\n"
"    \n"
"    <div class='status'>\n"
"      <div class='status-card'>\n"
"        <div class='status-label'>Mode</div>\n"
"        <div class='status-value' id='mode'>--</div>\n"
"      </div>\n"
"      <div class='status-card'>\n"
"        <div class='status-label'>Status</div>\n"
"        <div class='status-value' id='status'>--</div>\n"
"      </div>\n"
"      <div class='status-card'>\n"
"        <div class='status-label'>FFT Size</div>\n"
"        <div class='status-value' id='fftSize'>--</div>\n"
"      </div>\n"
"      <div class='status-card'>\n"
"        <div class='status-label'>Sample Rate</div>\n"
"        <div class='status-value' id='sampleRate'>--</div>\n"
"      </div>\n"
"      <div class='status-card'>\n"
"        <div class='status-label'>Update Rate</div>\n"
"        <div class='status-value' id='updateRate'>--</div>\n"
"      </div>\n"
"    </div>\n"
"    \n"
"    <div class='chart-container'>\n"
"      <canvas id='timeDomainChart'></canvas>\n"
"    </div>\n"
"    \n"
"    <div class='chart-container'>\n"
"      <canvas id='psdChart'></canvas>\n"
"    </div>\n"
"    \n"
"    <div class='chart-container'>\n"
"      <canvas id='spectrogramCanvas' width='800' height='300' style='width:100%; height:300px; background:#000;'></canvas>\n"
"    </div>\n"
"    \n"
"    <div class='led-display'>\n"
"      <div class='led' id='led0' title='0-200 Hz'></div>\n"
"      <div class='led' id='led1' title='200-400 Hz'></div>\n"
"      <div class='led' id='led2' title='400-600 Hz'></div>\n"
"      <div class='led' id='led3' title='600-800 Hz'></div>\n"
"      <div class='led' id='led4' title='800-1200 Hz'></div>\n"
"      <div class='led' id='led5' title='1200-1600 Hz'></div>\n"
"      <div class='led' id='led6' title='1600-2400 Hz'></div>\n"
"      <div class='led' id='led7' title='2400-4000 Hz'></div>\n"
"    </div>\n"
"    \n"
"    <div class='controls'>\n"
"      <div style='margin-bottom:10px;'>\n"
"        <span id='webControlStatus' style='color:#ff6b6b; font-weight:bold;'>⚠ Web Control Disabled - Set switches to 1111 (all ON)</span>\n"
"      </div>\n"
"      <label for='modeSelect' style='color:#fff; margin-right:10px;'>Waveform Mode:</label>\n"
"      <select id='modeSelect' class='btn' onchange='changeMode()' style='width:200px; text-align:left;'>\n"
"        <option value='0'>0: Silence</option>\n"
"        <option value='1'>1: 440 Hz Sine</option>\n"
"        <option value='2'>2: 1000 Hz Sine</option>\n"
"        <option value='3'>3: 2000 Hz Sine</option>\n"
"        <option value='4'>4: Mixed Tones</option>\n"
"        <option value='5'>5: Frequency Sweep</option>\n"
"        <option value='6'>6: White Noise</option>\n"
"        <option value='7'>7: Impulse Train</option>\n"
"        <option value='8'>8: LFM Chirp</option>\n"
"        <option value='9'>9: Sinc Function</option>\n"
"        <option value='10'>10: IQ LFM Chirp</option>\n"
"        <option value='11'>11: Signal + Noise</option>\n"
"      </select>\n"
"      <button class='btn' onclick='togglePause()' style='margin-left:20px;'>Pause/Resume</button>\n"
"      <button class='btn' onclick='resetView()'>Reset View</button>\n"
"    </div>\n"
"    \n"
"    <div class='footer'>\n"
"      FFT Signal Analyzer v1.0 | DE10-Nano FPGA Board | Real-Time Spectrum Analysis\n"
"    </div>\n"
"  </div>\n"
"  \n"
"  <script>\n"
"    // Chart.js setup\n"
"    const timeDomainCtx = document.getElementById('timeDomainChart').getContext('2d');\n"
"    \n"
"    const timeDomainChart = new Chart(timeDomainCtx, {\n"
"      type: 'line',\n"
"      data: {\n"
"        labels: [],\n"
"        datasets: [{\n"
"          label: 'Amplitude',\n"
"          data: [],\n"
"          borderColor: '#00ff00',\n"
"          backgroundColor: 'rgba(0, 255, 0, 0.1)',\n"
"          borderWidth: 1,\n"
"          fill: true,\n"
"          tension: 0,\n"
"          pointRadius: 0\n"
"        }]\n"
"      },\n"
"      options: {\n"
"        responsive: true,\n"
"        maintainAspectRatio: true,\n"
"        plugins: {\n"
"          title: { display: true, text: 'Time Domain Signal', color: '#fff', font: { size: 16 } },\n"
"          legend: { labels: { color: '#fff' } }\n"
"        },\n"
"        scales: {\n"
"          x: { title: { display: true, text: 'Sample', color: '#fff' }, ticks: { color: '#888' }, grid: { color: '#333' } },\n"
"          y: { title: { display: true, text: 'Amplitude', color: '#fff' }, ticks: { color: '#888' }, grid: { color: '#333' }, min: -1, max: 1 }\n"
"        },\n"
"        animation: { duration: 0 }\n"
"      }\n"
"    });\n"
"    \n"
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
"      if (numFrames === 0) return;\n"
"      \n"
"      // Clear canvas\n"
"      spectrogramCtx.fillStyle = '#000';\n"
"      spectrogramCtx.fillRect(0, 0, width, height);\n"
"      \n"
"      const frameWidth = width / maxHistory;\n"
"      const numBins = spectrogramHistory[0].length;\n"
"      const binHeight = height / numBins;\n"
"      \n"
"      // Draw each frame\n"
"      for (let f = 0; f < numFrames; f++) {\n"
"        const frame = spectrogramHistory[f];\n"
"        const x = f * frameWidth;\n"
"        \n"
"        // Draw each frequency bin (flip Y-axis so low freq at bottom)\n"
"        for (let bin = 0; bin < numBins; bin++) {\n"
"          const y = height - (bin + 1) * binHeight;  // Flip Y\n"
"          const value = frame[bin];\n"
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
"        document.getElementById('status').textContent = data.paused ? 'PAUSED' : 'RUNNING';\n"
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
"        // Update time-domain chart\n"
"        const timeSamples = data.time_domain || [];\n"
"        const timeLabels = timeSamples.map((_, i) => i * 4);  // Sample indices (downsampled)\n"
"        timeDomainChart.data.labels = timeLabels;\n"
"        timeDomainChart.data.datasets[0].data = timeSamples;\n"
"        timeDomainChart.update();\n"
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
"        // Update LED display\n"
"        const ledPattern = data.led_pattern || 0;\n"
"        for (let i = 0; i < 8; i++) {\n"
"          const led = document.getElementById('led' + i);\n"
"          if (ledPattern & (1 << i)) {\n"
"            led.classList.add('active');\n"
"          } else {\n"
"            led.classList.remove('active');\n"
"          }\n"
"        }\n"
"        \n"
"        // Update mode selector to match current mode\n"
"        const modeSelect = document.getElementById('modeSelect');\n"
"        const currentModeIndex = Array.from(modeSelect.options).findIndex(opt => \n"
"          data.mode && opt.text.includes(data.mode));\n"
"        if (currentModeIndex >= 0 && modeSelect.selectedIndex !== currentModeIndex) {\n"
"          modeSelect.selectedIndex = currentModeIndex;\n"
"        }\n"
"        \n"
"        // Update web control status indicator\n"
"        const statusEl = document.getElementById('webControlStatus');\n"
"        const isWebControl = data.web_control_active === true;\n"
"        if (isWebControl) {\n"
"          statusEl.textContent = '✓ Web Control Enabled';\n"
"          statusEl.style.color = '#51cf66';\n"
"          modeSelect.disabled = false;\n"
"        } else {\n"
"          statusEl.textContent = '⚠ Web Control Disabled - Set switches to 1111 (all ON)';\n"
"          statusEl.style.color = '#ff6b6b';\n"
"          modeSelect.disabled = true;\n"
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
"        .then(data => console.log('Mode changed to:', data.mode))\n"
"        .catch(error => console.error('Error changing mode:', error));\n"
"    }\n"
"    \n"
"    function togglePause() {\n"
"      fetch('/api/pause', { method: 'POST' })\n"
"        .then(response => response.json())\n"
"        .then(data => console.log('Pause state:', data.paused))\n"
"        .catch(error => console.error('Error toggling pause:', error));\n"
"    }\n"
"    \n"
"    function resetView() {\n"
"      spectrogramHistory.length = 0;\n"
"      console.log('View reset');\n"
"    }\n"
"  </script>\n"
"</body>\n"
"</html>\n";

/*===========================================================================
 * Callback Function Pointers
 *===========================================================================*/

static void (*g_mode_callback)(int mode) = NULL;
static void (*g_pause_callback)(void) = NULL;

void web_server_set_mode_callback(void (*callback)(int mode)) {
    g_mode_callback = callback;
}

void web_server_set_pause_callback(void (*callback)(void)) {
    g_pause_callback = callback;
}

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

uint64_t get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
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

    write(client_fd, header, header_len);
    if (body && body_len > 0) {
        write(client_fd, body, body_len);
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
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

#ifdef SO_REUSEPORT
    // SO_REUSEPORT is not available on all systems
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    // Make socket non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

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
        int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

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

                // Add PSD array (already in dB/Hz)
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
