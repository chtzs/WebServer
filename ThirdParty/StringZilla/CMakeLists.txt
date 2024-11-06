cmake_minimum_required(VERSION 3.22.1)
project(StringZilla)

set(CMAKE_CXX_STANDARD 17)

add_library(StringZilla INTERFACE)

target_include_directories(StringZilla
        INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

install(DIRECTORY include/ DESTINATION include)