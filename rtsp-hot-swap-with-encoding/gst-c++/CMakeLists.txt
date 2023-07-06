cmake_minimum_required(VERSION 3.16.3)
project(gst_rtsp_hot_swap)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED) #finding pkg-config is a helper tool

#using pkg-config to getting Gstreamer
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-video-1.0 gstreamer-audio-1.0)

#including GStreamer header files directory
include_directories(
        ${GLIB_INCLUDE_DIRS}
        ${GSTREAMER_INCLUDE_DIRS}
)

#linking GStreamer library directorystreaming
link_directories(
        ${GLIB_INCLUDE_DIRS}
        ${GSTREAMER_LIBRARY_DIRS}
)

#linking GStreamer libraries
link_libraries(
        ${GLIB_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
)

add_executable(${PROJECT_NAME} main.cpp)