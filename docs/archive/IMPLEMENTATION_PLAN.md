# FFT Analyzer - Implementation Plan for Recommendations

## Overview
This document provides detailed implementation plans for improving the FFT Analyzer codebase. Recommendations are prioritized by impact and risk.

---

## HIGH PRIORITY RECOMMENDATIONS

### 1. Add Bounds Checking to JSON Serialization Buffer

**Problem Location:** `web_server.c:784-844`

**Current Issue:**
- Uses 8KB static buffer for JSON serialization
- Multiple `snprintf()` calls without checking if `json_len` exceeds buffer size
- Buffer overflow possible with larger FFT sizes or many bands
- Each iteration adds to `json_len` without validation

**Risk:** High - Buffer overflow could cause crashes or security vulnerabilities

**Current Code Pattern:**
```c
char json[8192];
int json_len = 0;

json_len += snprintf(json + json_len, sizeof(json) - json_len, ...);
// Repeated many times with no check if json_len >= sizeof(json)
```

**Solution:**

**Option A: Add Safety Macro (Recommended)**
```c
#define JSON_SAFE_APPEND(buf, len, size, ...) do { \
    if ((len) < (size) - 1) { \
        int written = snprintf((buf) + (len), (size) - (len), __VA_ARGS__); \
        if (written > 0 && (len) + written < (size)) { \
            (len) += written; \
        } else { \
            fprintf(stderr, "[ERROR] JSON buffer overflow prevented\n"); \
            goto json_error; \
        } \
    } \
} while(0)
```

**Option B: Dynamic Buffer Allocation**
```c
// Calculate required size first
size_t required_size = estimate_json_size(&g_current_data);
char* json = malloc(required_size);
if (!json) {
    // Handle allocation failure
    send_error_response(client_fd);
    return;
}
// ... serialize ...
free(json);
```

**Implementation Steps:**
1. Add bounds checking macro to web_server.c
2. Replace all `json_len += snprintf()` calls with `JSON_SAFE_APPEND()`
3. Add `json_error:` label that sends 500 error response
4. Add unit test with large FFT_SIZE to verify overflow protection

**Estimated Effort:** 2-3 hours
**Files Modified:** `web_server.c`

---

### 2. Validate Network Input Data Sizes

**Problem Location:** `fft_analyzer_network.c:194-221`

**Current Issue:**
- `network_read_samples()` reads arbitrary amount of data from network
- No validation that received data matches expected FFT_SIZE
- Malformed packets could cause buffer overflows
- No maximum packet size enforcement

**Risk:** High - Remote buffer overflow via malicious network data

**Current Code:**
```c
int network_read_samples(network_config_t* config, float* buffer, int num_samples) {
    int bytes_needed = num_samples * sizeof(float);
    int bytes_read = 0;

    // TCP: reads until bytes_needed, but no validation
    // UDP: reads whatever arrives

    return bytes_read / sizeof(float);  // No check if this matches num_samples
}
```

**Solution:**

```c
#define MAX_FFT_SIZE 8192  // Absolute maximum
#define MIN_FFT_SIZE 32    // Absolute minimum

int network_read_samples(network_config_t* config, float* buffer,
                        int num_samples, int max_samples) {
    // Validate input parameters
    if (num_samples <= 0 || num_samples > MAX_FFT_SIZE) {
        fprintf(stderr, "[ERROR] Invalid num_samples: %d\n", num_samples);
        return -1;
    }

    if (num_samples > max_samples) {
        fprintf(stderr, "[ERROR] Requested %d samples exceeds buffer size %d\n",
                num_samples, max_samples);
        return -1;
    }

    int bytes_needed = num_samples * sizeof(float);
    int bytes_read = 0;

    if (config->protocol == NET_PROTOCOL_TCP) {
        // Set timeout to prevent infinite blocking
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(config->socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                   (const char*)&timeout, sizeof(timeout));

        while (bytes_read < bytes_needed) {
            int n = recv(config->socket_fd,
                        ((char*)buffer) + bytes_read,
                        bytes_needed - bytes_read, 0);
            if (n <= 0) {
                fprintf(stderr, "[ERROR] Connection lost or timeout\n");
                return -1;
            }
            bytes_read += n;

            // Prevent infinite loop
            if (bytes_read > bytes_needed) {
                fprintf(stderr, "[ERROR] Received more data than expected\n");
                return -1;
            }
        }
    } else {
        // UDP: validate packet size
        bytes_read = recvfrom(config->socket_fd, (char*)buffer,
                             bytes_needed, 0, NULL, NULL);
        if (bytes_read < 0) {
            perror("[ERROR] UDP receive failed");
            return -1;
        }

        // Validate received size matches expected
        if (bytes_read != bytes_needed) {
            fprintf(stderr, "[WARN] UDP packet size mismatch: got %d, expected %d\n",
                    bytes_read, bytes_needed);
            // Decide: reject or pad with zeros
            if (bytes_read > bytes_needed) {
                bytes_read = bytes_needed;  // Truncate
            }
        }
    }

    int samples_read = bytes_read / sizeof(float);

    // Final validation
    if (samples_read != num_samples) {
        fprintf(stderr, "[WARN] Sample count mismatch: got %d, expected %d\n",
                samples_read, num_samples);
    }

    return samples_read;
}
```

