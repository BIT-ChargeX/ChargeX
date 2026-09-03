# 需求 #15 — 核心业务数据存储

| 属性 | 内容 |
|------|------|
| 所属子系统 | 数据库端 |
| 需求类别 | 数据存储需求 |
| 开发技术 | C++ / Qt Creator / QSqlDatabase (QSQLITE) |

## 功能描述

数据库采用 SQLite 实现，通过 Qt 的 `QSqlDatabase`（驱动：QSQLITE）进行连接管理。系统启动时自动完成数据库初始化，建立 5 张核心数据表，并通过外键约束确保数据完整性。建表语句全部使用 `CREATE TABLE IF NOT EXISTS`，保证重复启动时不破坏已有数据。

首次启动时，系统检查 `admins` 表是否为空，若为空则自动写入默认管理员账号（`admin / 123456`）。

## 数据表结构

### users（用户表）

| 字段 | 类型 | 说明 |
|------|------|------|
| user_id | INTEGER PK | 自增主键 |
| phone | TEXT NOT NULL UNIQUE | 11 位手机号 |
| nickname | TEXT | 用户昵称 |
| avatar | BLOB | 头像二进制数据 |
| balance | REAL DEFAULT 0.00 | 钱包余额（元） |
| register_time | TEXT | 注册时间（ISO 8601） |
| status | INTEGER DEFAULT 1 | 1=正常，0=冻结 |

### admins（管理员表）

| 字段 | 类型 | 说明 |
|------|------|------|
| admin_id | INTEGER PK | 自增主键 |
| username | TEXT NOT NULL UNIQUE | 登录账号 |
| password | TEXT NOT NULL | 登录密码 |

### stations（充电站表）

| 字段 | 类型 | 说明 |
|------|------|------|
| station_id | INTEGER PK | 自增主键 |
| name | TEXT NOT NULL | 站点名称 |
| address | TEXT | 详细地址 |
| latitude | REAL | 纬度 |
| longitude | REAL | 经度 |
| total_piles | INTEGER DEFAULT 0 | 电桩总数 |
| online_rate | REAL DEFAULT 0.00 | 在线率 |

### chargers（充电桩表）

| 字段 | 类型 | 说明 |
|------|------|------|
| charger_id | INTEGER PK | 自增主键 |
| station_id | INTEGER FK | 所属充电站（→ stations） |
| type | TEXT | 快充 / 慢充 |
| power | REAL | 额定功率（kW） |
| status | INTEGER | 0=闲置，1=在用，2=故障 |
| total_count | INTEGER DEFAULT 0 | 累计充电次数 |
| total_duration | INTEGER DEFAULT 0 | 累计充电时长（秒） |

### orders（充电订单表）

| 字段 | 类型 | 说明 |
|------|------|------|
| order_id | INTEGER PK | 自增主键 |
| user_id | INTEGER FK | 下单用户（→ users） |
| charger_id | INTEGER FK | 使用电桩（→ chargers） |
| start_time | TEXT | 开始时间（ISO 8601） |
| end_time | TEXT | 结束时间（ISO 8601） |
| duration | INTEGER | 充电时长（秒） |
| energy | REAL | 充电电量（kWh） |
| amount | REAL | 费用（元） |
| status | INTEGER | 0=进行中，1=已完成，2=已取消 |

## 表间关联关系

| 父表 | 子表 | 关联字段 | 关系类型 | 业务含义 |
|------|------|----------|----------|----------|
| stations | chargers | station_id | 1 : N | 一个充电站包含多个充电桩 |
| users | orders | user_id | 1 : N | 一个用户可产生多条充电订单 |
| chargers | orders | charger_id | 1 : N | 一个充电桩可对应多条历史充电记录 |

## 前置条件

1. SQLite 驱动已正确集成至 Qt 项目（`QSqlDatabase`, `QSQLITE`）
2. 应用程序对数据库文件所在目录具有读写权限

## 输入

1. 系统启动信号（触发数据库初始化流程）
2. 各业务模块的 CRUD 操作请求

## 输出

1. 5 张核心数据表成功建立，默认管理员账号写入
2. 各业务模块 CRUD 操作的执行结果（成功/失败及数据集）

## 异常情况处理

| 异常场景 | 处理方式 |
|----------|----------|
| 数据库文件不存在 | SQLite 在首次连接时自动创建新数据库文件，随后正常执行建表语句 |
| 表结构已存在 | `CREATE TABLE IF NOT EXISTS` 保证幂等，跳过重复建表，已有数据完整保留 |
| 外键约束失败 | 返回 `SQLITE_CONSTRAINT` 错误码，业务层捕获后回滚事务并记录日志 |
| 磁盘空间不足 | SQLite 返回 `SQLITE_FULL`，业务层捕获后停止写操作并向上层报告错误 |

## UML 时序图

![需求17 UML时序图](17.png)

### 时序图参与者说明

| 参与者 | 说明 |
|--------|------|
| 业务处理模块 | 系统启动时触发初始化的核心逻辑层 |
| 数据库 | SQLite 数据库文件，负责建表与数据持久化 |
