#include "http/HttpResponse.h"
#include "http/HttpServer.h"

int main() {
    HttpServer server(8080);
    server.set_callback([&server](HttpRequest &request, HttpResponse &response) {
        if (request.url.ends_with(".json")) {
            response.set_body("{'name': 'Hello World!'}");
        } else {
            server.default_callback(request, response);
        }
    });
    server.start_server();
    server.stop_server();
    return 0;
}