**Additional Validation in Main Loop:**
```c
// In main processing loop (fft_analyzer_network.c:718)
int samples_read = network_read_samples(&g_network_config,
                                       signal_buffer, FFT_SIZE, FFT_SIZE);
if (samples_read != FFT_SIZE) {
    fprintf(stderr, "[ERROR] Expected %d samples, got %d\n",
            FFT_SIZE, samples_read);
    // Handle error: skip frame, use previous data, or switch to test mode
    continue;
}
```

**Implementation Steps:**
1. Add constants for max/min FFT sizes
2. Modify `network_read_samples()` signature to include `max_samples` parameter
3. Add validation at start of function
4. Add socket timeout for TCP reads
5. Add packet size validation for UDP
6. Update all call sites to pass buffer size
7. Add error handling in main loop

**Estimated Effort:** 3-4 hours
**Files Modified:** `fft_analyzer_network.c`

---

### 3. Use Platform-Specific Path Separators

**Problem Location:** `data_logger.c:99, 305, 439` and throughout

**Current Issue:**
- Hardcoded `/` for path separator: `"%s/%s"`
- Works on Windows by accident (accepts both), but not standard
- Inconsistent with Windows conventions
- May fail on some Windows configurations

**Risk:** Medium - Could cause file creation failures on some systems

**Current Code:**
```c
snprintf(logger->filepath, sizeof(logger->filepath), "%s/%s",
         logger->log_directory, temp_filename);
```

**Solution:**

**Add platform-specific path separator:**
```c
// In data_logger.h or data_logger.c header section
#ifdef _WIN32
    #define PATH_SEPARATOR "\\"
    #define PATH_SEPARATOR_CHAR '\\'
#else
    #define PATH_SEPARATOR "/"
    #define PATH_SEPARATOR_CHAR '/'
#endif

// Helper function to build paths safely
static void build_filepath(char* dest, size_t dest_size,
                          const char* dir, const char* filename) {
    // Remove trailing separator from directory if present
    size_t dir_len = strlen(dir);
    if (dir_len > 0 && (dir[dir_len-1] == '/' || dir[dir_len-1] == '\\')) {
        snprintf(dest, dest_size, "%.*s%s%s",
                (int)(dir_len-1), dir, PATH_SEPARATOR, filename);
    } else {
        snprintf(dest, dest_size, "%s%s%s",
                dir, PATH_SEPARATOR, filename);
    }
}
```

**Updated Usage:**
```c
// Replace all instances of manual path building
// OLD: snprintf(logger->filepath, sizeof(logger->filepath), "%s/%s", ...);
// NEW:
build_filepath(logger->filepath, sizeof(logger->filepath),
               logger->log_directory, temp_filename);
```

**Also Fix Directory Removal of Trailing Slash:**
```c
// In data_logger_set_directory (line 406-409)
// Currently only checks for '/' - should check both
size_t len = strlen(logger->log_directory);
if (len > 0 && (logger->log_directory[len-1] == '/' ||
                logger->log_directory[len-1] == '\\')) {
    logger->log_directory[len-1] = '\0';
}
```

**Implementation Steps:**
1. Add PATH_SEPARATOR macros to data_logger.c
2. Create `build_filepath()` helper function
3. Replace all manual path concatenations (3 locations in data_logger.c)
4. Update `data_logger_set_directory()` to handle both separators
5. Test on both Windows and Linux

**Estimated Effort:** 2 hours
**Files Modified:** `data_logger.c`, `data_logger.h`

---

### 4. Add MSVC Compatibility for Packed Structures

**Problem Location:** `data_logger.h:45, 53`

**Current Issue:**
- Uses GCC-specific `__attribute__((packed))`
- Will not compile with Microsoft Visual C++ compiler
- Windows users may want to use MSVC instead of MinGW

**Risk:** Medium - Prevents compilation on MSVC

**Current Code:**
```c
typedef struct {
    char magic[8];
    uint32_t version;
    // ...
} __attribute__((packed)) data_logger_header_t;
```

**Solution:**

```c
// Platform-specific packing directives
#ifdef _MSC_VER
    #define PACK_START __pragma(pack(push, 1))
    #define PACK_END __pragma(pack(pop))
    #define PACKED
#elif defined(__GNUC__)
    #define PACK_START
    #define PACK_END
    #define PACKED __attribute__((packed))
#else
    #define PACK_START
    #define PACK_END
    #define PACKED
    #warning "Structure packing not supported on this compiler"
#endif

// Updated structure definitions
PACK_START
typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t fft_size;
    uint32_t sample_rate;
    uint64_t start_time;
    uint8_t reserved[36];
} PACKED data_logger_header_t;
PACK_END

PACK_START
typedef struct {
    uint64_t timestamp_ms;
} PACKED data_frame_header_t;
PACK_END
```

**Verify Packing Works:**
```c
// Add compile-time assertion
_Static_assert(sizeof(data_logger_header_t) == 64,
               "Header must be exactly 64 bytes");
_Static_assert(sizeof(data_frame_header_t) == 8,
               "Frame header must be exactly 8 bytes");
```

**Implementation Steps:**
1. Add packing macros at top of data_logger.h
2. Wrap structure definitions with PACK_START/PACK_END
3. Add compile-time size assertions
4. Test compilation on both GCC and MSVC
5. Document in build instructions

**Estimated Effort:** 1-2 hours
**Files Modified:** `data_logger.h`

---

## MEDIUM PRIORITY RECOMMENDATIONS

### 5. Pre-allocate FFT Buffers

**Problem Location:** `fft_analyzer_network.c:373-393`

