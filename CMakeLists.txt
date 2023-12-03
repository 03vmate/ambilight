cmake_minimum_required(VERSION 3.10)
project(lhf-tvb)
include(FindLibJpegTurbo.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native")


set(SOURCES
        main.cpp
        simpleConfigParser.h
        SerialPort.cpp
        SerialPort.hpp
        v4l2mode.hpp
        networkmode.hpp
        averager.h
)


add_executable(lhf-tvb ${SOURCES})
target_include_directories(lhf-tvb PRIVATE ${TurboJPEG_INCLUDE_DIRS})
target_link_libraries(lhf-tvb ${TurboJPEG_LIBRARIES})
