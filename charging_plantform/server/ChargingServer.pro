# 东软充电桩平台 - 服务器(headless，无 GUI)
# 提供 TCP 业务服务(默认9000)，PC管理端通过 ADMIN_* 命令远程接入
# 注意：用“QT =”赋值覆盖默认的 core gui，确保纯控制台、不链接 QtGui
QT = core network sql
CONFIG += console c++17
CONFIG -= app_bundle

TARGET = ChargingServer
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/src $$PWD/src/common

SOURCES += \
    src/main.cpp \
    src/common/DbManager.cpp \
    src/common/AliyunSms.cpp \
    src/common/TcpServer.cpp \
    src/common/ConnectionHandler.cpp \
    src/common/SessionManager.cpp \
    src/service/UserService.cpp \
    src/service/StationService.cpp \
    src/service/OrderService.cpp \
    src/service/AdminService.cpp \
    src/service/PileService.cpp \
    src/service/SalesService.cpp

HEADERS += \
    src/common/ApiDefs.h \
    src/common/AliyunSms.h \
    src/common/DbManager.h \
    src/common/TcpServer.h \
    src/common/ConnectionHandler.h \
    src/common/SessionManager.h \
    src/service/UserService.h \
    src/service/StationService.h \
    src/service/OrderService.h \
    src/service/AdminService.h \
    src/service/PileService.h \
    src/service/SalesService.h