**Current Issue:**
- `compute_fft()` allocates/deallocates buffers on every call (10 Hz)
- Called in main loop at 10 Hz = 10 allocs/frees per second
- Causes memory fragmentation over time
- Unnecessary malloc overhead
- `kiss_fft_alloc()` also allocates twiddle factors each time

**Risk:** Low-Medium - Performance degradation, potential memory fragmentation

**Current Code:**
```c
void compute_fft(const float* input, float* magnitude, int size) {
    kiss_fft_cfg cfg = kiss_fft_alloc(size, 0, NULL, NULL);
    kiss_fft_cpx* fft_in = (kiss_fft_cpx*)malloc(size * sizeof(kiss_fft_cpx));
    kiss_fft_cpx* fft_out = (kiss_fft_cpx*)malloc(size * sizeof(kiss_fft_cpx));

    // ... FFT computation ...

    free(fft_in);
    free(fft_out);
    kiss_fft_free(cfg);
}
```

**Solution:**

**Create FFT Context Structure:**
```c
typedef struct {
    kiss_fft_cfg cfg;
    kiss_fft_cpx* fft_in;
    kiss_fft_cpx* fft_out;
    int size;
} fft_context_t;

fft_context_t* fft_context_create(int fft_size) {
    fft_context_t* ctx = (fft_context_t*)malloc(sizeof(fft_context_t));
    if (!ctx) return NULL;

    ctx->size = fft_size;
    ctx->cfg = kiss_fft_alloc(fft_size, 0, NULL, NULL);
    ctx->fft_in = (kiss_fft_cpx*)malloc(fft_size * sizeof(kiss_fft_cpx));
    ctx->fft_out = (kiss_fft_cpx*)malloc(fft_size * sizeof(kiss_fft_cpx));

    if (!ctx->cfg || !ctx->fft_in || !ctx->fft_out) {
        fft_context_destroy(ctx);
        return NULL;
    }

    return ctx;
}

void fft_context_destroy(fft_context_t* ctx) {
    if (!ctx) return;

    if (ctx->cfg) kiss_fft_free(ctx->cfg);
    if (ctx->fft_in) free(ctx->fft_in);
    if (ctx->fft_out) free(ctx->fft_out);
    free(ctx);
}

void compute_fft_with_context(fft_context_t* ctx, const float* input,
                              float* magnitude) {
    if (!ctx || ctx->size <= 0) return;

    // Copy input to complex array
    for (int i = 0; i < ctx->size; i++) {
        ctx->fft_in[i].r = input[i];
        ctx->fft_in[i].i = 0.0f;
    }

    // Perform FFT
    kiss_fft(ctx->cfg, ctx->fft_in, ctx->fft_out);

    // Compute magnitude
    for (int i = 0; i < ctx->size / 2; i++) {
        magnitude[i] = sqrtf(ctx->fft_out[i].r * ctx->fft_out[i].r +
                            ctx->fft_out[i].i * ctx->fft_out[i].i);
    }
}
```

**Similarly for PSD (Welch):**
```c
typedef struct {
    float* segment;
    float* segment_psd;
    float* accumulated_psd;
    int segment_size;
    int num_bins;
    fft_context_t* fft_ctx;  // Reuse FFT context
} psd_context_t;

psd_context_t* psd_context_create(int segment_size) {
    psd_context_t* ctx = (psd_context_t*)calloc(1, sizeof(psd_context_t));
    if (!ctx) return NULL;

    ctx->segment_size = segment_size;
    ctx->num_bins = segment_size / 2;

    ctx->segment = (float*)malloc(segment_size * sizeof(float));
    ctx->segment_psd = (float*)malloc(ctx->num_bins * sizeof(float));
    ctx->accumulated_psd = (float*)calloc(ctx->num_bins, sizeof(float));
    ctx->fft_ctx = fft_context_create(segment_size);

    if (!ctx->segment || !ctx->segment_psd ||
        !ctx->accumulated_psd || !ctx->fft_ctx) {
        psd_context_destroy(ctx);
        return NULL;
    }

    return ctx;
}

void psd_context_destroy(psd_context_t* ctx) {
    if (!ctx) return;

    if (ctx->segment) free(ctx->segment);
    if (ctx->segment_psd) free(ctx->segment_psd);
    if (ctx->accumulated_psd) free(ctx->accumulated_psd);
    if (ctx->fft_ctx) fft_context_destroy(ctx->fft_ctx);
    free(ctx);
}

void compute_psd_welch_with_context(psd_context_t* ctx, const float* signal,
                                    float* psd, int fft_size, int sample_rate) {
    const int overlap = ctx->segment_size / 2;
    int num_segments = 0;

    // Clear accumulator
    memset(ctx->accumulated_psd, 0, ctx->num_bins * sizeof(float));

    for (int start = 0; start <= fft_size - ctx->segment_size; start += overlap) {
        memcpy(ctx->segment, signal + start, ctx->segment_size * sizeof(float));
        compute_fft_with_context(ctx->fft_ctx, ctx->segment, ctx->segment_psd);

        for (int i = 0; i < ctx->num_bins; i++) {
            ctx->accumulated_psd[i] += ctx->segment_psd[i] * ctx->segment_psd[i];
        }
        num_segments++;
    }

    // Average, normalize, convert to dB
    for (int i = 0; i < ctx->num_bins; i++) {
        float power = ctx->accumulated_psd[i] / num_segments;
        power = power / (ctx->segment_size * ctx->segment_size);
        psd[i] = 10.0f * log10f(power + 1e-10f);
    }
}
```

