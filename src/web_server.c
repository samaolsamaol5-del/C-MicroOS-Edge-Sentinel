

static int is_valid_tg_token(const char *s) {
    if (!s || *s == '\0') return 0;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              c == ':' || c == '_' || c == '-')) {
            return 0;
        }
    }
    return 1;
}

static int is_valid_tg_chat_id(const char *s) {
    if (!s || *s == '\0') return 0;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              c == '_' || c == '-' || c == '@')) {
            return 0;
        }
    }
    return 1;
}

static void send_telegram_alert(double cpu, double ram) {
    if (!is_valid_tg_token(TELEGRAM_BOT_TOKEN) || !is_valid_tg_chat_id(TELEGRAM_CHAT_ID)) {
        return;
    }

    time_t now = time(NULL);
    if (now - last_alert_time < 300) return;

    char cmd[1024];
#if defined(_WIN32)
    int written = snprintf(
        cmd, sizeof(cmd),
        "curl.exe -s -m 5 -X POST \"https://api.telegram.org/bot%s/sendMessage\" "
        "--data-urlencode \"chat_id=%s\" "
        "--data-urlencode \"text=C-MicroOS Pro Alert\nHigh Resource Usage!\nCPU: %.1f%%\nRAM: %.1f%%\" > NUL 2>&1",
        TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID, cpu, ram
    );
#else
    int written = snprintf(
        cmd, sizeof(cmd),
        "curl -s -m 5 -X POST 'https://api.telegram.org/bot%s/sendMessage' "
        "--data-urlencode 'chat_id=%s' "
        "--data-urlencode 'text=C-MicroOS Pro Alert\nHigh Resource Usage!\nCPU: %.1f%%\nRAM: %.1f%%' > /dev/null 2>&1",
        TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID, cpu, ram
    );
#endif

    if (written > 0 && (size_t)written < sizeof(cmd)) {
        int rc = system(cmd);
        if (rc == 0) {
            last_alert_time = now;
        }
    }
}
