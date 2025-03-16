cmake_minimum_required(VERSION 3.20.2)
project(meeting_sdk_linux_sample VERSION 1.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find compilers - use GCC-12 if available, fallback to system default
find_program(GCC_12 "gcc-12")
find_program(GPP_12 "g++-12")

if(GCC_12 AND GPP_12)
    set(CMAKE_C_COMPILER ${GCC_12})
    set(CMAKE_CXX_COMPILER ${GPP_12})
    message(STATUS "Using GCC-12 compilers: ${CMAKE_C_COMPILER} and ${CMAKE_CXX_COMPILER}")
else()
    message(STATUS "GCC-12 not found, using system default")
endif()

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(ZOOM_SDK lib/zoomsdk)

# Make sure OpenSSL is found
find_package(OpenSSL REQUIRED)

find_package(ada REQUIRED)
find_package(CLI11 REQUIRED)
find_path(JWT_CPP_INCLUDE_DIRS "jwt-cpp/base.h")

find_package(X11 REQUIRED)
include_directories(${X11_INCLUDE_DIR})
link_directories(${X11_LIBRARIES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET glib-2.0)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

include_directories(${ZOOM_SDK}/h)
link_directories(${ZOOM_SDK} ${ZOOM_SDK})
link_directories(${ZOOM_SDK} ${ZOOM_SDK}/qt_libs/**)

add_executable(zoomsdk src/main.cpp
       src/Zoom.cpp
        src/Zoom.h
        src/Config.cpp
        src/Config.h
        src/util/Singleton.h
        src/util/Log.h
        src/events/AuthServiceEvent.cpp
        src/events/AuthServiceEvent.h
        src/events/MeetingServiceEvent.cpp
        src/events/MeetingServiceEvent.h
        src/events/MeetingReminderEvent.cpp
        src/events/MeetingReminderEvent.h
        src/events/MeetingRecordingCtrlEvent.cpp
        src/events/MeetingRecordingCtrlEvent.h
        src/raw_record/ZoomSDKAudioRawDataDelegate.cpp
        src/raw_record/ZoomSDKAudioRawDataDelegate.h
        src/raw_record/ZoomSDKRendererDelegate.cpp
        src/raw_record/ZoomSDKRendererDelegate.h
        src/raw_send/ZoomSDKVideoSource.h
        src/raw_send/ZoomSDKVideoSource.cpp
        src/util/SocketServer.h
        src/util/SocketServer.cpp
)

target_include_directories(zoomsdk PRIVATE ${JWT_CPP_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
target_link_libraries(zoomsdk PRIVATE meetingsdk ada::ada CLI11::CLI11 PkgConfig::deps ${OpenCV_LIBS} ${X11_LIBRARIES} ${OPENSSL_LIBRARIES})