cmake_minimum_required(VERSION 3.16)
project(DakshTorrent VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pthread")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
endif()

set(COMMON_SOURCES
    common/Net.cpp
    common/Utilities.cpp
)

add_executable(tracker
    tracker/main.cpp
    tracker/TrackerServer.cpp
    tracker/PeerManager.cpp
    tracker/FileManager.cpp
    tracker/SwarmManager.cpp
    tracker/HeartbeatMonitor.cpp
    ${COMMON_SOURCES}
)

add_executable(peer
    peer/main.cpp
    peer/Peer.cpp
    peer/TrackerClient.cpp
    peer/UploadServer.cpp
    peer/DownloadManager.cpp
    peer/Scheduler.cpp
    peer/PieceManager.cpp
    peer/HashManager.cpp
    peer/ResumeManager.cpp
    peer/FileAssembler.cpp
    peer/StatsManager.cpp
    peer/Dashboard.cpp
    ${COMMON_SOURCES}
)

target_include_directories(tracker PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/common)
target_include_directories(peer PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/common)

target_link_libraries(tracker PRIVATE ws2_32)
target_link_libraries(peer PRIVATE ws2_32)

if(MSVC)
    target_compile_options(tracker PRIVATE /W4)
    target_compile_options(peer PRIVATE /W4)
endif()
