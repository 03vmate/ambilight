cmake_minimum_required(VERSION 3.10)
project(lhf-tvb)

include(FindLibJpegTurbo.cmake)
find_package(OpenCV REQUIRED)

set(SOURCES
    main.cpp
)


add_executable(lhf-tvb ${SOURCES})
target_include_directories(lhf-tvb PRIVATE ${LibJpegTurbo_INCLUDE_DIRS})
target_link_libraries(lhf-tvb ${LibJpegTurbo_LIBRARIES} ${OpenCV_LIBS})