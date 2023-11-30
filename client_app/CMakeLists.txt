cmake_minimum_required(VERSION 3.27)
project(client_app)
find_package(X11 REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_executable(client_app main.cpp simpleConfigParser.h)
target_include_directories(client_app PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(client_app PRIVATE ${X11_LIBRARIES})