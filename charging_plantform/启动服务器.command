#!/bin/bash
# 双击启动业务服务器（监听 0.0.0.0:9000，供本机与局域网客户端连接）
# 关闭本窗口或按 Ctrl+C 即停止服务
cd "$(cd "$(dirname "$0")" && pwd)/server/build/Qt_6_11_2_for_macOS_Debug" || exit 1
exec ./ChargingServer
