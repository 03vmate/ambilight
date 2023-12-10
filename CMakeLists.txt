cmake_minimum_required(VERSION 3.10)
project(ambilight)
include(FindLibJpegTurbo.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native")


set(SOURCES
        main.cpp
        simpleConfigParser.h
        SerialPort.cpp
        SerialPort.hpp
        v4l2mode.hpp
        v4l2mode.cpp
        networkmode.hpp
        networkmode.cpp
        averager.h
)


add_executable(ambilight ${SOURCES})
target_include_directories(ambilight PRIVATE ${TurboJPEG_INCLUDE_DIRS})
target_link_libraries(ambilight ${TurboJPEG_LIBRARIES})
