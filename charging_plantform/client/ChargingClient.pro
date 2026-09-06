# 充电用户端（Qt Creator 直接打开本文件即可）
QT += core gui widgets network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
TARGET = ChargingClient
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

# 存在 QWebEngine 模块时启用腾讯地图导航页，否则自动降级
qtHaveModule(webenginewidgets) {
    QT += webenginewidgets
    DEFINES += USE_QT_WEBENGINE
}

INCLUDEPATH += $$PWD/src

SOURCES += \
    src/main.cpp \
    src/HomeWindow.cpp \
    src/common/NetClient.cpp \
    src/common/AppSession.cpp \
    src/common/MapApi.cpp \
    src/account/LoginWidget.cpp \
    src/account/ProfileWidget.cpp \
    src/account/RechargeWidget.cpp \
    src/account/RechargeRecordsWidget.cpp \
    src/account/PointsWidget.cpp \
    src/station_nav/StationListWidget.cpp \
    src/station_nav/StationDetailWidget.cpp \
    src/station_nav/NavWidget.cpp \
    src/charging/ChargingFlowWidget.cpp \
    src/charging/SettlementWidget.cpp

HEADERS += \
    src/HomeWindow.h \
    src/common/ApiDefs.h \
    src/common/NetClient.h \
    src/common/AppSession.h \
    src/common/MapApi.h \
    src/account/LoginWidget.h \
    src/account/ProfileWidget.h \
    src/account/RechargeWidget.h \
    src/account/RechargeRecordsWidget.h \
    src/account/PointsWidget.h \
    src/station_nav/StationListWidget.h \
    src/station_nav/StationDetailWidget.h \
    src/station_nav/NavWidget.h \
    src/charging/ChargingFlowWidget.h \
    src/charging/SettlementWidget.h
