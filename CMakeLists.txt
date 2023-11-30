cmake_minimum_required(VERSION 3.10)
project(lhf-tvb)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")

include(FindLibJpegTurbo.cmake)
find_package(OpenCV REQUIRED)

set(SOURCES
        main.cpp
        simpleConfigParser.h
        SerialPort.cpp
        SerialPort.hpp
        v4l2mode.hpp
)


add_executable(lhf-tvb ${SOURCES})
target_include_directories(lhf-tvb PRIVATE ${LibJpegTurbo_INCLUDE_DIRS})
target_link_libraries(lhf-tvb ${LibJpegTurbo_LIBRARIES} ${OpenCV_LIBS})