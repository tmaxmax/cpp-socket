cmake_minimum_required(VERSION 3.28)

project(termchat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)

add_library("${PROJECT_NAME}-socket" STATIC socket.cpp)
set_target_properties("${PROJECT_NAME}-socket" PROPERTIES PUBLIC_HEADER "socket.h")

add_library("${PROJECT_NAME}-proto" STATIC protocol.cpp)
set_target_properties("${PROJECT_NAME}-proto" PROPERTIES PUBLIC_HEADER "protocol.h")

add_executable("${PROJECT_NAME}-server" server.cpp)
target_link_libraries("${PROJECT_NAME}-server" "${PROJECT_NAME}-socket")
target_link_libraries("${PROJECT_NAME}-server" "${PROJECT_NAME}-proto")

add_executable("${PROJECT_NAME}-client" client.cpp)
target_link_libraries("${PROJECT_NAME}-client" "${PROJECT_NAME}-socket")
target_link_libraries("${PROJECT_NAME}-client" "${PROJECT_NAME}-proto")