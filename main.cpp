#include <unistd.h>
#include "server/webserver.h"

int main() {
    WebServer server(
        1317, 3, 60000, false,
        3306, "root", "123456", "webserver",
        12, 6);
    server.start();
} 