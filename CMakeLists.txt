cmake_minimum_required(VERSION 3.22.1)
project(WebServer)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(ThirdParty/StringZilla ${CMAKE_BINARY_DIR}/ThirdParty/StringZilla)

add_executable(WebServer main.cpp
        log/Logger.cpp
        log/Logger.h
        thread_pool/ThreadPool.cpp
        thread_pool/ThreadPool.h
        thread_pool/Task.cpp
        thread_pool/Task.h
        thread_pool/Worker.cpp
        thread_pool/Worker.h
        thread_pool/SharedContext.h
        tcp/multiplexing/MultiplexingLinux.cpp
        tcp/multiplexing/MultiplexingLinux.h
        tcp/multiplexing/Multiplexing.h
        tcp/multiplexing/MultiplexingWindows.h
        tcp/multiplexing/SocketPool.h
        tcp/TcpServer.cpp
        tcp/TcpServer.h
        http/HttpRequest.h
        http/HttpRequestParser.h
        http/HttpResponse.h
        http/HttpStatus.h
        http/HttpServer.h
        common/Predefined.h
        common/SafeQueue.h
        common/SafeMap.h
        common/SendQueue.h
        common/FileReader.h
        common/UrlHelper.h
        http/HttpRange.h
        common/FileSystem.h)
if (WIN32)
    #    target_link_libraries(WebServer libwsock32.a)
    target_link_libraries(WebServer libws2_32.a)
    target_link_libraries(WebServer MSWSOCK.DLL)
endif ()

target_link_libraries(WebServer StringZilla)