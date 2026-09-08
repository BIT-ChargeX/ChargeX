#!/bin/bash
# 双击启动充电桩模拟端（模拟 8 台桩，默认连本机 127.0.0.1:9000）
# 用法可选：ChargingPileSim [服务器IP] [端口] [device_id] [桩数]
cd "$(cd "$(dirname "$0")" && pwd)/piledev/build/Qt_6_11_2_for_macOS_Debug" || exit 1
exec ./ChargingPileSim
