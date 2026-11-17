#include "os_pulse_core.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <tlhelp32.h>
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <signal.h>
    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define CLOSE_SOCKET close
#endif

static char SERVER_TOKEN[64] = "admin123";
static int IS_PRO_LICENSED = 0;
static char TELEGRAM_BOT_TOKEN[128] = "";
static char TELEGRAM_CHAT_ID[64] = "";
static time_t last_alert_time = 0;

static int validate_license_key(const char *key) {
    if (!key || strncmp(key, "CMICRO-PRO-", 11) != 0 || strlen(key) != 20) return 0;
    if (key[15] != '-') return 0;
    unsigned int sum = 0;
    for (int i = 11; i < 15; i++) {
        if (key[i] < 'A' || key[i] > 'Z') return 0;
        sum += key[i];
    }
    char expected[5];
    snprintf(expected, sizeof(expected), "%04X", (sum * 31337) & 0xFFFF);
    return strncmp(key + 16, expected, 4) == 0;
}

static void send_telegram_alert(double cpu, double ram) {
    if (strlen(TELEGRAM_BOT_TOKEN) == 0 || strlen(TELEGRAM_CHAT_ID) == 0) return;
    time_t now = time(NULL);
    if (now - last_alert_time < 300) return;
    last_alert_time = now;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), 
             "curl -s -X POST \"https://api.telegram.org/bot%s/sendMessage\" -d \"chat_id=%s&text=🚨 *C-MicroOS Pro Alert*%%0AHigh Resource Usage!%%0ACPU: %.1f%%%%0ARAM: %.1f%%\" > /dev/null 2>&1",
             TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID, cpu, ram);
    system(cmd);
}

static void kill_process_by_pid(int pid) {
#if defined(_WIN32)
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe)) {
            do {
                if (pe.th32ProcessID == (DWORD)pid) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProc) { TerminateProcess(hProc, 1); CloseHandle(hProc); }
                    break;
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
#else
    if (pid > 1) kill((pid_t)pid, SIGKILL);
#endif
}

static int check_auth(const char *request) {
    char auth_search[96];
    snprintf(auth_search, sizeof(auth_search), "X-Auth-Token: %s", SERVER_TOKEN);
    if (strstr(request, auth_search) != NULL) return 1;
    snprintf(auth_search, sizeof(auth_search), "token=%s", SERVER_TOKEN);
    if (strstr(request, auth_search) != NULL) return 1;
    return 0;
}

