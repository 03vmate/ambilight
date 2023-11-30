cmake_minimum_required(VERSION 3.10)
project(lhf-tvb)
include(FindLibJpegTurbo.cmake)

set(SOURCES
        main.cpp
        simpleConfigParser.h
        SerialPort.cpp
        SerialPort.hpp
        v4l2mode.hpp
        networkmode.hpp
)


add_executable(lhf-tvb ${SOURCES})
target_include_directories(lhf-tvb PRIVATE ${LibJpegTurbo_INCLUDE_DIRS})
target_link_libraries(lhf-tvb ${LibJpegTurbo_LIBRARIES})