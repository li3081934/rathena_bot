# Bot 系统优化与迭代计划

> 基于当前 `bot_ctrl.cpp` + `npc/bot/bot_manager.txt` 实现的分析

---

## 现有功能清单

| 功能 | 文件位置 | 说明 |
|------|----------|------|
| 创建 Bot | `npc/bot/bot_manager.txt:58-71` | 初心者或服事 Bot |
| 摧毁 Bot | `npc/bot/bot_manager.txt` | 删除随从 |
| 转职 | `npc/bot/bot_manager.txt:73-97` | 6 种一转 / 12 种二转 |
| 加点 | `Bot_Control_Crystal` 菜单 | 分配属性点 |
| 学技能 | `Bot_Control_Crystal` 菜单 | 学习技能 |
| AI 模式切换 | `Bot_Control_Crystal` 菜单 | 跟随/支援/待机 |
| 召回 | `Bot_Control_Crystal` 菜单 | 传送 Bot 到身边 |
| 复活 | `Bot_Control_Crystal` 菜单 | 复活死亡的 Bot |
| 自动 Buff | `bot_ctrl.cpp:108-122` | 加速 + 天赐 |
| 自动跟随 | `bot_ctrl.cpp:131-151` | 保持在主人 3 格范围 |

---

## 优化与迭代方向

### 1. AI 系统大幅增强（P0）

**现状问题：**
- `bot_ctrl.cpp:123-129` 自动治疗逻辑被注释掉，Bot **不会战斗、不会治疗、不会使用物品**
- AI 仅有 `AI_FOLLOW / AI_SUPPORT / AI_STANDBY` 三种状态，SUPPORT 模式下除上 Buff 外不做任何事
- 与现有宠物/生命体/佣兵系统相比，Bot AI 过于简陋

**建议实现：**
- **战斗 AI**：让 Bot 主动攻击主人正在攻击的目标，根据职业使用对应技能
- **智能治疗**：实现被注释的 `status_heal` 逻辑，根据 HP 阈值自动使用治疗技能或物品
- **物品使用**：Bot 可自动使用恢复道具（白水、蓝水、纤维药水等）
- **技能策略**：按职业配置决策树
  - 牧师系：自动治疗、复活、霸邪
  - 法师系：自动释放属性魔法
  - 骑士系：嘲讽/拉怪
  - 猎人系：远程风筝
- **仇恨系统**：骑士/十字军 Bot 可嘲讽拉怪保护主人

**涉及文件：** `src/map/bot_ctrl.cpp`

---

### 2. 职业与技能扩展（P1）

**现状问题：**
- 只支持到二转，不支持进阶二转、三转、扩展职业
- `bot_manager.txt:73-97` 转职列表仅 6 个一转 + 12 个二转
- 创建时只有"初心者"和"服事"两个选项

**建议实现：**
- 支持所有职业：进阶二转、三转、扩展职业（忍者/枪手/拳圣/超初等）
- 转职后自动配置对应职业技能
- 按职业预设初始技能分配方案
- 创建界面增加更多初始职业选项

**涉及文件：** `npc/bot/bot_manager.txt`, `src/map/bot_ctrl.cpp`

---

### 3. Bot 养成系统（P2）

**建议实现：**
- **独立经验系统**：Bot 战斗获得独立经验，有独立的等级成长
- **装备系统**：Bot 可穿戴装备（需在 `bot_ctrl` 实现装备数据加载和属性加成）
- **进化系统**：类似生命体，达成条件（等级、好感度、任务）后进化更强形态
- **个性系统**：生成时随机获得性格特质，影响 AI 行为
  - "勇敢"：增加攻击频率
  - "谨慎"：更注重治疗和保命
  - "冷静"：优先使用控制技能

**涉及文件：** `src/map/bot_ctrl.cpp`, `db/re/item_db_bot.yml`, 新增数据库表

---

### 4. 多 Bot 与编队系统（P3）

**建议实现：**
- 允许玩家拥有多个 Bot（当前限制 1 个）
- 编队系统：一键切换不同 Bot 配置（PVE 队、PVP 队、挂机队）
- 多 Bot 协同：多个 Bot 同时在场时的战术配合
- Bot 预设方案保存/加载

**涉及文件：** `src/map/bot_ctrl.cpp`, 数据库 schema

---

### 5. 配置可定制化（P0）

**现状问题：**
- Bot 参数在 `bot_ctrl.cpp` 中**硬编码**：
  - HP 治疗阈值
  - Buff 触发条件
  - 跟随距离（3 格）
  - AI 思考间隔（500ms）

**建议实现：**
- 新建 `conf/battle/bot.conf` 配置文件
- 所有 Bot 行为参数可通过配置文件调整
- 增加 `@botconfig` GM 命令动态调整
- 支持每只 Bot 独立 AI 参数配置

**涉及文件：** `src/map/bot_ctrl.cpp`, `conf/battle/bot.conf`（新）

---

### 6. 管理界面增强（P2）

**现状问题：**
- 所有操作通过 `Bot_Control_Crystal`（物品 ID 61001）的菜单进行
- 没有直观的状态展示

