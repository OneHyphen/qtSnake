#ifndef FOOD_H
#define FOOD_H

#include <QList>
#include <QPoint>

// 食物类：负责多个食物的位置与随机生成（棋盘上可同时存在多个食物）
class Food
{
public:
    Food();

    // 清空并生成 count 个食物，避开蛇身、障碍(blocked)且互不重叠
    void reset(int count, int columns, int rows, const QList<QPoint> &snakeBody,
               const QList<QPoint> &blocked = {});

    // 吃掉 cell 处的食物（若存在），返回是否吃到
    bool consume(const QPoint &cell);

    // 补充一个食物（避开蛇身、障碍与现有食物）
    void replenish(int columns, int rows, const QList<QPoint> &snakeBody,
                   const QList<QPoint> &blocked = {});

    // 当前所有食物位置
    const QList<QPoint> &positions() const;

    // 某格是否有食物
    bool contains(const QPoint &cell) const;

    // 直接放置一个食物（例如 AI 蛇死亡转化），已存在则忽略
    void add(const QPoint &cell);

private:
    // 生成一个不与蛇身/障碍/现有食物重叠的位置；无可用空格返回 false
    bool spawnOne(int columns, int rows, const QList<QPoint> &snakeBody,
                  const QList<QPoint> &blocked);

    QList<QPoint> m_positions; // 所有食物所在的网格坐标
};

#endif // FOOD_H
