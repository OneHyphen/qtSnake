#ifndef SNAKE_H
#define SNAKE_H

#include <QList>
#include <QPoint>

// 移动方向枚举
enum class Direction {
    Up,
    Down,
    Left,
    Right
};

// 蛇类：负责蛇身体数据、方向、移动、增长与自身碰撞检测
class Snake
{
public:
    Snake();

    // 重置蛇到初始位置（位于网格中央，水平向右）
    void reset(int columns, int rows);

    // 设置当前方向（非法反向会被忽略，返回是否设置成功）
    bool setDirection(Direction dir);

    // 移动一步；grow 为 true 时尾巴保留（蛇增长），否则移除尾巴
    void move(bool grow);

    // 缩短蛇身（毒果等），至少保留 1 节
    void shrink(int cells);

    // 直接设置蛇头位置（传送果）
    void setHead(const QPoint &p);

    // 整体平移并让越界的节环绕到对面（穿墙传送用）
    void translateWrapped(const QPoint &delta, int columns, int rows);

    // 蛇头位置
    QPoint head() const;

    // 按当前方向计算下一步的蛇头位置（不实际移动）
    QPoint nextHead() const;

    // 蛇身体（索引 0 为蛇头）
    const QList<QPoint> &body() const;

    // 当前方向
    Direction direction() const;

    // 是否撞到自己（蛇头与除头外的身体重叠）
    bool selfCollision() const;

    // 是否占据某网格（用于食物生成时避开蛇身）
    bool occupies(const QPoint &point) const;

    // 蛇的长度
    int length() const;

private:
    QList<QPoint> m_body;   // 蛇身体，头在前
    Direction m_direction;  // 当前移动方向
};

#endif // SNAKE_H