**Update Main Function:**
```c
int main(int argc, char* argv[]) {
    // ... initialization ...

    // Allocate DSP contexts
    printf("[*] Allocating DSP contexts...\n");
    fft_context_t* fft_ctx = fft_context_create(FFT_SIZE);
    psd_context_t* psd_ctx = psd_context_create(256);  // Welch segment size

    if (!fft_ctx || !psd_ctx) {
        fprintf(stderr, "[ERROR] Failed to create DSP contexts\n");
        ret = 1;
        goto cleanup;
    }
    printf("[OK] DSP contexts allocated\n\n");

    // ... main loop ...
    while (g_running) {
        // ... get signal data ...

        // Compute FFT (no allocation)
        compute_fft_with_context(fft_ctx, signal_buffer, magnitude_buffer);

        // Compute PSD (no allocation)
        compute_psd_welch_with_context(psd_ctx, signal_buffer, psd_buffer,
                                       FFT_SIZE, SAMPLE_RATE);

        // ... rest of processing ...
    }

cleanup:
    // ... cleanup ...

    fft_context_destroy(fft_ctx);
    psd_context_destroy(psd_ctx);

    // ...
}
```

**Performance Improvement:**
- Before: 40 malloc/free calls per second (10 Hz × 4 allocations per frame)
- After: 2 malloc calls at startup, 2 free calls at shutdown
- Estimated performance gain: 5-10% CPU reduction
- Eliminates memory fragmentation risk

**Implementation Steps:**
1. Add fft_context_t and psd_context_t structures
2. Implement create/destroy functions
3. Update compute_fft and compute_psd_welch to use contexts
4. Update main() to create contexts at startup
5. Add proper cleanup in shutdown path
6. Profile before/after to measure improvement

**Estimated Effort:** 4-5 hours
**Files Modified:** `fft_analyzer_network.c`

---

### 6. Add Authentication Option for Web Interface

**Problem Location:** `web_server.c` - entire file

**Current Issue:**
- No authentication required
- Anyone on network can access web interface
- Anyone can change modes, start/stop recording
- Could be exploited if exposed to internet or untrusted network

**Risk:** Medium-High (depending on deployment) - Unauthorized access/control

**Solution:**

**Option A: Simple Token-Based Auth (Recommended for local use)**

```c
// Add to web_server.h
typedef struct {
    bool auth_enabled;
    char auth_token[64];
} web_server_config_t;

// Add to web_server.c
static web_server_config_t g_server_config = {
    .auth_enabled = false,
    .auth_token = ""
};

void web_server_set_auth_token(const char* token) {
    if (token && strlen(token) > 0) {
        strncpy(g_server_config.auth_token, token, sizeof(g_server_config.auth_token) - 1);
        g_server_config.auth_enabled = true;
        printf("[WEB] Authentication enabled (token: %s)\n", token);
    } else {
        g_server_config.auth_enabled = false;
        printf("[WEB] Authentication disabled\n");
    }
}

// Check authorization header
static bool check_authorization(const char* request_buffer) {
    if (!g_server_config.auth_enabled) {
        return true;  // Auth disabled, allow all
    }

    // Look for: Authorization: Bearer <token>
    char* auth_line = strstr(request_buffer, "Authorization:");
    if (!auth_line) {
        return false;
    }

    char* bearer = strstr(auth_line, "Bearer ");
    if (!bearer) {
        return false;
    }

    bearer += 7;  // Skip "Bearer "
    char token[64] = {0};
    int i = 0;
    while (bearer[i] && bearer[i] != '\r' && bearer[i] != '\n' && i < 63) {
        token[i] = bearer[i];
        i++;
    }
    token[i] = '\0';

    return (strcmp(token, g_server_config.auth_token) == 0);
}

// Add to web_server_handle_requests after parsing request
if (!check_authorization(buffer)) {
    const char* msg = "{\"error\":\"Unauthorized\"}";
    send_response(client_fd, "401 Unauthorized", "application/json",
                 msg, strlen(msg));
    close(client_fd);
    continue;
}
```

**Update HTML to Send Token:**
```javascript
// Add to embedded HTML_CONTENT
const AUTH_TOKEN = localStorage.getItem('auth_token') || '';

async function fetchWithAuth(url, options = {}) {
    if (AUTH_TOKEN) {
        options.headers = {
            ...options.headers,
            'Authorization': `Bearer ${AUTH_TOKEN}`
        };
    }
    return fetch(url, options);
}

// Replace all fetch() calls with fetchWithAuth()
```

**Add login prompt in HTML:**
```html
<div id="loginPrompt" style="display:none; position:fixed; top:50%; left:50%; transform:translate(-50%,-50%); background:#2a2a2a; padding:30px; border-radius:8px;">
    <h2>Authentication Required</h2>
    <input type="password" id="tokenInput" placeholder="Enter access token">
    <button onclick="setAuthToken()">Login</button>
</div>

<script>
function setAuthToken() {
    const token = document.getElementById('tokenInput').value;
    localStorage.setItem('auth_token', token);
    location.reload();
}

// Show login prompt on 401
async function updateData() {
    try {
        const response = await fetchWithAuth('/api/fft');
        if (response.status === 401) {
            document.getElementById('loginPrompt').style.display = 'block';
            return;
        }
        // ... rest of code ...
    }
}
</script>
```

