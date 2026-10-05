#include "food.h"

#include <QRandomGenerator>
#include <QSet>

// 默认构造
Food::Food() = default;

// 清空并重新生成 count 个食物（空格不足时能生成多少算多少）
void Food::reset(int count, int columns, int rows, const QList<QPoint> &snakeBody,
                 const QList<QPoint> &blocked)
{
    m_positions.clear();
    for (int i = 0; i < count; ++i) {
        if (!spawnOne(columns, rows, snakeBody, blocked)) {
            break; // 空格不足，能生成多少算多少
        }
    }
}

// 吃掉指定格的食物，返回是否存在
bool Food::consume(const QPoint &cell)
{
    const int index = m_positions.indexOf(cell);
    if (index < 0) {
        return false;
    }
    m_positions.removeAt(index);
    return true;
}

// 补充一颗食物（避开蛇身、障碍与现有食物）
void Food::replenish(int columns, int rows, const QList<QPoint> &snakeBody,
                     const QList<QPoint> &blocked)
{
    spawnOne(columns, rows, snakeBody, blocked);
}

// 当前所有食物位置
const QList<QPoint> &Food::positions() const
{
    return m_positions;
}

// 指定格是否有食物
bool Food::contains(const QPoint &cell) const
{
    return m_positions.contains(cell);
}

// 在指定格添加一颗食物（AI 蛇死亡掉落等）
void Food::add(const QPoint &cell)
{
    if (!m_positions.contains(cell)) {
        m_positions.append(cell);
    }
}

// 随机生成一个不与蛇身/障碍/现有食物重叠的位置；无可用空格或多次冲突时返回 false
bool Food::spawnOne(int columns, int rows, const QList<QPoint> &snakeBody,
                    const QList<QPoint> &blocked)
{
    // 可用空格不足时放弃
    if (snakeBody.size() + m_positions.size() >= columns * rows) {
        return false;
    }

    // 用哈希集合汇总所有占用格（蛇身 / 障碍 / 已有食物），把逐次线性查找降为 O(1)
    QSet<QPoint> occupied;
    occupied.reserve(snakeBody.size() + m_positions.size() + blocked.size());
    for (const QPoint &p : snakeBody) occupied.insert(p);
    for (const QPoint &p : blocked)    occupied.insert(p);
    for (const QPoint &p : m_positions) occupied.insert(p);

    // 随机取点，直到不冲突（限制尝试次数，避免极端情况死循环）
    QPoint candidate;
    bool conflict = true;
    int attempts = 0;
    do {
        candidate = QPoint(QRandomGenerator::global()->bounded(columns),
                           QRandomGenerator::global()->bounded(rows));
        conflict = occupied.contains(candidate);
        ++attempts;
    } while (conflict && attempts < 1000);

    if (conflict) {
        return false; // 多次尝试仍冲突，放弃本次生成
    }

    m_positions.append(candidate);
    return true;
}
