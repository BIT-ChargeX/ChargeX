#!/bin/bash
# 双击启动充电用户端（登录 / 找桩 / 充电 / 我的）
# 默认连本机 127.0.0.1:9000；如需连其他服务器，取消下一行注释并改 IP
export CHARGING_SERVER_HOST=10.194.77.247
cd "$(cd "$(dirname "$0")" && pwd)/client/build/Qt_6_11_2_for_macOS_Debug" || exit 1
open ChargingClient.app