**Command Line Option:**
```c
// In main()
char auth_token[64] = "";

// Parse arguments
else if (strcmp(argv[i], "--auth-token") == 0 && i + 1 < argc) {
    strncpy(auth_token, argv[++i], sizeof(auth_token) - 1);
}

// After web server init
if (strlen(auth_token) > 0) {
    web_server_set_auth_token(auth_token);
} else {
    printf("[WARN] No authentication configured. Use --auth-token to secure access.\n");
}
```

**Usage:**
```bash
fft_analyzer_network.exe --test --auth-token "my_secret_token_123"
```

**Implementation Steps:**
1. Add auth config structure and globals
2. Implement `check_authorization()` function
3. Add auth check to all API endpoints
4. Update HTML with token input and storage
5. Add command-line option for token
6. Document in README

**Estimated Effort:** 4-6 hours
**Files Modified:** `web_server.c`, `web_server.h`, `fft_analyzer_network.c`

---

### 7. Implement Network Reconnection Logic

**Problem Location:** `fft_analyzer_network.c:718-723`

**Current Issue:**
- On network read failure, silently switches to test mode
- No attempt to reconnect
- User may not notice connection was lost
- Have to restart entire application to reconnect

**Risk:** Low-Medium - Poor user experience, data loss

**Current Code:**
```c
int samples_read = network_read_samples(&g_network_config,
                                       signal_buffer, FFT_SIZE);
if (samples_read < 0) {
    fprintf(stderr, "[ERROR] Network read failed, switching to test mode\n");
    current_mode = MODE_440HZ;
}
```

**Solution:**

```c
// Add connection state tracking
typedef enum {
    CONN_STATE_DISCONNECTED,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_RECONNECTING
} connection_state_t;

typedef struct {
    connection_state_t state;
    int retry_count;
    int max_retries;
    int retry_delay_ms;
    time_t last_retry_time;
    bool auto_reconnect;
} connection_manager_t;

static connection_manager_t g_conn_manager = {
    .state = CONN_STATE_DISCONNECTED,
    .retry_count = 0,
    .max_retries = 5,
    .retry_delay_ms = 5000,  // 5 seconds
    .last_retry_time = 0,
    .auto_reconnect = true
};

bool attempt_reconnect(network_config_t* config) {
    time_t now = time(NULL);

    // Check if enough time has passed since last retry
    if (g_conn_manager.last_retry_time > 0 &&
        difftime(now, g_conn_manager.last_retry_time) <
        (g_conn_manager.retry_delay_ms / 1000)) {
        return false;  // Too soon to retry
    }

    // Check retry limit
    if (g_conn_manager.retry_count >= g_conn_manager.max_retries) {
        fprintf(stderr, "[ERROR] Max reconnection attempts (%d) reached\n",
                g_conn_manager.max_retries);
        return false;
    }

    printf("[*] Reconnection attempt %d/%d to %s:%d...\n",
           g_conn_manager.retry_count + 1,
           g_conn_manager.max_retries,
           config->host, config->port);

    g_conn_manager.state = CONN_STATE_RECONNECTING;
    g_conn_manager.last_retry_time = now;
    g_conn_manager.retry_count++;

    // Close old connection
    network_close(config);

    // Attempt new connection
    int result = network_connect(config);

    if (result >= 0) {
        printf("[OK] Reconnected successfully\n");
        g_conn_manager.state = CONN_STATE_CONNECTED;
        g_conn_manager.retry_count = 0;  // Reset counter
        return true;
    } else {
        fprintf(stderr, "[ERROR] Reconnection failed\n");
        g_conn_manager.state = CONN_STATE_DISCONNECTED;
        return false;
    }
}

// In main loop
while (g_running) {
    // ... mode handling ...

    if (!g_paused) {
        if (current_mode == MODE_NETWORK_INPUT && use_network) {
            // Check connection state
            if (g_conn_manager.state == CONN_STATE_CONNECTED) {
                int samples_read = network_read_samples(&g_network_config,
                                                       signal_buffer, FFT_SIZE);
                if (samples_read < 0) {
                    fprintf(stderr, "[ERROR] Network read failed\n");
                    g_conn_manager.state = CONN_STATE_DISCONNECTED;

                    // Try immediate reconnect
                    if (!attempt_reconnect(&g_network_config)) {
                        // Failed, switch to test mode temporarily
                        fprintf(stderr, "[WARN] Using test signal until reconnected\n");
                        generate_sine_wave(signal_buffer, FFT_SIZE, 440.0f, 0.8f);
                    } else {
                        // Reconnected, skip this frame
                        continue;
                    }
                }
            } else if (g_conn_manager.state == CONN_STATE_DISCONNECTED) {
                // Not connected, try to reconnect
                if (g_conn_manager.auto_reconnect &&
                    attempt_reconnect(&g_network_config)) {
                    continue;  // Successfully reconnected, skip this frame
                } else {
                    // Still disconnected, use test signal
                    generate_sine_wave(signal_buffer, FFT_SIZE, 440.0f, 0.8f);
                }
            }
        } else {
            // Generate test waveform
            // ... existing code ...
        }

        // ... rest of processing ...
    }

    // ... update web interface ...
}
```

**Add Web Interface Indicator:**
```javascript
// Update status bar to show connection state
<div class='status-item'>
    <div class='status-label'>Network</div>
    <div class='status-value' id='networkStatus'>--</div>
</div>

// In JavaScript updateData()
if (data.network_state === 'connected') {
    document.getElementById('networkStatus').textContent = 'Connected';
    document.getElementById('networkStatus').style.color = '#51cf66';
} else if (data.network_state === 'reconnecting') {
    document.getElementById('networkStatus').textContent = 'Reconnecting...';
    document.getElementById('networkStatus').style.color = '#ff9800';
} else {
    document.getElementById('networkStatus').textContent = 'Disconnected';
    document.getElementById('networkStatus').style.color = '#f44336';
}
```

