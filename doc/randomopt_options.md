# 随机属性（Random Option）词条说明

> 数据来源：`db/re/item_randomopt_db.yml`（Id / Option / Script 三列）。
> 本表只列“能出什么属性”。**具体数值不在此表**，由各 Group 的
> `MinValue~MaxValue` 掷出；`Chance` 是同池内权重（`1 = 0.01%`，
> `10000 = 100%`，同池按权重比，不是绝对概率）。
> 单件装备最多 **5 条**（`MAX_ITEM_RDM_OPT`，见 `src/common/mmo.hpp`）。
> 客户端显示名对照：`roBrowserLegacy/src/DB/Items/ItemRandomOptionTable.js`。
> 相关文件：`db/re/item_randomopt_group.yml`（组池）、
> `doc/sample/randomopt.txt`（脚本示例）、`src/map/mob.cpp`
> （`mob_setdropitem_option` 掷点入口）。
> 空号：86、173、174、204、205。
>
> 命名规律（属性/体型/近远/暴击四大家族通用）：
> `*_TARGET` = 造成的增伤，`*_USER` = 受到的减伤。

## 1. 基础面板（Id 1–24）

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 1 | VAR_MAXHPAMOUNT | 生命上限（数值） | |
| 2 | VAR_MAXSPAMOUNT | 法力上限（数值） | |
| 3 | VAR_STRAMOUNT | STR | |
| 4 | VAR_AGIAMOUNT | AGI | |
| 5 | VAR_VITAMOUNT | VIT | |
| 6 | VAR_INTAMOUNT | INT | |
| 7 | VAR_DEXAMOUNT | DEX | |
| 8 | VAR_LUKAMOUNT | LUK | |
| 9 | VAR_MAXHPPERCENT | 生命上限（百分比） | |
| 10 | VAR_MAXSPPERCENT | 法力上限（百分比） | |
| 11 | VAR_HPACCELERATION | HP 自然回复 | 脚本 `bHPrecovRate` |
| 12 | VAR_SPACCELERATION | SP 自然回复 | 脚本 `bSPrecovRate` |
| 13 | VAR_ATKPERCENT | ATK（百分比） | |
| 14 | VAR_MAGICATKPERCENT | MATK（百分比） | |
| 15 | VAR_PLUSASPD | ASPD（数值） | |
| 16 | VAR_PLUSASPDPERCENT | ASPD（百分比） | |
| 17 | VAR_ATTPOWER | 攻击力 | |
| 18 | VAR_HITSUCCESSVALUE | 命中 | |
| 19 | VAR_ATTMPOWER | 魔法攻击力 | |
| 20 | VAR_ITEMDEFPOWER | 物防 | |
| 21 | VAR_MDEFPOWER | 魔防 | |
| 22 | VAR_AVOIDSUCCESSVALUE | 闪避 | |
| 23 | VAR_PLUSAVOIDSUCCESSVALUE | 完美闪避 | |
| 24 | VAR_CRITICALSUCCESSVALUE | 暴击 | |

## 2. 十属性攻防（Id 25–85、193）

属性顺序：无 / 水 / 地 / 火 / 风 / 毒 / 圣 / 暗 / 念 / 不死。

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 25–34 | ATTR_TOLERACE_* | 该属性耐性 | 25=无，26=水，27=地，28=火，29=风，30=毒，31=圣，32=暗，33=念，34=不死 |
| 35 | ATTR_TOLERACE_ALLBUTNOTHING | 全属性耐性（除无属性） | |
| 36–45 | DAMAGE_PROPERTY_*_USER | 该属性物理减伤 | 顺序同上（36=无 … 45=不死） |
| 46–55 | DAMAGE_PROPERTY_*_TARGET | 该属性物理增伤 | 顺序同上 |
| 56–65 | MDAMAGE_PROPERTY_*_USER | 该属性魔法减伤 | 顺序同上 |
| 66–75 | MDAMAGE_PROPERTY_*_TARGET | 该属性魔法增伤 | 顺序同上 |
| 76–85 | BODY_ATTR_* | 防具属性赋予 | 76=无 … 85=不死，穿上变对应属性 |
| 193 | ATTR_TOLERACE_ALL | 全属性耐性 | 35 是“除无属性外”，193 含无属性 |

## 3. 种族（Id 87–146、194–217）

种族顺序：无形 / 不死 / 动物 / 植物 / 昆虫 / 鱼 / 恶魔 / 人 / 天使 / 龙。

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 87–96 | RACE_TOLERACE_* | 该种族减伤 | |
| 97–106 | RACE_DAMAGE_* | 该种族物理增伤 | |
| 107–116 | RACE_MDAMAGE_* | 该种族魔法增伤 | |
| 117–126 | RACE_CRI_PERCENT_* | 该种族暴击率 | |
| 127–136 | RACE_IGNORE_DEF_PERCENT_* | 无视该种族物防（百分比） | |
| 137–146 | RACE_IGNORE_MDEF_PERCENT_* | 无视该种族魔防（百分比） | |
| 194–203 | RACE_WEAPON_TOLERACE_* | 武器格挡该种族伤害 | 脚本 `bSubRace,BF_WEAPON` |
| 206–217 | RACE_*_PLAYER_HUMAN / _DORAM | 对玩家（人形/多拉姆）减伤/增伤/魔伤/暴击/无视双防 | 206=减伤人形，207=减伤多拉姆，208/209=增伤，210/211=魔伤，212/213=暴击，214/215=无视物防，216/217=无视魔防 |

