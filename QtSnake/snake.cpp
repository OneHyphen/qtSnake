#include "snake.h"

// 构造：重置到默认初始状态
Snake::Snake()
    : m_direction(Direction::Right)
{
}

// 重置蛇到初始位置（位于网格中央，水平向右）
void Snake::reset(int columns, int rows)
{
    m_body.clear();

    // 初始蛇：3 节，位于网格中央，水平放置，头朝右
    // 头部在中央偏右，身体依次向左
    const int centerY = rows / 2;
    const int centerX = columns / 2;

    m_body.append(QPoint(centerX + 1, centerY)); // 蛇头
    m_body.append(QPoint(centerX,     centerY)); // 身体
    m_body.append(QPoint(centerX - 1, centerY)); // 身体

    m_direction = Direction::Right;
}

// 设置当前方向（非法反向会被忽略，返回是否成功）
bool Snake::setDirection(Direction dir)
{
    // 禁止 180 度掉头：新方向与当前方向相反则忽略
    const bool reverse =
        (m_direction == Direction::Up    && dir == Direction::Down)  ||
        (m_direction == Direction::Down  && dir == Direction::Up)    ||
        (m_direction == Direction::Left  && dir == Direction::Right) ||
        (m_direction == Direction::Right && dir == Direction::Left);

    if (reverse) {
        return false;
    }

    m_direction = dir;
    return true;
}

// 移动一步；grow 为 true 时尾巴保留（蛇增长）
void Snake::move(bool grow)
{
    const QPoint newHead = nextHead();

    // 头部插入到最前
    m_body.prepend(newHead);

    // 未增长时移除尾巴，保持长度不变
    if (!grow) {
        m_body.removeLast();
    }
}

// 缩短蛇身（毒果等），至少保留 1 节
void Snake::shrink(int cells)
{
    for (int i = 0; i < cells && m_body.size() > 1; ++i) {
        m_body.removeLast();
    }
}

// 直接设置蛇头位置（边界环绕用）
void Snake::setHead(const QPoint &p)
{
    if (!m_body.isEmpty()) {
        m_body[0] = p;
    }
}

// 整体平移并让越界的节环绕到对面（穿墙传送用）
void Snake::translateWrapped(const QPoint &delta, int columns, int rows)
{
    for (QPoint &p : m_body) {
        p += delta;
        if (columns > 0) {
            p.rx() = ((p.x() % columns) + columns) % columns;
        }
        if (rows > 0) {
            p.ry() = ((p.y() % rows) + rows) % rows;
        }
    }
}

// 蛇头位置
QPoint Snake::head() const
{
    return m_body.first();
}

// 按当前方向计算下一步的蛇头位置（不实际移动）
QPoint Snake::nextHead() const
{
    QPoint newHead = m_body.first();

    switch (m_direction) {
    case Direction::Up:    newHead.ry() -= 1; break;
    case Direction::Down:  newHead.ry() += 1; break;
    case Direction::Left:  newHead.rx() -= 1; break;
    case Direction::Right: newHead.rx() += 1; break;
    }

    return newHead;
}

// 蛇身体（索引 0 为蛇头）
const QList<QPoint> &Snake::body() const
{
    return m_body;
}

// 当前移动方向
Direction Snake::direction() const
{
    return m_direction;
}

// 是否撞到自己（蛇头与身体重叠）
bool Snake::selfCollision() const
{
    const QPoint h = head();

    // 从索引 1 开始检查（跳过蛇头自身）
    for (int i = 1; i < m_body.size(); ++i) {
        if (m_body.at(i) == h) {
            return true;
        }
    }
    return false;
}

// 是否占据某网格（食物生成时避开蛇身）
bool Snake::occupies(const QPoint &point) const
{
    return m_body.contains(point);
}

// 蛇的长度
int Snake::length() const
{
    return m_body.size();
}
