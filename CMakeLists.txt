cmake_minimum_required(VERSION 3.20)
project(http_server)
set(CMAKE_C_STANDARD 99)

add_compile_options(-g)

file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/main.c
    ${CMAKE_SOURCE_DIR}/src/http_parser.c
    ${CMAKE_SOURCE_DIR}/src/socket.c
    ${CMAKE_SOURCE_DIR}/src/threading.c
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
