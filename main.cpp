#include <fstream>

#include "http/HttpResponse.h"
#include "http/HttpServer.h"

int main() {
    HttpServer server(8080);
    // server.set_callback([](const HttpRequest& request, HttpResponse& response) {
    //     response.set_body("Hello World!");
    // });
    server.start_server();
    server.stop_server();
    return 0;
}
