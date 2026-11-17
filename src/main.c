#include "os_pulse_core.h"
#include <string.h>

int main(int argc, char *argv[]) {
    get_system_metrics();
    sleep_ms(200);

    if (argc > 1) {
        if (strcmp(argv[1], "--json") == 0) {
            print_json(get_system_metrics());
            return 0;
        } else if (strcmp(argv[1], "--web") == 0) {
            int port = (argc > 2) ? atoi(argv[2]) : 8080;
            start_web_server(port);
            return 0;
        }
    }

    while (1) {
        print_dashboard(get_system_metrics());
        sleep_ms(1000);
    }
    return 0;
}
