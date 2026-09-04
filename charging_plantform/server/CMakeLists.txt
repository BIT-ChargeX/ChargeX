cmake_minimum_required(VERSION 3.16)
project(ChargingServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Network Sql QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Network Sql)
endif()

if(Qt6_FOUND)
    set(QT_LIBRARY_TARGETS Qt6::Network Qt6::Sql)
else()
    set(QT_LIBRARY_TARGETS Qt5::Network Qt5::Sql)
endif()

file(GLOB_RECURSE SERVER_SOURCES
    src/*.cpp
    src/*.h
)

add_executable(ChargingServer ${SERVER_SOURCES})

target_include_directories(ChargingServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

target_link_libraries(ChargingServer PRIVATE ${QT_LIBRARY_TARGETS})
