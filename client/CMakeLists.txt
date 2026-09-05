cmake_minimum_required(VERSION 3.16)
project(ChargingClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt6 优先，找不到则回退 Qt5
find_package(Qt6 COMPONENTS Widgets Network QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network)
endif()

if(Qt6_FOUND)
    set(QT_LIBRARY_TARGETS Qt6::Widgets Qt6::Network)
    find_package(Qt6 COMPONENTS WebEngineWidgets QUIET)
    if(Qt6WebEngineWidgets_FOUND)
        list(APPEND QT_LIBRARY_TARGETS Qt6::WebEngineWidgets)
        set(WEBENGINE_DEFINE USE_QT_WEBENGINE)
    endif()
else()
    set(QT_LIBRARY_TARGETS Qt5::Widgets Qt5::Network)
    find_package(Qt5 COMPONENTS WebEngineWidgets QUIET)
    if(Qt5WebEngineWidgets_FOUND)
        list(APPEND QT_LIBRARY_TARGETS Qt5::WebEngineWidgets)
        set(WEBENGINE_DEFINE USE_QT_WEBENGINE)
    endif()
endif()

file(GLOB_RECURSE CLIENT_SOURCES
    src/*.cpp
    src/*.h
)

add_executable(ChargingClient ${CLIENT_SOURCES})

target_include_directories(ChargingClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ChargingClient PRIVATE ${QT_LIBRARY_TARGETS})

if(WEBENGINE_DEFINE)
    target_compile_definitions(ChargingClient PRIVATE ${WEBENGINE_DEFINE})
    message(STATUS "ChargingClient: QWebEngineWidgets 可用，导航页启用腾讯地图网页")
else()
    message(STATUS "ChargingClient: 未找到 QWebEngineWidgets，导航页使用降级模式")
endif()
