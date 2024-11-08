#include "http/HttpResponse.h"
#include "http/HttpServer.h"

int get_or_default(std::unordered_map<sz::string, sz::string> &map, const sz::string &key) {
    sz::string value = "0";
    if (map.count(key) > 0) {
        value = map[key];
    }
    char *ptr = nullptr;
    return strtol(value.c_str(), &ptr, 10);
}

int main() {
    HttpServer server(8080);
    server.add_custom_request_callback("/api/add", [](HttpRequest &request, HttpResponse &response) {
        int a = get_or_default(request.parameters, "a");
        int b = get_or_default(request.parameters, "b");
        int value = a + b;
        response.set_body(std::to_string(value));
        response.insert("Content-Type", "text/html");
    });
    server.set_callback([&server](HttpRequest &request, HttpResponse &response) {
        std::cout << request.url << std::endl;
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
