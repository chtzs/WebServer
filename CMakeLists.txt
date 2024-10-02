cmake_minimum_required(VERSION 3.22.1)
project(WebServer)

set(CMAKE_CXX_STANDARD 17)

add_executable(WebServer main.cpp
        http/TcpServer.cpp
        http/TcpServer.h
        log/Logger.cpp
        log/Logger.h
        thread_pool/ThreadPool.cpp
        thread_pool/ThreadPool.h
        thread_pool/Task.cpp
        thread_pool/Task.h
        thread_pool/Worker.cpp
        thread_pool/Worker.h
        thread_pool/SharedContext.h
        thread_pool/SafeQueue.h
        http/MultiplexingLinux.cpp
        http/MultiplexingLinux.h
        common/Predefined.h)

if (WIN32)
    target_link_libraries(WebServer libwsock32.a)
endif ()