static const char *HTML_DASHBOARD =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n"
"Connection: close\r\n\r\n"
"<!DOCTYPE html><html lang='ar' dir='rtl'><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>حارس الأنظمة | C-MicroOS Pro</title>"
"<style>"
"body{background:#0b1120;color:#f8fafc;font-family:system-ui,-apple-system,'Segoe UI',Tahoma,sans-serif;margin:0;padding:1.5rem;}"
".container{max-width:1050px;margin:auto;}"
".login-box{max-width:380px;margin:6rem auto;background:#111827;padding:2rem;border-radius:14px;border:1px solid #1e293b;text-align:center;box-shadow:0 10px 25px rgba(0,0,0,0.5);}"
".login-box input{width:100%;box-sizing:border-box;padding:0.75rem;margin:1rem 0;background:#0f172a;border:1px solid #334155;color:#fff;border-radius:8px;text-align:center;font-size:1rem;}"
".header{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #1e293b;padding-bottom:1rem;flex-wrap:wrap;gap:1rem;}"
".title-wrap{display:flex;align-items:center;gap:0.75rem;}"
"h1{color:#38bdf8;margin:0;font-size:1.45rem;}"
".badge{background:#064e3b;color:#34d399;padding:0.25rem 0.65rem;border-radius:999px;font-size:0.75rem;font-weight:700;}"
".badge-pro{background:#4c1d95;color:#ddd6fe;padding:0.25rem 0.65rem;border-radius:999px;font-size:0.75rem;font-weight:700;}"
".controls{display:flex;align-items:center;gap:0.6rem;flex-wrap:wrap;}"
".alert-ctrl{display:flex;align-items:center;gap:0.5rem;background:#1e293b;padding:0.4rem 0.8rem;border-radius:8px;font-size:0.85rem;}"
".alert-ctrl input{width:50px;background:#0f172a;border:1px solid #334155;color:#f8fafc;padding:0.2rem;border-radius:4px;text-align:center;}"
".btn{background:#1e293b;border:1px solid #334155;color:#38bdf8;padding:0.45rem 0.85rem;border-radius:8px;cursor:pointer;font-size:0.85rem;font-weight:600;}"
".btn:hover{background:#334155;}"
".btn-primary{background:#0284c7;color:#fff;border:none;width:100%;padding:0.75rem;font-size:1rem;}"
".btn-danger{background:#7f1d1d;border:1px solid #ef4444;color:#fecaca;padding:0.2rem 0.5rem;border-radius:4px;cursor:pointer;font-size:0.75rem;font-weight:600;}"
".alert-banner{display:none;background:#7f1d1d;border:1px solid #ef4444;color:#fecaca;padding:0.85rem 1rem;border-radius:8px;margin-top:1rem;font-weight:600;}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:1rem;margin-top:1.25rem;}"
".card{background:#111827;padding:1.25rem;border-radius:12px;border:1px solid #1f2937;}"
".label{color:#9ca3af;font-size:0.82rem;}"
".value{font-size:1.85rem;font-weight:700;margin-top:0.4rem;color:#22d3ee;direction:ltr;display:inline-block;}"
".sub{color:#6b7280;font-size:0.8rem;margin-top:0.25rem;}"
".split{display:grid;grid-template-columns:1.4fr 1fr;gap:1rem;margin-top:1rem;}"
"@media(max-width:768px){.split{grid-template-columns:1fr;}}"
"canvas{width:100%;height:210px;background:#0f172a;border-radius:8px;margin-top:0.75rem;display:block;}"
"table{width:100%;border-collapse:collapse;margin-top:0.75rem;font-size:0.85rem;}"
"th,td{text-align:start;padding:0.55rem 0.4rem;border-bottom:1px solid #1f2937;}"
"</style></head><body>"
"<div id='loginScreen' class='login-box'>"
"  <h2 style='color:#38bdf8;margin-top:0;'>🔒 C-MicroOS Security</h2>"
"  <p style='color:#9ca3af;font-size:0.9rem;'>أدخل رمز الوصول الخاص بالخادم للمتابعة</p>"
"  <input type='password' id='tokenInput' placeholder='رمز الوصول (الافتراضي: admin123)'>"
"  <button class='btn btn-primary' onclick='login()'>دخول آمن</button>"
"  <p id='loginErr' style='color:#f87171;font-size:0.85rem;display:none;margin-top:0.75rem;'>رمز الوصول غير صحيح!</p>"
"</div>"
"<div id='mainApp' class='container' style='display:none;'>"
"<div class='header'>"
"  <div class='title-wrap'>"
"    <h1 id='appTitle'>حارس الأنظمة C-MicroOS</h1><span class='badge'>● مباشر</span><span id='licBadge' class='badge-pro'>PRO</span>"
"  </div>"
"  <div class='controls'>"
"    <button id='langBtn' class='btn' onclick='toggleLang()'>🌐 English</button>"
"    <button id='csvBtn' class='btn' onclick='exportCSV()'>📥 تصدير CSV</button>"
"    <button id='licBtn' class='btn' onclick='activateLicense()'>🔑 تفعيل الترخيص</button>"
"    <div class='alert-ctrl'><span id='threshLbl'>حد التنبيه:</span> <input type='number' id='thresh' value='80' min='1' max='99'> %</div>"
"  </div>"
"</div>"
"<div id='alertBox' class='alert-banner'></div>"
"<div class='grid'>"
"  <div class='card'><div class='label' id='lblCpu'>حمل المعالج (CPU)</div><div class='value' id='cpu'>--%</div><div class='sub'>قراءة مباشرة من النواة</div></div>"
"  <div class='card'><div class='label' id='lblRam'>استهلاك الذاكرة (RAM)</div><div class='value' id='ram'>-- MB</div><div class='sub' id='ram_sub'>--</div></div>"
"  <div class='card'><div class='label' id='lblDisk'>مساحة التخزين</div><div class='value' id='disk'>-- GB</div><div class='sub' id='disk_sub'>--</div></div>"
"  <div class='card'><div class='label' id='lblUptime'>مدة التشغيل</div><div class='value' id='uptime'>-- s</div><div class='sub'>وقت عمل النواة</div></div>"
"</div>"
"<div class='split'>"
"  <div class='card'><div class='label' id='lblChart'>المراقبة الحية للموارد (60 ثانية)</div><canvas id='chart' width='560' height='210'></canvas></div>"
"  <div class='card'><div class='label' id='lblProcs'>أكثر العمليات استهلاكاً للذاكرة</div><table><thead><tr><th>PID</th><th>العملية</th><th>الذاكرة</th><th>إجراء</th></tr></thead><tbody id='procBody'></tbody></table></div>"
"</div></div>"
"<script>"
"let authToken = sessionStorage.getItem('cmicro_token') || '', lang = 'ar', latestData = null;"
"async function login(){"
"  const val = document.getElementById('tokenInput').value;"
"  const r = await fetch('/api/metrics', { headers: { 'X-Auth-Token': val } });"
"  if(r.ok){ authToken = val; sessionStorage.setItem('cmicro_token', val); showApp(); } else { document.getElementById('loginErr').style.display='block'; }"
"}"
"function showApp(){ document.getElementById('loginScreen').style.display='none'; document.getElementById('mainApp').style.display='block'; setInterval(update, 1000); update(); }"
"if(authToken){ fetch('/api/metrics', { headers: { 'X-Auth-Token': authToken } }).then(r => { if(r.ok) showApp(); }); }"
"async function activateLicense(){"
"  const key = prompt('أدخل مفتاح الترخيص التجاري (مثال: CMICRO-PRO-XXXX-YYYY):');"
"  if(!key) return;"
"  const r = await fetch('/api/license?key=' + encodeURIComponent(key), { method:'POST', headers: { 'X-Auth-Token': authToken } });"
"  const txt = await r.text();"
"  alert(txt === 'VALID' ? '✅ تم تفعيل النسخة الاحترافية وتنبيهات تيليجرام بنجاح!' : '❌ مفتاح الترخيص غير صالح');"
"  update();"
"}"
"async function killProc(pid){"
"  if(!confirm('تأكيد إيقاف العملية PID ' + pid + '؟')) return;"
"  const r = await fetch('/api/kill?pid=' + pid, { method: 'POST', headers: { 'X-Auth-Token': authToken } });"
"  if(r.status === 403) alert('⚠️ ميزة إيقاف العمليات وتنبيهات الخوادم تتطلب تفعيل ترخيص PRO!');"
"  update();"
"}"
"function exportCSV(){"
"  if(!latestData) return;"
"  if(!latestData.pro_licensed){ alert('⚠️ تصدير التقارير متاح في النسخة الاحترافية (PRO) فقط!'); return; }"
"  let csv = 'Metric,Value\\nCPU %,' + latestData.cpu_usage + '\\nRAM (MB),' + latestData.ram_free_mb + '\\nDisk Free (GB),' + latestData.disk_free_gb + '\\n';"
"  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });"
"  const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = 'cmicro_pro_report.csv'; a.click();"
"}"
"function toggleLang(){ lang = lang === 'ar' ? 'en' : 'ar'; document.documentElement.dir = lang === 'ar' ? 'rtl' : 'ltr'; document.getElementById('langBtn').innerText = lang === 'ar' ? '🌐 English' : '🌐 عربي'; }"
"const cpuHist = Array(40).fill(0), ramHist = Array(40).fill(0), canvas = document.getElementById('chart'), ctx = canvas.getContext('2d');"
"function drawChart(){"
"  const w = canvas.width, h = canvas.height; ctx.clearRect(0,0,w,h);"
"  function plot(arr, c){ ctx.strokeStyle=c; ctx.lineWidth=2.5; ctx.beginPath(); arr.forEach((v,i)=>{ let x=(w/(arr.length-1))*i, y=h-(v/100)*h; i===0?ctx.moveTo(x,y):ctx.lineTo(x,y); }); ctx.stroke(); }"
"  plot(ramHist, '#a855f7'); plot(cpuHist, '#22d3ee');"
"}"
"async function update(){"
"  try{"
"    const r = await fetch('/api/metrics', { headers: { 'X-Auth-Token': authToken } });"
"    if(!r.ok) return;"
"    const d = await r.json(); latestData = d;"
"    document.getElementById('licBadge').innerText = d.pro_licensed ? 'PRO ACTIVATED' : 'FREE EDITION';"
"    document.getElementById('licBadge').style.background = d.pro_licensed ? '#065f46' : '#374151';"
"    const ramPct = d.ram_total_mb > 0 ? ((d.ram_total_mb - d.ram_free_mb) / d.ram_total_mb)*100 : 0;"
"    document.getElementById('cpu').innerText = d.cpu_usage.toFixed(1) + '%';"
"    document.getElementById('ram').innerText = (d.ram_total_mb - d.ram_free_mb) + ' MB';"
"    document.getElementById('ram_sub').innerText = 'متاح: ' + d.ram_free_mb + ' / ' + d.ram_total_mb + ' MB';"
"    document.getElementById('disk').innerText = d.disk_free_gb + ' GB';"
"    document.getElementById('disk_sub').innerText = 'من أصل ' + d.disk_total_gb + ' GB';"
"    document.getElementById('uptime').innerText = d.uptime_sec + ' s';"
"    cpuHist.push(d.cpu_usage); cpuHist.shift(); ramHist.push(ramPct); ramHist.shift(); drawChart();"
"    const thresh = parseFloat(document.getElementById('thresh').value) || 80;"
"    const alertBox = document.getElementById('alertBox');"
"    if(d.cpu_usage > thresh || ramPct > thresh){"
"      alertBox.style.display = 'block';"
"      alertBox.innerText = '⚠️ تنبيه: تجاوز استخدام الموارد الحد المسموح (CPU: ' + d.cpu_usage.toFixed(1) + '% | RAM: ' + ramPct.toFixed(1) + '%)';"
"    } else { alertBox.style.display = 'none'; }"
"    document.getElementById('procBody').innerHTML = (d.processes || []).map(p => '<tr><td>'+p.pid+'</td><td>'+p.name+'</td><td dir=\"ltr\">'+p.mem_mb.toFixed(1)+' MB</td><td><button class=\"btn-danger\" onclick=\"killProc('+p.pid+')\">إيقاف</button></td></tr>').join('');"
"  }catch(e){}"
"}"
"</script></body></html>";

