#include "os_pulse_core.h"
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
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

static void kill_process_by_pid(int pid) {
#if defined(_WIN32)
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe)) {
            do {
                if (pe.th32ProcessID == (DWORD)pid) {
                    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProc) {
                        TerminateProcess(hProc, 1);
                        CloseHandle(hProc);
                    }
                    break;
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
#else
    if (pid > 1) {
        kill((pid_t)pid, SIGKILL);
    }
#endif
}

static const char *HTML_DASHBOARD =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n"
"Connection: close\r\n\r\n"
"<!DOCTYPE html><html lang='ar' dir='rtl'><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>حارس الأنظمة | C-MicroOS Edge Sentinel</title>"
"<link rel='icon' type='image/svg+xml' href=\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'><rect width='64' height='64' rx='14' fill='%230f172a'/><path d='M8 34h12l6-16 10 32 8-22 4 6h8' stroke='%2322d3ee' stroke-width='5' fill='none' stroke-linecap='round' stroke-linejoin='round'/></svg>\">"
"<style>"
"body{background:#0b1120;color:#f8fafc;font-family:system-ui,-apple-system,'Segoe UI',Tahoma,sans-serif;margin:0;padding:1.5rem;}"
".container{max-width:1050px;margin:auto;}"
".header{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #1e293b;padding-bottom:1rem;flex-wrap:wrap;gap:1rem;}"
".title-wrap{display:flex;align-items:center;gap:0.75rem;}"
"h1{color:#38bdf8;margin:0;font-size:1.45rem;}"
".badge{background:#064e3b;color:#34d399;padding:0.25rem 0.65rem;border-radius:999px;font-size:0.75rem;font-weight:700;}"
".controls{display:flex;align-items:center;gap:0.6rem;flex-wrap:wrap;}"
".alert-ctrl{display:flex;align-items:center;gap:0.5rem;background:#1e293b;padding:0.4rem 0.8rem;border-radius:8px;font-size:0.85rem;}"
".alert-ctrl input{width:50px;background:#0f172a;border:1px solid #334155;color:#f8fafc;padding:0.2rem;border-radius:4px;text-align:center;}"
".btn{background:#1e293b;border:1px solid #334155;color:#38bdf8;padding:0.4rem 0.8rem;border-radius:8px;cursor:pointer;font-size:0.85rem;font-weight:600;}"
".btn:hover{background:#334155;}"
".btn-danger{background:#7f1d1d;border:1px solid #ef4444;color:#fecaca;padding:0.2rem 0.5rem;border-radius:4px;cursor:pointer;font-size:0.75rem;font-weight:600;}"
".btn-danger:hover{background:#991b1b;}"
".alert-banner{display:none;background:#7f1d1d;border:1px solid #ef4444;color:#fecaca;padding:0.85rem 1rem;border-radius:8px;margin-top:1rem;font-weight:600;animation:pulse 1.5s infinite;}"
"@keyframes pulse{0%,100%{opacity:1;}50%{opacity:0.75;}}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:1rem;margin-top:1.25rem;}"
".card{background:#111827;padding:1.25rem;border-radius:12px;border:1px solid #1f2937;box-shadow:0 4px 12px rgba(0,0,0,0.4);}"
".label{color:#9ca3af;font-size:0.82rem;letter-spacing:0.02em;}"
".value{font-size:1.85rem;font-weight:700;margin-top:0.4rem;color:#22d3ee;direction:ltr;display:inline-block;}"
".sub{color:#6b7280;font-size:0.8rem;margin-top:0.25rem;}"
".split{display:grid;grid-template-columns:1.4fr 1fr;gap:1rem;margin-top:1rem;}"
"@media(max-width:768px){.split{grid-template-columns:1fr;}}"
"canvas{width:100%;height:210px;background:#0f172a;border-radius:8px;margin-top:0.75rem;display:block;}"
".legend{display:flex;gap:1rem;font-size:0.8rem;margin-top:0.5rem;color:#9ca3af;}"
".dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-inline-end:5px;}"
"table{width:100%;border-collapse:collapse;margin-top:0.75rem;font-size:0.85rem;}"
"th,td{text-align:start;padding:0.55rem 0.4rem;border-bottom:1px solid #1f2937;}"
"th{color:#9ca3af;font-weight:600;}"
"</style></head><body><div class='container'>"
"<div class='header'>"
"  <div class='title-wrap'>"
"    <svg width='32' height='32' viewBox='0 0 64 64'><rect width='64' height='64' rx='14' fill='#1e293b'/><path d='M8 34h12l6-16 10 32 8-22 4 6h8' stroke='#22d3ee' stroke-width='5' fill='none' stroke-linecap='round' stroke-linejoin='round'/></svg>"
"    <h1 id='appTitle'>حارس الأنظمة C-MicroOS</h1><span class='badge' id='liveBadge'>● مباشر</span>"
"  </div>"
"  <div class='controls'>"
"    <button id='langBtn' class='btn' onclick='toggleLang()'>🌐 English</button>"
"    <button id='csvBtn' class='btn' onclick='exportCSV()'>📥 تصدير تقرير CSV</button>"
"    <button id='soundBtn' class='btn' onclick='toggleSound()'>🔔 التنبيه الصوتي: مفعل</button>"
"    <div class='alert-ctrl'><span id='threshLbl'>حد التنبيه:</span> <input type='number' id='thresh' value='80' min='1' max='99'> %</div>"
"  </div>"
"</div>"
"<div id='alertBox' class='alert-banner'></div>"
"<div class='grid'>"
"  <div class='card'><div class='label' id='lblCpu'>حمل المعالج (CPU)</div><div class='value' id='cpu'>--%</div><div class='sub' id='subCpu'>قراءة مباشرة من النواة</div></div>"
"  <div class='card'><div class='label' id='lblRam'>استهلاك الذاكرة (RAM)</div><div class='value' id='ram'>-- MB</div><div class='sub' id='ram_sub'>--</div></div>"
"  <div class='card'><div class='label' id='lblDisk'>مساحة التخزين</div><div class='value' id='disk'>-- GB</div><div class='sub' id='disk_sub'>--</div></div>"
"  <div class='card'><div class='label' id='lblUptime'>مدة التشغيل</div><div class='value' id='uptime'>-- s</div><div class='sub' id='subUptime'>وقت عمل النواة المتواصل</div></div>"
"</div>"
"<div class='split'>"
"  <div class='card'>"
"    <div class='label' id='lblChart'>المراقبة الحية للموارد (آخر 60 ثانية)</div>"
"    <canvas id='chart' width='560' height='210'></canvas>"
"    <div class='legend'><span><i class='dot' style='background:#22d3ee'></i><span id='legCpu'>المعالج %</span></span><span><i class='dot' style='background:#a855f7'></i><span id='legRam'>الذاكرة %</span></span></div>"
"  </div>"
"  <div class='card'>"
"    <div class='label' id='lblProcs'>أكثر العمليات استهلاكاً للذاكرة (مع إمكانية الإنهاء الفوري)</div>"
"    <table><thead><tr><th id='thPid'>المعرف</th><th id='thName'>العملية</th><th id='thMem'>الذاكرة</th><th id='thAct'>إجراء</th></tr></thead><tbody id='procBody'></tbody></table>"
"  </div>"
"</div></div>"
"<script>"
"let lang = 'ar', soundEnabled = true, audioCtx = null, latestData = null;"
"const dict = {"
"  ar: { title:'حارس الأنظمة C-MicroOS', live:'● مباشر', langBtn:'🌐 English', csvBtn:'📥 تصدير تقرير CSV', soundOn:'🔔 التنبيه الصوتي: مفعل', soundOff:'🔕 التنبيه الصوتي: صامت', thresh:'حد التنبيه:', cpu:'حمل المعالج (CPU)', subCpu:'قراءة مباشرة من النواة', ram:'استهلاك الذاكرة (RAM)', disk:'مساحة التخزين', uptime:'مدة التشغيل', subUptime:'وقت عمل النواة المتواصل', chart:'المراقبة الحية للموارد (آخر 60 ثانية)', legCpu:'المعالج %', legRam:'الذاكرة %', procs:'أكثر العمليات استهلاكاً للذاكرة (مع إمكانية الإنهاء الفوري)', pid:'المعرف', name:'العملية', mem:'الذاكرة', act:'إجراء', kill:'إيقاف', free:'متاح:', total:'الإجمالي:', freeOf:'متاح من أصل', sec:'ثانية', alert:'⚠ تنبيه: تم تجاوز الحد المسموح! المعالج:' },"
"  en: { title:'C-MicroOS Edge Sentinel', live:'● LIVE', langBtn:'🌐 عربي', csvBtn:'📥 Export CSV Report', soundOn:'🔔 Audio Alert: ON', soundOff:'🔕 Audio Alert: OFF', thresh:'Threshold:', cpu:'CPU Load', subCpu:'Real-time Kernel Telemetry', ram:'Memory Usage', disk:'Root Storage', uptime:'System Uptime', subUptime:'Active Kernel Time', chart:'Real-Time Resource Telemetry (60s History)', legCpu:'CPU %', legRam:'RAM %', procs:'Top Active Processes (With Instant Kill)', pid:'PID', name:'Process', mem:'Memory', act:'Action', kill:'Kill', free:'Free:', total:'Total:', freeOf:'Free of', sec:'s', alert:'⚠ ALERT: High Load Detected! CPU:' }"
"};"
"function applyLang(){"
"  const t = dict[lang];"
"  document.documentElement.dir = lang === 'ar' ? 'rtl' : 'ltr';"
"  document.documentElement.lang = lang;"
"  document.getElementById('appTitle').innerText = t.title;"
"  document.getElementById('liveBadge').innerText = t.live;"
"  document.getElementById('langBtn').innerText = t.langBtn;"
"  document.getElementById('csvBtn').innerText = t.csvBtn;"
"  document.getElementById('soundBtn').innerText = soundEnabled ? t.soundOn : t.soundOff;"
"  document.getElementById('threshLbl').innerText = t.thresh;"
"  document.getElementById('lblCpu').innerText = t.cpu;"
"  document.getElementById('subCpu').innerText = t.subCpu;"
"  document.getElementById('lblRam').innerText = t.ram;"
"  document.getElementById('lblDisk').innerText = t.disk;"
"  document.getElementById('lblUptime').innerText = t.uptime;"
"  document.getElementById('subUptime').innerText = t.subUptime;"
"  document.getElementById('lblChart').innerText = t.chart;"
"  document.getElementById('legCpu').innerText = t.legCpu;"
"  document.getElementById('legRam').innerText = t.legRam;"
"  document.getElementById('lblProcs').innerText = t.procs;"
"  document.getElementById('thPid').innerText = t.pid;"
"  document.getElementById('thName').innerText = t.name;"
"  document.getElementById('thMem').innerText = t.mem;"
"  document.getElementById('thAct').innerText = t.act;"
"}"
"function toggleLang(){ lang = lang === 'ar' ? 'en' : 'ar'; applyLang(); if(latestData) renderProcs(latestData); }"
"function playBeep(){"
"  if(!soundEnabled) return;"
"  try{"
"    if(!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();"
"    if(audioCtx.state === 'suspended') audioCtx.resume();"
"    const osc = audioCtx.createOscillator(), gain = audioCtx.createGain();"
"    osc.type = 'sine'; osc.frequency.setValueAtTime(880, audioCtx.currentTime);"
"    osc.frequency.exponentialRampToValueAtTime(440, audioCtx.currentTime + 0.25);"
"    gain.gain.setValueAtTime(0.15, audioCtx.currentTime);"
"    gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + 0.25);"
"    osc.connect(gain); gain.connect(audioCtx.destination);"
"    osc.start(); osc.stop(audioCtx.currentTime + 0.25);"
"  }catch(e){}"
"}"
"function toggleSound(){ soundEnabled = !soundEnabled; applyLang(); if(soundEnabled) playBeep(); }"
"async function killProc(pid){"
"  if(!confirm('Are you sure you want to terminate PID ' + pid + '?')) return;"
"  await fetch('/api/kill?pid=' + pid, {method: 'POST'});"
"  update();"
"}"
"function exportCSV(){"
"  if(!latestData) return;"
"  const t = dict[lang];"
"  let csv = 'Metric,Value\\n';"
"  csv += 'CPU %,' + latestData.cpu_percent + '\\n';"
"  csv += 'RAM Used (MB),' + latestData.ram_used_mb + '\\n';"
"  csv += 'RAM Free (MB),' + latestData.ram_free_mb + '\\n';"
"  csv += 'Disk Free (GB),' + latestData.disk_free_gb + '\\n';"
"  csv += 'Uptime (sec),' + latestData.uptime_sec + '\\n\\n';"
"  csv += 'PID,Process Name,Memory (MB)\\n';"
"  (latestData.processes || []).forEach(p => { csv += p.pid + ',\"' + p.name + '\"\\,' + p.mem_mb + '\\n'; });"
"  const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });"
"  const url = URL.createObjectURL(blob);"
"  const a = document.createElement('a'); a.href = url;"
"  a.download = 'cmicro_os_report_' + Date.now() + '.csv';"
"  a.click();"
"}"
"const cpuHist = Array(40).fill(0), ramHist = Array(40).fill(0);"
"const canvas = document.getElementById('chart'), ctx = canvas.getContext('2d');"
"function drawChart(){"
"  const w = canvas.width, h = canvas.height;"
"  ctx.clearRect(0,0,w,h);"
"  ctx.strokeStyle='#1e293b'; ctx.lineWidth=1;"
"  for(let i=1;i<4;i++){ let y=(h/4)*i; ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(w,y); ctx.stroke(); }"
"  function plot(arr, color){"
"    ctx.strokeStyle=color; ctx.lineWidth=2.5; ctx.beginPath();"
"    arr.forEach((v,i)=>{ let x=(w/(arr.length-1))*i; let y=h-(v/100)*h; i===0?ctx.moveTo(x,y):ctx.lineTo(x,y); });"
"    ctx.stroke();"
"  }"
"  plot(ramHist, '#a855f7'); plot(cpuHist, '#22d3ee');"
"}"
"function renderProcs(d){"
"  const t = dict[lang];"
"  const tbody = document.getElementById('procBody');"
"  tbody.innerHTML = (d.processes || []).map(p => '<tr><td>'+p.pid+'</td><td>'+p.name+'</td><td dir=\"ltr\">'+p.mem_mb.toFixed(1)+' MB</td><td><button class=\"btn-danger\" onclick=\"killProc('+p.pid+')\">'+t.kill+'</button></td></tr>').join('');"
"}"
"async function update(){"
"  try{"
"    const r = await fetch('/api/metrics');"
"    const d = await r.json();"
"    latestData = d;"
"    const t = dict[lang];"
"    const ramPct = d.ram_total_mb > 0 ? (d.ram_used_mb / d.ram_total_mb)*100 : 0;"
"    document.getElementById('cpu').innerText = d.cpu_percent.toFixed(1) + '%';"
"    document.getElementById('ram').innerText = d.ram_used_mb + ' MB';"
"    document.getElementById('ram_sub').innerText = t.free + ' ' + d.ram_free_mb + ' / ' + t.total + ' ' + d.ram_total_mb + ' MB (' + ramPct.toFixed(0) + '%)';"
"    document.getElementById('disk').innerText = d.disk_free_gb + ' GB';"
"    document.getElementById('disk_sub').innerText = t.freeOf + ' ' + d.disk_total_gb + ' GB';"
"    document.getElementById('uptime').innerText = d.uptime_sec + ' ' + t.sec;"
"    cpuHist.push(d.cpu_percent); cpuHist.shift();"
"    ramHist.push(ramPct); ramHist.shift();"
"    drawChart();"
"    const thresh = parseFloat(document.getElementById('thresh').value) || 80;"
"    const alertBox = document.getElementById('alertBox');"
"    if(d.cpu_percent > thresh || ramPct > thresh){"
"      alertBox.style.display = 'block';"
"      alertBox.innerText = t.alert + ' ' + d.cpu_percent.toFixed(1) + '% | RAM: ' + ramPct.toFixed(1) + '% (' + thresh + '%)';"
"      playBeep();"
"    } else { alertBox.style.display = 'none'; }"
"    renderProcs(d);"
"  }catch(e){}"
"}"
"applyLang(); setInterval(update, 1000); update();"
"</script></body></html>";

void start_web_server(int port) {
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) { perror("Socket failed"); return; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("Bind failed"); CLOSE_SOCKET(server_fd); return; }
    if (listen(server_fd, 10) < 0) { perror("Listen failed"); CLOSE_SOCKET(server_fd); return; }

    printf("==================================================\n");
    printf("  C-MicroOS v1.1 (Kill Process & CSV Export)      \n");
    printf("  Dashboard URL : http://localhost:%d             \n", port);
    printf("==================================================\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        socket_t client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd == INVALID_SOCKET) continue;

        char request[1024] = {0};
        recv(client_fd, request, sizeof(request) - 1, 0);

        if (strncmp(request, "GET /api/metrics", 16) == 0) {
            SystemMetrics m = get_system_metrics();
            char json_body[2048];
            format_json(m, json_body, sizeof(json_body));

            char response[2560];
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Connection: close\r\n\r\n%s",
                     json_body);
            send(client_fd, response, (int)strlen(response), 0);
        } else if (strncmp(request, "POST /api/kill", 14) == 0) {
            char *pid_str = strstr(request, "pid=");
            if (pid_str) {
                int pid = atoi(pid_str + 4);
                kill_process_by_pid(pid);
            }
            const char *resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK";
            send(client_fd, resp, (int)strlen(resp), 0);
        } else {
            send(client_fd, HTML_DASHBOARD, (int)strlen(HTML_DASHBOARD), 0);
        }
        CLOSE_SOCKET(client_fd);
    }
    CLOSE_SOCKET(server_fd);
#if defined(_WIN32)
    WSACleanup();
#endif
}