**Add Command Line Options:**
```bash
--no-auto-reconnect     Disable automatic reconnection
--max-retries N         Maximum reconnection attempts (default: 5)
--retry-delay MS        Delay between retries in ms (default: 5000)
```

**Implementation Steps:**
1. Add connection_manager_t structure
2. Implement attempt_reconnect() function
3. Update main loop to handle connection states
4. Add network state to FFT data structure
5. Update web interface to show connection status
6. Add command-line options for reconnect behavior
7. Test with unreliable network (unplug cable, firewall block, etc.)

**Estimated Effort:** 5-6 hours
**Files Modified:** `fft_analyzer_network.c`, `web_server.h`, `web_server.c` (HTML)

---

### 8. Sanitize system() Call for Browser Auto-Open

**Problem Location:** `fft_analyzer_network.c:686-696`

**Current Issue:**
- Uses `system()` with formatted string
- Could be injection vector if web_port is manipulated
- `system()` is generally unsafe

**Risk:** Low - Port is integer from argv, but still poor practice

**Current Code:**
```c
if (auto_open_browser) {
    printf("[*] Opening browser to http://localhost:%d ...\n", web_port);
    char browser_cmd[256];
#ifdef _WIN32
    snprintf(browser_cmd, sizeof(browser_cmd), "start http://localhost:%d", web_port);
#elif __APPLE__
    snprintf(browser_cmd, sizeof(browser_cmd), "open http://localhost:%d", web_port);
#else
    snprintf(browser_cmd, sizeof(browser_cmd), "xdg-open http://localhost:%d", web_port);
#endif
    system(browser_cmd);
}
```

**Solution:**

**Option A: Remove system() call entirely (Recommended)**
```c
// Just print the URL
if (auto_open_browser) {
    printf("\n");
    printf("========================================\n");
    printf("  Open your browser to:\n");
    printf("  http://localhost:%d\n", web_port);
    printf("========================================\n");
    printf("\n");
}
```

**Option B: Use platform-specific safe APIs**
```c
#ifdef _WIN32
#include <shellapi.h>

bool open_browser_windows(int port) {
    char url[256];
    snprintf(url, sizeof(url), "http://localhost:%d", port);

    // ShellExecute is safer than system()
    HINSTANCE result = ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    return ((INT_PTR)result > 32);
}
#endif

// In main()
if (auto_open_browser) {
    #ifdef _WIN32
        if (open_browser_windows(web_port)) {
            printf("[OK] Browser opened\n");
        } else {
            printf("[INFO] Could not auto-open browser. Please open manually:\n");
            printf("       http://localhost:%d\n", web_port);
        }
    #else
        // On Linux/Mac, just print URL - too many desktop environments to handle
        printf("\n[INFO] Please open your browser to: http://localhost:%d\n\n", web_port);
    #endif
}
```

**Validate port number:**
```c
// In argument parsing
else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
    web_port = atoi(argv[++i]);
    // Validate port range
    if (web_port < 1024 || web_port > 65535) {
        fprintf(stderr, "[ERROR] Invalid port %d. Must be 1024-65535\n", web_port);
        print_usage(argv[0]);
        return 1;
    }
}
```

**Implementation Steps:**
1. Remove or replace system() call
2. Add port validation
3. Use ShellExecute on Windows (optional)
4. Update documentation

**Estimated Effort:** 1-2 hours
**Files Modified:** `fft_analyzer_network.c`

---

## LOW PRIORITY RECOMMENDATIONS

### 9. Extract Magic Numbers to Named Constants

**Problem Locations:** Throughout codebase

**Current Issues:**
- Magic numbers scattered: `256`, `128`, `10`, `4`, `2`, etc.
- Hard to understand meaning without context
- Difficult to change values
- Inconsistent downsample factors

**Examples:**
```c
// web_server.c:800 - Downsample factor
for (int i = 0; i < g_current_data.fft_size; i += 4)

// fft_analyzer_network.c:396 - Welch segment size
const int segment_size = 256;

// web_server.c:830 - PSD downsample
for (int i = 0; i < g_current_data.psd_size; i += 2)
```

**Solution:**

**Create constants header or section:**
```c
// In fft_analyzer_network.c or new constants.h
/*===========================================================================
 * DSP Configuration Constants
 *===========================================================================*/
#define FFT_SIZE                512
#define SAMPLE_RATE            8000
#define UPDATE_RATE_HZ          10
#define UPDATE_RATE_MS          (1000 / UPDATE_RATE_HZ)

// Welch PSD parameters
#define WELCH_SEGMENT_SIZE      256
#define WELCH_OVERLAP_PERCENT   50
#define WELCH_OVERLAP_SAMPLES   (WELCH_SEGMENT_SIZE * WELCH_OVERLAP_PERCENT / 100)
#define WELCH_NUM_BINS          (WELCH_SEGMENT_SIZE / 2)
#define PSD_OUTPUT_SIZE         WELCH_NUM_BINS

// Frequency band analysis
#define NUM_FREQUENCY_BANDS     8
#define BAND_ENERGY_THRESHOLD   0.01f

// Web interface downsampling
#define WEB_TIME_DOMAIN_DOWNSAMPLE  4  // 512 -> 128 points
#define WEB_MAGNITUDE_DOWNSAMPLE    4  // 256 -> 64 points
#define WEB_PSD_DOWNSAMPLE          2  // 128 -> 64 points

// Network configuration
#define NETWORK_TIMEOUT_SEC     5
#define MAX_FFT_SIZE_LIMIT      8192
#define MIN_FFT_SIZE_LIMIT      32

// Data logging
#define LOG_FLUSH_INTERVAL      10  // Flush every N frames

/*===========================================================================
 * Buffer Sizes
 *===========================================================================*/
#define JSON_BUFFER_SIZE        8192
#define HTTP_REQUEST_BUFFER     4096
#define FILEPATH_MAX            512
#define DIRECTORY_PATH_MAX      256
```

