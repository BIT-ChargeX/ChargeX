# 充电桩模拟端 ChargingPileSim —— 模拟“真实物理世界”的充电桩终端
# 独立进程，作为第三个接入方经 TCP 连 ChargingServer：
#  HELLO 绑定电桩 → 每5s REPORT(状态/SOC/功率) → 取待办指令 → 执行 → RESULT 回执
QT = core network
CONFIG += console c++17
CONFIG -= app_bundle

TARGET = ChargingPileSim
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/src

SOURCES += \
    src/main.cpp \
    src/PileModel.cpp \
    src/SimClient.cpp

HEADERS += \
    src/PileModel.h \
    src/SimClient.h