**建议实现：**
- 增加 NPC 对话管理界面（Prontera Bot 管理员增加更多交互）
- Bot 状态窗口（HP/SP/等级/职业/技能/装备等）
- 快捷指令：`/bot follow`、`/bot attack`、`/bot heal`、`/bot stay`
- Bot 信息浮动显示（类似佣兵的血条和状态图标）

**涉及文件：** `npc/bot/bot_manager.txt`, `src/map/bot_ctrl.cpp`, `src/map/clif.cpp`

---

### 7. Web API 集成（P3）

**现状：**
- 已有 `web-server` 提供 REST API（`charconfig`、`emblem`、`merchantstore`）
- Bot 系统完全无 Web 接口

**建议实现：**
- 增加 Bot 管理 REST API：
  - `GET /api/bot/{account_id}` — 查询 Bot 状态
  - `POST /api/bot/{account_id}/command` — 下发指令
  - `PUT /api/bot/{account_id}/config` — 调整配置
  - `GET /api/bot/{account_id}/inventory` — 查看装备
- 可对接外部工具、Mobile App、Web Dashboard

**涉及文件：** `src/web/` 下新增 controller

---

### 8. 数据库与持久化增强（P1）

**现状问题：**
- Bot 数据通过 `#BOT_<account_id>` 账户存在 MySQL `login`/`char` 表中
- 没有独立的 Bot 数据表，数据结构混杂
- 全部使用 MyISAM 引擎（不支持事务、无外键约束）

**建议实现：**
- 创建独立 `bot` 表：
  ```sql
  CREATE TABLE `bot` (
    `bot_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `account_id` INT UNSIGNED NOT NULL,
    `char_id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(32) NOT NULL,
    `class` INT UNSIGNED NOT NULL,
    `level` INT UNSIGNED NOT NULL DEFAULT 1,
    `hp` INT NOT NULL,
    `sp` INT NOT NULL,
    `str` INT NOT NULL DEFAULT 1,
    `agi` INT NOT NULL DEFAULT 1,
    `vit` INT NOT NULL DEFAULT 1,
    `int` INT NOT NULL DEFAULT 1,
    `dex` INT NOT NULL DEFAULT 1,
    `luk` INT NOT NULL DEFAULT 1,
    `ai_mode` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `ai_config` BLOB,
    `inventory` BLOB,
    `equipment` BLOB,
    `create_date` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`bot_id`),
    INDEX `account_id` (`account_id`)
  ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
  ```
- 迁移现有 Bot 数据到新表
- 使用 InnoDB 引擎（新表）

**涉及文件：** `sql-files/main.sql`, `src/map/bot_ctrl.cpp`

---

### 9. 稳定性与安全性优化（P2）

**建议实现：**
- **路径优化**：`path_search` 调用增加缓存，减少寻路计算开销
- **跨地图处理**：主人换地图时 Bot 自动跟随（当前缺少自动召回机制）
- **防滥用**：
  - 防止 Bot 卡地形导致服务器 Tick 消耗过高
  - 防止无限刷物品
  - Bot 操作频率限制
- **断线重连**：主人断线重连后 Bot 状态恢复
- **Bot 上限控制**：每地图/每账号可配置 Bot 数量上限

**涉及文件：** `src/map/bot_ctrl.cpp`, `src/map/path.cpp`

---

### 10. 测试体系（P3）

**现状：**
- **完全无单元测试或集成测试**
- CI 仅做编译检查

**建议实现：**
- 为 `bot_ctrl` 核心函数（创建、销毁、AI 逻辑）增加单元测试
- NPC 脚本自动化测试框架
- CI 中增加测试步骤

---

## 优先级汇总

| 优先级 | 方向 | 工作量 | 影响 |
|--------|------|--------|------|
| 🔴 P0 | AI 战斗 + 治疗 | 中 | 解决最大功能缺口 |
| 🔴 P0 | 配置参数外部化 | 小 | 提升可维护性 |
| 🟡 P1 | 职业扩展到三转 | 中 | 跟上版本 |
| 🟡 P1 | 独立 Bot 数据表 | 中 | 更好的扩展基础 |
| 🟢 P2 | 养成系统 | 大 | 增加长期可玩性 |
| 🟢 P2 | 管理界面增强 | 中 | 提升用户体验 |
| 🟢 P2 | 稳定性优化 | 中 | 系统健壮性 |
| 🔵 P3 | 多 Bot / 编队 | 大 | 高阶功能 |
| 🔵 P3 | Web API | 中 | 外部集成 |
| 🔵 P3 | 测试体系 | 大 | 长期工程质量 |

---

## 涉及文件清单

```
src/map/bot_ctrl.cpp       — Bot 核心控制（C++）
src/map/bot_ctrl.hpp       — Bot 数据结构和常量
npc/bot/bot_manager.txt    — Bot 管理 NPC 脚本
db/re/item_db_bot.yml      — Bot Control Crystal 物品定义
npc/scripts_custom.conf    — 脚本加载配置
sql-files/main.sql         — 数据库表定义
conf/battle/               — 新配置文件目录
src/web/                   — Web API（可选）
```