## 4. 体型 / 阶级（Id 147–167）

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 147–148 | CLASS_DAMAGE_NORMAL / _BOSS_TARGET | 对普怪 / BOSS 增伤 | 147=普怪，148=BOSS |
| 149–150 | CLASS_DAMAGE_NORMAL / _BOSS_USER | 对普怪 / BOSS 减伤 | 脚本 `bSubClass`，149=普怪，150=BOSS |
| 151–152 | CLASS_MDAMAGE_NORMAL / _BOSS | 对普怪 / BOSS 魔伤 | 151=普怪，152=BOSS |
| 153–154 | CLASS_IGNORE_DEF_PERCENT_NORMAL / _BOSS | 无视普怪 / BOSS 物防 | 153=普怪，154=BOSS |
| 155–156 | CLASS_IGNORE_MDEF_PERCENT_NORMAL / _BOSS | 无视普怪 / BOSS 魔防 | 155=普怪，156=BOSS |
| 157–163 | DAMAGE_SIZE_* | 体型物理增减伤 | 157=小增伤，158=中增伤，159=大增伤，160=小减伤，161=中减伤，162=大减伤，163=完美体型（无视体型修正） |
| 164 | DAMAGE_CRI_TARGET | 暴击伤害 | 脚本 `bCritAtkRate` |
| 165 | DAMAGE_CRI_USER | 被暴击减伤 | 脚本 `bCritDefRate` |
| 166 | RANGE_ATTACK_DAMAGE_TARGET | 远程增伤 | 脚本 `bLongAtkRate` |
| 167 | RANGE_ATTACK_DAMAGE_USER | 远程减伤 | 脚本 `bLongAtkDef` |

## 5. 治疗 / 施法（Id 168–172）

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 168 | HEAL_VALUE | 治疗量 | 脚本 `bHealPower` |
| 169 | HEAL_MODIFY_PERCENT | 治疗修正（百分比） | 脚本 `bHealPower2` |
| 170 | DEC_SPELL_CAST_TIME | 变咏（减少吟唱时间） | |
| 171 | DEC_SPELL_DELAY_TIME | 延迟（减少技能后延迟） | |
| 172 | DEC_SP_CONSUMPTION | 耗蓝减少 | |

## 6. 武器属性 / 不坏（Id 175–186）

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 175–184 | WEAPON_ATTR_* | 武器属性赋予 | 175=无，176=水，177=地，178=火，179=风，180=毒，181=圣，182=暗，183=念，184=不死 |
| 185 | WEAPON_INDESTRUCTIBLE | 武器不坏 | |
| 186 | BODY_INDESTRUCTIBLE | 防具不坏 | |

## 7. 体型魔法（Id 187–192）

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 187–189 | MDAMAGE_SIZE_SMALL / MIDIUM / LARGE_TARGET | 小/中/大体型魔伤 | |
| 190–192 | MDAMAGE_SIZE_SMALL / MIDIUM / LARGE_USER | 小/中/大体型魔减伤 | |

## 8. 魔法技能增伤（Id 221–231）

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 221–230 | ADDSKILLMDAMAGE_* | 该属性魔法技能增伤 | 221=无，222=水，223=地，224=火，225=风，226=毒，227=圣，228=暗，229=念，230=不死 |
| 231 | ADDSKILLMDAMAGE_ALL | 全系魔法技能增伤 | |

## 9. 经验（Id 232–242）

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 232–241 | ADDEXPPERCENT_KILLRACE_* | 杀该种族经验加成 | 顺序：无形/不死/动物/植物/昆虫/鱼/恶魔/人/天使/龙 |
| 242 | ADDEXPPERCENT_KILLRACE_ALL | 杀怪经验加成（全种族） | |

## 10. 四转面板（Id 243–254）

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 243 | VAR_POWAMOUNT | POW | |
| 244 | VAR_SPLAMOUNT | SPL | |
| 245 | VAR_STAAMOUNT | STA | |
| 246 | VAR_WISAMOUNT | WIS | |
| 247 | VAR_CONAMOUNT | CON | |
| 248 | VAR_CRTAMOUNT | CRT | |
| 249 | VAR_PATKAMOUNT | P.ATK | |
| 250 | VAR_SMATKAMOUNT | S.MATK | |
| 251 | VAR_RESAMOUNT | RES | |
| 252 | VAR_MRESAMOUNT | MRES | |
| 253 | VAR_HEAL_PLUS | 治疗加成 | 脚本 `bHPlus`；区别于 168（治疗量） |
| 254 | VAR_CRITICAL_RATE | 暴击率 | 脚本 `bCRate`；区别于 24（暴击） |

## 11. 杂项（Id 218–220）

| Id | 常量名 | 中文含义 | 备注 |
|----|--------|----------|------|
| 218 | REFLECT_DAMAGE_PERCENT | 反伤（百分比） | 脚本 `bReduceDamageReturn` |
| 219 | MELEE_ATTACK_DAMAGE_TARGET | 近战增伤 | 脚本 `bShortAtkRate` |
| 220 | MELEE_ATTACK_DAMAGE_USER | 近战减伤 | 脚本 `bNearAtkDef` |
