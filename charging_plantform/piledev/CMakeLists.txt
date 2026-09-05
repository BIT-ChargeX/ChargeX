cmake_minimum_required(VERSION 3.16)
project(ChargingPileSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Network QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Network)
endif()

if(Qt6_FOUND)
    set(QT_LIBRARY_TARGETS Qt6::Network)
else()
    set(QT_LIBRARY_TARGETS Qt5::Network)
endif()

file(GLOB_RECURSE SIM_SOURCES
    src/*.cpp
    src/*.h
)

add_executable(ChargingPileSim ${SIM_SOURCES})

target_include_directories(ChargingPileSim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ChargingPileSim PRIVATE ${QT_LIBRARY_TARGETS})
