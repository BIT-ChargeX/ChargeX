# PC 管理端（PCAdmin） - Linux + Qt
# 充电用户端与管理端均为“客户端”，业务统一由 ChargingServer 处理；
# 本程序无任何数据库访问，全部操作通过 TCP + JSON 协议远程请求服务器。
QT += core gui widgets network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
TARGET = ChargingAdmin
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/src

SOURCES += \
    src/main.cpp \
    src/LoginDialog.cpp \
    src/MainWindow.cpp \
    src/common/NetClient.cpp \
    src/common/AdminSession.cpp \
    src/common/Theme.cpp \
    src/common/SimpleCharts.cpp \
    src/common/NavRail.cpp \
    src/pages/UserMgmtWidget.cpp \
    src/pages/StationMgmtWidget.cpp \
    src/pages/PileWidget.cpp \
    src/pages/DeviceRuntimeWidget.cpp \
    src/pages/MonitorWidget.cpp \
    src/pages/SalesWidget.cpp

HEADERS += \
    src/LoginDialog.h \
    src/MainWindow.h \
    src/common/ApiDefs.h \
    src/common/NetClient.h \
    src/common/AdminSession.h \
    src/common/Theme.h \
    src/common/SimpleCharts.h \
    src/common/NavRail.h \
    src/pages/UserMgmtWidget.h \
    src/pages/StationMgmtWidget.h \
    src/pages/PileWidget.h \
    src/pages/DeviceRuntimeWidget.h \
    src/pages/MonitorWidget.h \
    src/pages/SalesWidget.h
