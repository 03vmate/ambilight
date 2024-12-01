cmake_minimum_required(VERSION 3.10)
project(ambilight)
include(FindLibJpegTurbo.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native")


set(SOURCES
        main.cpp
        ConfigParser.h
        SerialPort.cpp
        SerialPort.hpp
        colorOfBlock.hpp
        colorOfBlock.cpp
        v4l2mode.hpp
        v4l2mode.cpp
        networkmode.hpp
        networkmode.cpp
        averager.h
        ConfigParser.cpp
        V4L2Capture.cpp
        V4L2Capture.h
)


add_executable(ambilight ${SOURCES})
target_include_directories(ambilight PRIVATE ${TurboJPEG_INCLUDE_DIRS})
target_link_libraries(ambilight ${TurboJPEG_LIBRARIES})