void start_web_server(int port) {
    char *env_token = getenv("CMICRO_TOKEN");
    if (env_token && strlen(env_token) > 0) snprintf(SERVER_TOKEN, sizeof(SERVER_TOKEN), "%s", env_token);
    char *env_lic = getenv("CMICRO_LICENSE");
    if (env_lic && validate_license_key(env_lic)) IS_PRO_LICENSED = 1;
    char *env_tg_token = getenv("CMICRO_TG_TOKEN");
    if (env_tg_token) snprintf(TELEGRAM_BOT_TOKEN, sizeof(TELEGRAM_BOT_TOKEN), "%s", env_tg_token);
    char *env_tg_chat = getenv("CMICRO_TG_CHAT");
    if (env_tg_chat) snprintf(TELEGRAM_CHAT_ID, sizeof(TELEGRAM_CHAT_ID), "%s", env_tg_chat);

#if defined(_WIN32)
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("Bind failed"); return; }
    listen(server_fd, 10);

    printf("==================================================\n");
    printf("  C-MicroOS v1.2.1 Pro + Telegram Alerts          \n");
    printf("  Dashboard URL : http://localhost:%d             \n", port);
    printf("  Access Token  : %s                              \n", SERVER_TOKEN);
    printf("  License Mode  : %s                              \n", IS_PRO_LICENSED ? "PRO ACTIVATED" : "FREE (Limited)");
    printf("==================================================\n");

    while (1) {
        struct sockaddr_in client_addr; socklen_t addrlen = sizeof(client_addr);
        socket_t client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd == INVALID_SOCKET) continue;

        char request[2048] = {0};
        recv(client_fd, request, sizeof(request) - 1, 0);

        if (strncmp(request, "GET /api/metrics", 16) == 0) {
            if (!check_auth(request)) {
                const char *unauth = "HTTP/1.1 401 Unauthorized\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nUnauthorized";
                send(client_fd, unauth, (int)strlen(unauth), 0);
            } else {
                SystemMetrics m = get_system_metrics();
                double ram_used_calc = (double)(m.ram_total_mb - m.ram_free_mb);
                double ram_pct = m.ram_total_mb > 0 ? (ram_used_calc / m.ram_total_mb) * 100 : 0;
                if (IS_PRO_LICENSED && (m.cpu_usage > 80.0 || ram_pct > 80.0)) {
                    send_telegram_alert(m.cpu_usage, ram_pct);
                }
                char json_body[2048];
                format_json(m, json_body, sizeof(json_body));
                size_t len = strlen(json_body);
                if (len > 2 && json_body[len - 1] == '}') {
                    snprintf(json_body + len - 1, sizeof(json_body) - len + 1, ",\"pro_licensed\":%s}", IS_PRO_LICENSED ? "true" : "false");
                }
                char response[2600];
                snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n%s", json_body);
                send(client_fd, response, (int)strlen(response), 0);
            }
        } else if (strncmp(request, "POST /api/license", 17) == 0) {
            if (!check_auth(request)) {
                const char *unauth = "HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n";
                send(client_fd, unauth, (int)strlen(unauth), 0);
            } else {
                char *k = strstr(request, "key="); char key_buf[32] = {0};
                if (k) sscanf(k + 4, "%20s", key_buf);
                if (validate_license_key(key_buf)) {
                    IS_PRO_LICENSED = 1;
                    const char *ok = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nVALID";
                    send(client_fd, ok, (int)strlen(ok), 0);
                } else {
                    const char *bad = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nINVALID";
                    send(client_fd, bad, (int)strlen(bad), 0);
                }
            }
        } else if (strncmp(request, "POST /api/kill", 14) == 0) {
            if (!check_auth(request)) {
                const char *unauth = "HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n";
                send(client_fd, unauth, (int)strlen(unauth), 0);
            } else if (!IS_PRO_LICENSED) {
                const char *forb = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nPRO_REQUIRED";
                send(client_fd, forb, (int)strlen(forb), 0);
            } else {
                char *pid_str = strstr(request, "pid=");
                if (pid_str) kill_process_by_pid(atoi(pid_str + 4));
                const char *resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK";
                send(client_fd, resp, (int)strlen(resp), 0);
            }
        } else {
            send(client_fd, HTML_DASHBOARD, (int)strlen(HTML_DASHBOARD), 0);
        }
        CLOSE_SOCKET(client_fd);
    }
}
