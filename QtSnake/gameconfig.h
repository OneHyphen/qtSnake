#ifndef GAMECONFIG_H
#define GAMECONFIG_H

#include <QString>

// 难度等级（三档）
enum class Difficulty {
    Easy,   // 简单
    Normal, // 普通
    Hard    // 困难
};

// 单档难度的集中参数配置
// 说明：速度用“移动间隔(ms)”表示，间隔越小 = 速度越快。
struct DifficultyConfig
{
    QString name;               // 显示名（简单 / 普通 / 困难）

    int initialIntervalMs;      // 初始逻辑移动间隔（毫秒）
    int minIntervalMs;          // 最快时的逻辑移动间隔（毫秒，越大速度越慢）
    int speedRampSec;           // 从初始速度平滑提升到最快速度所需的存活时间（秒）

    int minEventSec;            // 随机事件最小间隔（秒）
    int maxEventSec;            // 随机事件最大间隔（秒）

    int wallCount;              // 随机墙数量（上限）

    int waterCount;             // 初始水潭数量

    double specialFoodChance;   // 特殊食物出现概率系数
    double shieldChance;        // 护盾出现概率

    bool enablePortal;          // 是否开放传送门
    bool enableAISnake;         // 是否开放 AI 蛇
    bool enableFog;             // 是否开放迷雾
    bool enableMovingWall;      // 是否开放移动墙
    bool enableFlood;           // 是否开放洪水

    // 危险事件的基础权重（随存活时间进一步上调）
    double dangerBiasBase;
};

// 取得某难度的配置
inline DifficultyConfig getDifficultyConfig(Difficulty difficulty)
{
    DifficultyConfig c;
    switch (difficulty) {
    case Difficulty::Easy:
        c.name              = QStringLiteral("简单");
        c.initialIntervalMs = 340;   // 100%
        c.minIntervalMs     = 272;   // 125%（最快）
        c.speedRampSec      = 120;
        c.minEventSec       = 30;
        c.maxEventSec       = 45;
        c.wallCount         = 9;
        c.waterCount        = 12;
        c.specialFoodChance = 1.00;
        c.shieldChance      = 0.35;
        c.enablePortal      = false;
        c.enableAISnake     = false;
        c.enableFog         = false;
        c.enableMovingWall  = false;
        c.enableFlood       = false;
        c.dangerBiasBase    = 0.05;
        break;
    case Difficulty::Hard:
        c.name              = QStringLiteral("困难");
        c.initialIntervalMs = 180;   // 130%
        c.minIntervalMs     = 130;   // 180%（最快）
        c.speedRampSec      = 90;
        c.minEventSec       = 8;
        c.maxEventSec       = 20;
        c.wallCount         = 27;
        c.waterCount        = 21;
        c.specialFoodChance = 3.00;
        c.shieldChance      = 0.15;
        c.enablePortal      = true;
        c.enableAISnake     = true;
        c.enableFog         = true;
        c.enableMovingWall  = true;
        c.enableFlood       = true;
        c.dangerBiasBase    = 0.30;
        break;
    case Difficulty::Normal:
    default:
        c.name              = QStringLiteral("普通");
        c.initialIntervalMs = 260;   // 115%
        c.minIntervalMs     = 206;   // 145%（最快）
        c.speedRampSec      = 135;
        c.minEventSec       = 20;
        c.maxEventSec       = 35;
        c.wallCount         = 16;
        c.waterCount        = 15;
        c.specialFoodChance = 2.00;
        c.shieldChance      = 0.25;
        c.enablePortal      = true;
        c.enableAISnake     = true;
        c.enableFog         = true;
        c.enableMovingWall  = false;
        c.enableFlood       = false;
        c.dangerBiasBase    = 0.15;
        break;
    }
    return c;
}

#endif // GAMECONFIG_H