**Replace magic numbers:**
```c
// Before:
for (int i = 0; i < g_current_data.fft_size; i += 4)

// After:
for (int i = 0; i < g_current_data.fft_size; i += WEB_TIME_DOMAIN_DOWNSAMPLE)

// Before:
const int segment_size = 256;

// After:
const int segment_size = WELCH_SEGMENT_SIZE;
```

**Implementation Steps:**
1. Identify all magic numbers
2. Group by category (DSP, network, web, logging)
3. Create named constants with comments
4. Replace throughout codebase
5. Verify functionality unchanged

**Estimated Effort:** 3-4 hours
**Files Modified:** All source files

---

### 10. Standardize Error Message Format

**Current Issue:**
- Inconsistent prefixes: `[ERROR]`, `[LOGGER]`, `[WEB]`, `[*]`, `[OK]`
- Mixed stderr/stdout usage
- No timestamps
- No log levels

**Examples:**
```c
fprintf(stderr, "[ERROR] Failed to open file\n");
printf("[*] Initializing web server...\n");
printf("[OK] Ready!\n");
perror("[ERROR] Connection failed");
```

**Solution:**

**Create logging system:**
```c
// logging.h
typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

typedef enum {
    LOG_MODULE_MAIN,
    LOG_MODULE_NETWORK,
    LOG_MODULE_DSP,
    LOG_MODULE_WEB,
    LOG_MODULE_LOGGER
} log_module_t;

void log_message(log_level_t level, log_module_t module, const char* format, ...);
void log_set_level(log_level_t min_level);
void log_set_timestamps(bool enabled);

// Convenience macros
#define LOG_DEBUG(mod, ...) log_message(LOG_LEVEL_DEBUG, mod, __VA_ARGS__)
#define LOG_INFO(mod, ...)  log_message(LOG_LEVEL_INFO, mod, __VA_ARGS__)
#define LOG_WARN(mod, ...)  log_message(LOG_LEVEL_WARN, mod, __VA_ARGS__)
#define LOG_ERROR(mod, ...) log_message(LOG_LEVEL_ERROR, mod, __VA_ARGS__)
```

**Implementation:**
```c
// logging.c
static log_level_t g_min_log_level = LOG_LEVEL_INFO;
static bool g_show_timestamps = true;

static const char* level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR"
};

static const char* module_names[] = {
    "MAIN", "NETWORK", "DSP", "WEB", "LOGGER"
};

void log_message(log_level_t level, log_module_t module, const char* format, ...) {
    if (level < g_min_log_level) return;

    FILE* output = (level >= LOG_LEVEL_WARN) ? stderr : stdout;

    // Timestamp
    if (g_show_timestamps) {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        fprintf(output, "[%02d:%02d:%02d] ",
                tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    }

    // Level and module
    fprintf(output, "[%-6s] [%-7s] ", level_names[level], module_names[module]);

    // Message
    va_list args;
    va_start(args, format);
    vfprintf(output, format, args);
    va_end(args);

    fprintf(output, "\n");
    fflush(output);
}
```

**Usage:**
```c
// Before:
printf("[*] Initializing web server on port %d...\n", web_port);
fprintf(stderr, "[ERROR] Failed to initialize web server\n");

// After:
LOG_INFO(LOG_MODULE_WEB, "Initializing web server on port %d", web_port);
LOG_ERROR(LOG_MODULE_WEB, "Failed to initialize web server");
```

**Add command-line options:**
```bash
--log-level debug|info|warn|error    Set minimum log level
--no-timestamps                       Disable timestamps in logs
--quiet                              Minimal output
```

**Implementation Steps:**
1. Create logging.h/c
2. Replace all printf/fprintf with LOG_* macros
3. Add command-line options
4. Test log output

**Estimated Effort:** 4-5 hours
**Files Modified:** All source files

---

### 11. Add Unit Tests for DSP Functions

**Current Issue:**
- No automated tests
- Manual testing only
- Risk of regressions when modifying DSP code
- Hard to verify correctness of FFT, PSD, etc.

**Solution:**

