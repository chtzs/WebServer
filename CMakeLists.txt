cmake_minimum_required(VERSION 3.22.1)
project(WebServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(ThirdParty/StringZilla ${CMAKE_BINARY_DIR}/ThirdParty/StringZilla)

set(WebServer_PUBLIC_HEADERS
        include/webserver/log/Logger.h
        include/webserver/tcp/TcpServer.h
        include/webserver/tcp/multiplexing/MultiplexingLinux.h
        include/webserver/tcp/multiplexing/Multiplexing.h
        include/webserver/tcp/multiplexing/MultiplexingWindows.h
        include/webserver/tcp/multiplexing/SocketPool.h
        include/webserver/thread_pool/ThreadPool.h
        include/webserver/thread_pool/Task.h
        include/webserver/thread_pool/Worker.h
        include/webserver/thread_pool/SharedContext.h
        include/webserver/http/HttpRequest.h
        include/webserver/http/HttpRequestParser.h
        include/webserver/http/HttpResponse.h
        include/webserver/http/HttpStatus.h
        include/webserver/http/HttpServer.h
        include/webserver/http/HttpRange.h
        include/webserver/common/Predefined.h
        include/webserver/common/SafeQueue.h
        include/webserver/common/SafeMap.h
        include/webserver/common/SendQueue.h
        include/webserver/common/FileReader.h
        include/webserver/common/UrlHelper.h
        include/webserver/common/FileSystem.h)

set(WebServer_SOURCES
        src/thread_pool/ThreadPool.cpp
        src/thread_pool/Task.cpp
        src/thread_pool/Worker.cpp
        src/tcp/multiplexing/MultiplexingLinux.cpp
        src/tcp/TcpServer.cpp)

set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/webserver")
# ************** For Static Library ************** #
add_library(WebServer ${WebServer_SOURCES} ${WebServer_PUBLIC_HEADERS})

target_include_directories(
        WebServer
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if (WIN32)
    target_link_libraries(WebServer libws2_32.a)
    target_link_libraries(WebServer MSWSOCK.DLL)
endif ()

target_link_libraries(WebServer StringZilla)
target_compile_definitions(WebServer PUBLIC WEBSERVER_STATIC_DEFINE)

# ************** For Executable File ************** #
add_executable(WebServerExecutable main.cpp ${WebServer_SOURCES} ${WebServer_PUBLIC_HEADERS})

target_include_directories(WebServerExecutable PRIVATE ${INCLUDE_DIR})
if (WIN32)
    target_link_libraries(WebServerExecutable libws2_32.a)
    target_link_libraries(WebServerExecutable MSWSOCK.DLL)
endif ()
target_link_libraries(WebServerExecutable StringZilla)

set_target_properties(WebServerExecutable PROPERTIES OUTPUT_NAME "WebServer")

# ************** For Installation ************** #

install(TARGETS WebServer StringZilla
        EXPORT WebServerTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

#install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
#        DESTINATION include)

# 写入库的版本信息
include(CMakePackageConfigHelpers)
set(PACKAGE_VERSION ${PROJECT_VERSION})

write_basic_package_version_file(
        WebServerConfigVersion.cmake
        VERSION ${PACKAGE_VERSION}
        COMPATIBILITY AnyNewerVersion  # 表示该函数库向下兼容
)
install(EXPORT WebServerTargets
        FILE WebServerTargets.cmake
        NAMESPACE WebServer::
        DESTINATION lib/cmake/WebServer
)

configure_file(WebServerConfig.cmake.in WebServerConfig.cmake @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/WebServerConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/WebServerConfigVersion.cmake"
        DESTINATION lib/cmake/WebServer
)