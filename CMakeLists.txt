cmake_minimum_required(VERSION 3.16)

project(cpp-socket)

if (UNIX)
    add_compile_definitions(_LINUX)
elseif (WIN32)
    add_compile_definitions(_WIN32)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_SOURCE client.cxx server.cxx)

include(GNUInstallDirs)

add_library(${PROJECT_NAME} SHARED ${LIB_SOURCE})
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "client.hxx;server.hxx")

find_package(Threads)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

find_package(fmt)
target_link_libraries(${PROJECT_NAME} fmt::fmt)

find_library(CLSOCKET clsocket)
target_link_libraries(${PROJECT_NAME} clsocket)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

file(GLOB files "cmd/*.cxx")
foreach (file ${files})
    get_filename_component(file_name ${file} NAME_WE)
    add_executable("${PROJECT_NAME}-${file_name}" ${file})
    target_link_libraries("${PROJECT_NAME}-${file_name}" PRIVATE ${PROJECT_NAME})
    target_link_libraries("${PROJECT_NAME}-${file_name}" PRIVATE fmt::fmt)
endforeach ()