**Create test framework:**
```c
// tests/test_dsp.c
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define TEST_EPSILON 1e-5

int tests_passed = 0;
int tests_failed = 0;

#define ASSERT_FLOAT_EQ(a, b) do { \
    if (fabsf((a) - (b)) > TEST_EPSILON) { \
        fprintf(stderr, "FAIL: %s:%d: %f != %f\n", __FILE__, __LINE__, (a), (b)); \
        tests_failed++; \
        return; \
    } \
    tests_passed++; \
} while(0)

void test_compute_fft_dc_signal() {
    printf("Test: FFT of DC signal...\n");

    float input[512];
    float magnitude[256];

    // DC signal (constant 1.0)
    for (int i = 0; i < 512; i++) {
        input[i] = 1.0f;
    }

    compute_fft(input, magnitude, 512);

    // DC component should be ~512 (sum of all samples)
    ASSERT_FLOAT_EQ(magnitude[0], 512.0f);

    // All other bins should be near zero
    for (int i = 1; i < 256; i++) {
        if (fabsf(magnitude[i]) > 0.1f) {
            fprintf(stderr, "FAIL: Bin %d = %f (expected ~0)\n", i, magnitude[i]);
            tests_failed++;
            return;
        }
    }
    tests_passed++;
}

void test_compute_fft_sine_wave() {
    printf("Test: FFT of sine wave...\n");

    float input[512];
    float magnitude[256];

    // Generate 1kHz sine at 8kHz sample rate
    float freq = 1000.0f;
    float sample_rate = 8000.0f;

    for (int i = 0; i < 512; i++) {
        float t = (float)i / sample_rate;
        input[i] = sinf(2.0f * M_PI * freq * t);
    }

    compute_fft(input, magnitude, 512);

    // Calculate expected bin
    int expected_bin = (int)(freq * 512 / sample_rate);  // bin 64

    // Find peak bin
    int peak_bin = 0;
    float peak_value = 0.0f;
    for (int i = 0; i < 256; i++) {
        if (magnitude[i] > peak_value) {
            peak_value = magnitude[i];
            peak_bin = i;
        }
    }

    // Peak should be at expected bin
    if (peak_bin != expected_bin) {
        fprintf(stderr, "FAIL: Peak at bin %d, expected %d\n", peak_bin, expected_bin);
        tests_failed++;
        return;
    }

    tests_passed++;
}

void test_psd_white_noise() {
    printf("Test: PSD of white noise...\n");

    float signal[512];
    float psd[128];

    // Generate white noise
    srand(42);  // Fixed seed for reproducibility
    for (int i = 0; i < 512; i++) {
        signal[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    }

    compute_psd_welch(signal, psd, 512, 8000);

    // White noise PSD should be relatively flat
    float mean_psd = 0.0f;
    for (int i = 0; i < 128; i++) {
        mean_psd += psd[i];
    }
    mean_psd /= 128.0f;

    // Check that most bins are within reasonable range of mean
    int outliers = 0;
    for (int i = 0; i < 128; i++) {
        if (fabsf(psd[i] - mean_psd) > 10.0f) {  // 10 dB deviation
            outliers++;
        }
    }

    if (outliers > 20) {  // Allow some variation
        fprintf(stderr, "FAIL: Too many outliers in white noise PSD: %d\n", outliers);
        tests_failed++;
        return;
    }

    tests_passed++;
}

void test_band_energy() {
    printf("Test: Band energy calculation...\n");

    float magnitude[256];

    // Create signal with energy only in 500-1000 Hz
    for (int i = 0; i < 256; i++) {
        float freq = (float)i * 8000.0f / 512.0f;
        if (freq >= 500.0f && freq <= 1000.0f) {
            magnitude[i] = 1.0f;
        } else {
            magnitude[i] = 0.0f;
        }
    }

    // Test band that should have energy
    float energy1 = get_band_energy(magnitude, 512, 400.0f, 1200.0f);
    if (energy1 < 0.5f) {
        fprintf(stderr, "FAIL: Expected energy in band, got %f\n", energy1);
        tests_failed++;
        return;
    }

    // Test band that should have no energy
    float energy2 = get_band_energy(magnitude, 512, 2000.0f, 3000.0f);
    if (energy2 > 0.1f) {
        fprintf(stderr, "FAIL: Expected no energy in band, got %f\n", energy2);
        tests_failed++;
        return;
    }

    tests_passed++;
}

int main() {
    printf("Running DSP tests...\n\n");

    test_compute_fft_dc_signal();
    test_compute_fft_sine_wave();
    test_psd_white_noise();
    test_band_energy();

    printf("\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);

    return (tests_failed == 0) ? 0 : 1;
}
```

**Add to build system:**
```makefile
# Add to Makefile.windows
test: test_dsp.exe
	./test_dsp.exe

test_dsp.exe: tests/test_dsp.c kiss_fft.c
	$(CC) $(CFLAGS) -o $@ $^ -lm
```

**Implementation Steps:**
1. Create tests/ directory
2. Write test cases for each DSP function
3. Add test target to build system
4. Document how to run tests
5. Add to CI/CD if available

**Estimated Effort:** 6-8 hours
**Files Modified:** New test files, Makefile

---

## Summary

### Effort Estimate by Priority

**HIGH PRIORITY (Critical fixes):**
- Total: 10-13 hours
- Should be done before production deployment

**MEDIUM PRIORITY (Important improvements):**
- Total: 15-20 hours
- Recommended for production use

**LOW PRIORITY (Nice to have):**
- Total: 13-17 hours
- Can be done incrementally

### Recommended Implementation Order

1. **Week 1:** High priority items (security & stability)
   - JSON bounds checking
   - Network input validation
   - Path separators
   - MSVC compatibility

2. **Week 2:** Medium priority items (performance & UX)
   - Pre-allocate FFT buffers
   - Network reconnection
   - Browser auto-open fix

3. **Week 3:** Authentication (if deploying to network)
   - Web interface authentication

4. **Week 4+:** Low priority (code quality)
   - Magic numbers
   - Logging system
   - Unit tests

Would you like me to start implementing any of these recommendations?
