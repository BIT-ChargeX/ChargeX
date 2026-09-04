cmake_minimum_required(VERSION 3.16)
project(ChargingAdmin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Widgets Network QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network)
endif()

if(Qt6_FOUND)
    set(QT_LIBRARY_TARGETS Qt6::Widgets Qt6::Network)
else()
    set(QT_LIBRARY_TARGETS Qt5::Widgets Qt5::Network)
endif()

file(GLOB_RECURSE ADMIN_SOURCES
    src/*.cpp
    src/*.h
)

add_executable(ChargingAdmin ${ADMIN_SOURCES})

target_include_directories(ChargingAdmin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ChargingAdmin PRIVATE ${QT_LIBRARY_TARGETS})
