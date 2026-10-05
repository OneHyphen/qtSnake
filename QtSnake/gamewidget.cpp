#include "gamewidget.h"

#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QSettings>
#include <QTimer>

#ifdef QT_DEBUG
#include <QDebug>
#endif

#include <algorithm>
#include <climits>
#include <cmath>

namespace {

// 方向取反判断
bool isReverse(Direction a, Direction b)
{
    return (a == Direction::Up    && b == Direction::Down)  ||
           (a == Direction::Down  && b == Direction::Up)    ||
           (a == Direction::Left  && b == Direction::Right) ||
           (a == Direction::Right && b == Direction::Left);
}

// 把点按指定方向移动一格
void stepPoint(QPoint &p, Direction dir)
{
    switch (dir) {
    case Direction::Up:    p.ry() -= 1; break;
    case Direction::Down:  p.ry() += 1; break;
    case Direction::Left:  p.rx() -= 1; break;
    case Direction::Right: p.rx() += 1; break;
    }
}

// 水潭减速倍率（移动间隔乘以该值）
qreal waterSlowFactor(Difficulty d)
{
    switch (d) {
    case Difficulty::Easy: return 1.25;
    case Difficulty::Hard: return 1.75;
    case Difficulty::Normal:
    default:               return 1.5;
    }
}

// 存活奖励节点
int rewardTimeForIndex(int i)
{
    static const int times[] = {30, 60, 90, 120};
    if (i < 4) return times[i];
    return 120 + (i - 3) * 30;
}
// 第 i 个存活奖励节点对应的加分
int rewardScoreForIndex(int i)
{
    static const int scores[] = {50, 100, 200, 500};
    if (i < 4) return scores[i];
    return 500 + (i - 3) * 300;
}

// 随机 [minSec, maxSec] 秒的随机时长
qreal randSeconds(int minSec, int maxSec)
{
    if (maxSec <= minSec) return qreal(minSec);
    return qreal(QRandomGenerator::global()->bounded(minSec * 10, maxSec * 10 + 1)) / 10.0;
}

// MM:SS
QString formatTime(int seconds)
{
    if (seconds < 0) seconds = 0;
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

// 曼哈顿距离
int manhattan(const QPoint &a, const QPoint &b)
{
    return qAbs(a.x() - b.x()) + qAbs(a.y() - b.y());
}

constexpr qreal kPi = 3.14159265358979323846;
constexpr qreal kTeleportPhaseSec = 0.30;   // 传送消失/出现各阶段时长（秒）
constexpr qreal kPortalFxSec = 0.35;        // 传送门特效时长（秒）
constexpr qreal kTeleportMinScale = 0.12;   // 传送动画最小缩放
constexpr qreal kShieldInvincibleSec = 1.5; // 护盾消耗后的无敌时长（秒）
constexpr qreal kSpeedEffectSec = 6.0;      // 加速/减速果时长（秒）
constexpr qreal kSpeedBoostFactor = 1.5;    // 加速/减速果对移动间隔的倍率
constexpr qreal kWaterExpandSec = 8.0;      // 水潭扩大时长（秒）
constexpr qreal kFloodSec = 10.0;           // 洪水时长（秒）
constexpr qreal kExtraFoodSec = 8.0;        // 额外食物时长（秒）
constexpr qreal kFogSec = 8.0;              // 迷雾时长（秒）
constexpr int kFoodScore = 10;              // 普通/额外食物得分
constexpr int kAICellScore = 10;            // 击杀 AI 蛇每节得分
constexpr int kGemScore = 50;               // 宝石得分

// 传送失败原因 → 文本（调试日志用）
const char *tpReasonText(TeleportFailReason r)
{
    switch (r) {
    case TeleportFailReason::None:               return "None";
    case TeleportFailReason::OutOfBounds:        return "OutOfBounds";
    case TeleportFailReason::BlockedByWall:      return "BlockedByWall";
    case TeleportFailReason::BlockedByMovingWall:return "BlockedByMovingWall";
    case TeleportFailReason::BlockedBySnake:     return "BlockedBySnake";
    case TeleportFailReason::BlockedByAISnake:   return "BlockedByAISnake";
    case TeleportFailReason::NeedsShieldNoShield:return "NeedsShieldNoShield";
    case TeleportFailReason::AlreadyTeleporting: return "AlreadyTeleporting";
    case TeleportFailReason::NoFreeCell:         return "NoFreeCell";
    }
    return "Unknown";
}

// 传送来源 → 文本（调试日志用）
const char *tpSourceText(TeleportSource s)
{
    return s == TeleportSource::Portal ? "Portal" : "Fruit";
}

// 传送调试日志：仅 Debug 构建输出，Release 完全静音
#ifdef QT_DEBUG
void tpLog(const QString &msg)
{
    qDebug().noquote() << msg;
}
#else
void tpLog(const QString &) {}
#endif

} // namespace

// 构造：读取已保存的最高分、建立画面刷新定时器并初始化蛇与食物
GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent)
{
    // 允许通过键盘获得焦点，从而接收 keyPressEvent
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(300, 200);

    // 读取保存的最高分
    QSettings settings(QStringLiteral("QtSnake"), QStringLiteral("QtSnake"));
    m_highScore = settings.value(QStringLiteral("highScore"), 0).toInt();

    // 画面刷新定时器：固定 ~16ms，只驱动渲染（逻辑移动由时间累计控制）
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &GameWidget::gameUpdate);

    applyConfig();

    // 初始化蛇与食
    m_snake.reset(kColumns, kRows);
    m_food.reset(kFoodCount, kColumns, kRows, m_snake.body());
    m_previousBody = m_snake.body();

    m_frameClock.start();
}

// 只读状态查询（供主窗口刷新显示）
GameState GameWidget::state() const { return m_state; }
int GameWidget::score() const { return m_score; }
int GameWidget::highScore() const { return m_highScore; }
int GameWidget::snakeLength() const { return m_snake.length(); }
int GameWidget::survivalTime() const { return int(m_survivalSec); }
Difficulty GameWidget::difficulty() const { return m_difficulty; }

// 依据当前难度刷新配置参数
void GameWidget::applyConfig()
{
    m_config = getDifficultyConfig(m_difficulty);
}

// 清空场上元素与各类计时（开始 / 返回菜单共用）
void GameWidget::clearFieldState()
{
    // 场上元素
    m_walls.clear();
    m_water.clear();
    m_tempWater.clear();
    m_extraFood.clear();
    m_specialFoods.clear();
    m_aiSnakes.clear();
    m_pendingGameOver = false;

    // 传送门 / 传送动画
    m_portalsActive = false;
    m_exitLockActive = false;
    m_lastTeleportExit = QPoint();
    m_tpState = TeleportState::None;
    m_tpProgress = 0.0;
    m_tpNeedsShield = false;
    m_portalBurstTimer = 0.0;
    m_portalRippleTimer = 0.0;

    // 护盾 / 各类计时
    m_hasShield = false;
    m_invincibleTimer = 0.0;
    m_fogTimer = 0.0;
    m_tempWaterTimer = 0.0;
    m_extraFoodTimer = 0.0;
    m_movingWallActive = false;
    m_movingWallAccum = 0.0;
    m_banner.clear();
    m_bannerTimer = 0.0;
    m_speedBoostTimer = 0.0;
    m_slowTimer = 0.0;
}

// 开始 / 重新开始一局
void GameWidget::startGame()
{
    applyConfig();

    // 重置蛇、分数与方向队列
    m_snake.reset(kColumns, kRows);
    m_previousBody = m_snake.body();
    m_directionQueue.clear();

    m_score = 0;
    m_foodSpeedupMs = 0;
    m_speedProgress = 0.0;
    m_shiftHeld = false;

    // 清空场上元素与各类计
    clearFieldState();

    // 存活与奖
    m_survivalSec = 0.0;
    m_lastSurvivalWhole = -1;
    m_nextRewardIndex = 0;

    // 初始元素（水潭、随机墙数量随难度变化）
    m_spawnSafeDist = 3; // 开局避免在蛇头附近生成，防止一开局就被堵死
    spawnWater(m_config.waterCount);
    spawnWalls(m_config.wallCount / 2);
    if (m_config.enablePortal) spawnPortals();
    m_spawnSafeDist = 0;

    // 障碍生成后再放食物，避免食物落在随机墙 / 水潭 / 传送门
    m_food.reset(kFoodCount, kColumns, kRows, m_snake.body(), blockedCells());

    // 随机事件计时
    m_eventCountdown = randSeconds(m_config.minEventSec, m_config.maxEventSec);

    m_state = GameState::Running;
    resetGameTiming();
    m_timer->start(kFrameIntervalMs); // 始终以 60FPS

    emit scoreChanged(m_score);
    emit highScoreChanged(m_highScore);
    emit stateChanged(m_state);
    emit survivalTimeChanged(0);
    emit gameStarted();
    update();
}

// 暂停 / 继续切换
void GameWidget::togglePause()
{
    if (m_state == GameState::Running) {
        m_state = GameState::Paused;
        m_timer->stop();
    } else if (m_state == GameState::Paused) {
        m_state = GameState::Running;
        // 只丢弃暂停期间的时间，保留插值进度，避免恢复瞬间回退跳变
        m_frameClock.restart();
        m_timer->start(kFrameIntervalMs);
    } else {
        return;
    }

    emit stateChanged(m_state);
    update();
}

// 返回就绪（菜单）状态
void GameWidget::returnToMenu()
{
    m_timer->stop();
    m_state = GameState::Ready;
    m_snake.reset(kColumns, kRows);
    m_food.reset(kFoodCount, kColumns, kRows, m_snake.body());
    m_previousBody = m_snake.body();
    m_directionQueue.clear();
    clearFieldState();
    m_survivalSec = 0.0;
    emit stateChanged(m_state);
    update();
}

// 设置难度（仅在本局开始前有效）
void GameWidget::setDifficulty(Difficulty difficulty)
{
    // 游戏进行中锁定难度
    if (m_state == GameState::Running || m_state == GameState::Paused) {
        return;
    }
    if (m_difficulty == difficulty) {
        return;
    }
    m_difficulty = difficulty;
    applyConfig();
}

// 请求改变方向（反向校验并排队，防连按异常）
void GameWidget::requestDirection(Direction dir)
{
    if (m_state != GameState::Running) {
        return;
    }

    const Direction reference =
        m_directionQueue.isEmpty() ? m_snake.direction() : m_directionQueue.last();

    if (isReverse(reference, dir)) {
        return;
    }
    if (!m_directionQueue.isEmpty() && m_directionQueue.last() == dir) {
        return;
    }
    if (m_directionQueue.size() >= kMaxQueueSize) {
        return;
    }

    m_directionQueue.append(dir);
}

// 每帧调用：累计时间、必要时移动蛇、并刷新画面
void GameWidget::gameUpdate()
{
    if (m_state != GameState::Running) {
        return;
    }

    // 累计真实经过时间；单帧上限 100ms，防止异常情况下累计过多
    const qint64 elapsed = m_frameClock.restart();
    const qint64 frameDelta = std::min<qint64>(elapsed, 100);
    const qreal dt = qreal(frameDelta) / 1000.0;
    m_moveAccumulator += frameDelta;

    // 传送动画期间：冻结世界，只推进动画，禁止蛇正常移动
    updateSurvival(dt);
    updateTimers(dt);
    if (m_tpState != TeleportState::None) {
        updateTeleport(dt);
        m_moveAccumulator = 0;
        update(boardRect());
        return;
    }

    // 每帧系统更新
    updateDynamicSpeed(dt);
    updateSpecialFoods(dt);
    updateAISnakes(dt);
    if (m_pendingGameOver) {
        m_pendingGameOver = false;
        gameOver();
        return;
    }
    updateMovingWall(dt);
    updateRandomEvents(dt);

    const qint64 interval = effectiveInterval();
    m_frameIntervalMs = int(interval); // 本帧缓存一次，绘制插值时直接复用，避免重复计算水潭等信息

    // 只有累计时间达到逻辑移动间隔，蛇才真正移动一次
    if (interval > 0 && m_moveAccumulator >= interval) {
        m_moveAccumulator -= interval; // 保留余量，避免长期运行速度漂移

        // 每步最多应用队列中的一个方向
        if (!m_directionQueue.isEmpty()) {
            m_snake.setDirection(m_directionQueue.takeFirst());
        }

        // 保存移动前的蛇身，供插值使用
        m_previousBody = m_snake.body();

        QPoint next = m_snake.nextHead();

        // 传送门：蛇头即将进入传送门格（或已停在门上）→ 统一传送入口；
        // 触发成功则本步不移动、不做跨地图坐标插值，仅进入传送动画。
        if (tryPortalTeleport(next)) {
            m_moveAccumulator = 0;
            update(boardRect());
            return;
        }
        // 目标非法（无护盾压墙/越界、出口被蛇身/AI 占据等）→ 本次不传送，按普通移动继续

        // 边界环绕：持盾 / 无敌时从对面穿出（持盾则消耗护盾），否则死亡
        bool wrapped = false;
        if (next.x() < 0 || next.x() >= kColumns || next.y() < 0 || next.y() >= kRows) {
            if (m_hasShield || m_invincibleTimer > 0.0) {
                if (next.x() < 0)              next.rx() = kColumns - 1;
                else if (next.x() >= kColumns) next.rx() = 0;
                if (next.y() < 0)              next.ry() = kRows - 1;
                else if (next.y() >= kRows)    next.ry() = 0;
                wrapped = true;
                if (m_hasShield) {                 // 实际穿墙 → 消耗护盾
                    consumeShield();
                }
                showBanner(QStringLiteral("🛡 穿墙！"));
            } else {
                gameOver();
                return;
            }
        }

        const bool eatNormal = m_food.contains(next);
        const bool eatExtra = m_extraFood.contains(next);
        bool grow = eatNormal || eatExtra;

        int specialIndex = -1;
        for (int i = 0; i < m_specialFoods.size(); ++i) {
            if (m_specialFoods.at(i).pos == next) { specialIndex = i; break; }
        }

        // 碰撞检测（墙 / 移动墙 / 自己 / AI 蛇），统一处理穿墙与护盾
        if (resolveMovementCollision(next, grow)) {
            gameOver();
            return;
        }

        // 执行移动
        m_snake.move(grow);
        if (wrapped) {
            m_snake.setHead(next);            // 环绕：蛇头改到对面
            m_previousBody = m_snake.body();  // 环绕后不做插值滑动
        }

        // 蛇头离开防回传锁定的出口格 → 解锁，此后再进入传送门可正常触发
        if (m_exitLockActive && m_snake.head() != m_lastTeleportExit) {
            m_exitLockActive = false;
        }

        // 吃普通/额外食物
        if (eatNormal) {
            m_food.consume(next);
            m_food.replenish(kColumns, kRows, m_snake.body(), blockedCells());
            awardEat();
        }
        if (eatExtra) {
            m_extraFood.removeAll(next);
            awardEat();
        }

        // 特殊食物
        if (specialIndex >= 0) {
            const SpecialFoodType type = m_specialFoods.at(specialIndex).type;
            m_specialFoods.removeAt(specialIndex);
            applySpecialFood(type);
        }
    }

    update(boardRect()); // 每帧刷新画面（约 60FPS
}

// 处理游戏结束：结算最高分并切换状态
void GameWidget::gameOver()
{
    m_state = GameState::GameOver;
    m_timer->stop();

    if (m_score > m_highScore) {
        m_highScore = m_score;
        QSettings settings(QStringLiteral("QtSnake"), QStringLiteral("QtSnake"));
        settings.setValue(QStringLiteral("highScore"), m_highScore);
        emit highScoreChanged(m_highScore);
    }

    emit stateChanged(m_state);
    update();
}

// 吃到普通食物后的累计提速
void GameWidget::speedUp()
{
    // 吃食物累计提速（只影响逻辑间隔，不重启 QTimer
    m_foodSpeedupMs = std::min(m_foodSpeedupMs + kSpeedStep, m_config.initialIntervalMs);
}

// 吃食物的统一结算（加分 / 音效 / 提速）
void GameWidget::awardEat()
{
    m_score += kFoodScore;
    emit scoreChanged(m_score);
    emit foodEaten();
    speedUp();
}

// 结合提速 / 减速 / 水潭后的实际移动间隔（毫秒）
int GameWidget::effectiveInterval() const
{
    // 动态提速：初始间隔 -> 最快间隔（随存活时间线性平滑过渡）
    const qreal base = qreal(m_config.initialIntervalMs)
                     - (m_config.initialIntervalMs - m_config.minIntervalMs) * m_speedProgress
                     - qreal(m_foodSpeedupMs);
    qreal interval = std::max<qreal>(kMinInterval, base);

    if (m_speedBoostTimer > 0.0) interval /= kSpeedBoostFactor; // 加速果
    if (m_slowTimer > 0.0)       interval *= kSpeedBoostFactor; // 减速果
    if (m_shiftHeld)             interval /= kBoostDivisor;

    // 水潭减速（只降速，不影响方向控制）
    const QPoint h = m_snake.head();
    if (isWater(h)) {
        interval *= waterSlowFactor(m_difficulty);
    }

    return std::max(kMinInterval, int(interval));
}

// 依据存活时间平滑提升速度进度（0..1）
void GameWidget::updateDynamicSpeed(qreal dt)
{
    Q_UNUSED(dt);
    if (m_config.speedRampSec <= 0) {
        m_speedProgress = 1.0;
        return;
    }
    // 线性平滑过渡，避免瞬移式加
    m_speedProgress = std::min<qreal>(1.0, m_survivalSec / qreal(m_config.speedRampSec));
}

// 重置时间累计与帧时钟（不改变定时器间隔）
void GameWidget::resetGameTiming()
{
    m_moveAccumulator = 0;
    m_frameClock.restart();
}

// 累计存活时间并在整秒时上报 / 发奖励
void GameWidget::updateSurvival(qreal dt)
{
    m_survivalSec += dt;
    const int whole = int(m_survivalSec);
    if (whole != m_lastSurvivalWhole) {
        m_lastSurvivalWhole = whole;
        emit survivalTimeChanged(whole);
        checkSurvivalReward();
    }
}

// 按既定时间节点发放存活奖励
void GameWidget::checkSurvivalReward()
{
    // 使用明确时间节点，避免每帧加
    while (m_survivalSec >= qreal(rewardTimeForIndex(m_nextRewardIndex))) {
        const int bonus = rewardScoreForIndex(m_nextRewardIndex);
        m_score += bonus;
        emit scoreChanged(m_score);
        showBanner(QStringLiteral("存活奖励 +%1").arg(bonus));
        ++m_nextRewardIndex;
    }
}

// 显示一条提示横幅（约 1.8 秒）
void GameWidget::showBanner(const QString &text)
{
    m_banner = text;
    m_bannerTimer = 1.8;
}

// 随机事件倒计时与加权抽取
void GameWidget::updateRandomEvents(qreal dt)
{
    m_eventCountdown -= dt;
    if (m_eventCountdown > 0.0) {
        return;
    }
    m_eventCountdown = randSeconds(m_config.minEventSec, m_config.maxEventSec);

    struct Cand { RandomEvent type; double w; };
    QList<Cand> cands;
    // 追加一个正权重的候选事件
    auto add = [&](RandomEvent t, double w) { if (w > 0.0) cands.append({t, w}); };

    // 危险事件权重随存活时间上
    const double dangerBias = m_config.dangerBiasBase
                            + std::min<qreal>(1.0, m_survivalSec / 120.0) * 0.7;

    add(RandomEvent::ExtraFood, 1.0);                                // 额外食物（安全）
    add(RandomEvent::WaterExpand, 1.0);                              // 水潭扩大
    add(RandomEvent::WallSpawn, 1.0 * (1.0 + dangerBias));           // 随机墙（危险
    add(RandomEvent::SpeedUp, 0.8);                                  // 加速（安全
    if (m_config.enablePortal)     add(RandomEvent::Portal, 0.8);
    if (m_config.enableFog)        add(RandomEvent::Fog, 0.8 * (1.0 + dangerBias));
    if (m_config.enableFlood)      add(RandomEvent::Flood, 1.0 * (1.0 + dangerBias));
    if (m_config.enableMovingWall) add(RandomEvent::MovingWall, 1.0 * (1.0 + dangerBias));
    if (m_config.enableAISnake)    add(RandomEvent::AISnake, 1.0 * (1.0 + dangerBias));

    if (cands.isEmpty()) return;

    double total = 0.0;
    for (const Cand &c : cands) total += c.w;
    double r = QRandomGenerator::global()->generateDouble() * total;
    RandomEvent chosen = cands.first().type;
    for (const Cand &c : cands) {
        r -= c.w;
        if (r <= 0.0) { chosen = c.type; break; }
    }
    triggerEvent(chosen);
}

// 触发指定的随机事件
void GameWidget::triggerEvent(RandomEvent event)
{
    switch (event) {
    case RandomEvent::ExtraFood: { // 额外食物
        const int n = 3 + QRandomGenerator::global()->bounded(3);
        for (int i = 0; i < n; ++i) {
            QPoint p;
            if (randomFreeCell(p)) m_extraFood.append(p);
        }
        m_extraFoodTimer = kExtraFoodSec;
        showBanner(QStringLiteral("🍎 额外食物！"));
        break; }
    case RandomEvent::WaterExpand: { // 水潭扩大
        const int n = 5 + QRandomGenerator::global()->bounded(5);
        int placed = 0;
        int guard = 0;
        while (placed < n && guard++ < 20) {
            const QList<QPoint> blob = growBlob(2, std::min(5, n - placed));
            if (blob.isEmpty()) break;
            for (const QPoint &p : blob) { m_tempWater.append(p); ++placed; }
        }
        m_tempWaterTimer = std::max(m_tempWaterTimer, kWaterExpandSec);
        showBanner(QStringLiteral("🌊 水潭扩大！"));
        break; }
    case RandomEvent::WallSpawn: // 随机
        spawnWalls(2 + QRandomGenerator::global()->bounded(3));
        showBanner(QStringLiteral("🧱 出现随机墙！"));
        break;
    case RandomEvent::SpeedUp: // 加
        m_speedBoostTimer = std::max(m_speedBoostTimer, kSpeedEffectSec);
        showBanner(QStringLiteral("⚡ 加速！"));
        break;
    case RandomEvent::Portal: // 传送门
        spawnPortals();
        showBanner(QStringLiteral("🌀 传送门出现！"));
        break;
    case RandomEvent::Fog: // 迷雾
        m_fogTimer = kFogSec;
        showBanner(QStringLiteral("🌫 起雾了！"));
        break;
    case RandomEvent::Flood: { // 洪水
        const int n = 12 + QRandomGenerator::global()->bounded(10);
        int placed = 0;
        int guard = 0;
        while (placed < n && guard++ < 20) {
            const QList<QPoint> blob = growBlob(3, std::min(6, n - placed));
            if (blob.isEmpty()) break;
            for (const QPoint &p : blob) { m_tempWater.append(p); ++placed; }
        }
        m_tempWaterTimer = std::max(m_tempWaterTimer, kFloodSec);
        showBanner(QStringLiteral("🌊 洪水来袭！"));
        break; }
    case RandomEvent::MovingWall: // 移动
        if (!m_movingWallActive) {
            QPoint p;
            if (randomFreeCell(p)) {
                m_movingWall = p;
                m_movingWallActive = true;
                m_movingWallDir = Direction::Right;
            }
        }
        showBanner(QStringLiteral("🧱 移动墙出现！"));
        break;
    case RandomEvent::AISnake: // AI 
        spawnAISnake();
        showBanner(QStringLiteral("👻 小蛇出现！"));
        break;
    }
}

// ---------------- 场上元素生成 ----------------

// 该格是否为水潭（含水潭与事件临时水潭）
bool GameWidget::isWater(const QPoint &p) const
{
    return m_water.contains(p) || m_tempWater.contains(p);
}

// 该格是否可放置（避开蛇 / 墙 / 水 / 食物 / 传送门等）
bool GameWidget::cellIsFree(const QPoint &p) const
{
    if (p.x() < 0 || p.x() >= kColumns || p.y() < 0 || p.y() >= kRows) return false;
    if (m_snake.occupies(p)) return false;
    if (m_walls.contains(p) || isWater(p)) return false;
    if (m_food.contains(p) || m_extraFood.contains(p)) return false;
    if (m_portalsActive && (p == m_portalA || p == m_portalB)) return false;
    if (m_movingWallActive && p == m_movingWall) return false;
    for (const SpecialFood &s : m_specialFoods) if (s.pos == p) return false;
    for (const AISnake &ai : m_aiSnakes) if (ai.body.contains(p)) return false;
    if (m_spawnSafeDist > 0) {
        const QPoint h = m_snake.head();
        if (manhattan(p, h) <= m_spawnSafeDist) return false;
    }
    return true;
}

// 当前所有障碍格（食物生成时避开）
QList<QPoint> GameWidget::blockedCells() const
{
    // 食物生成时应避开的格子
    // 随机墙 / 水潭 / 临时水 / 传送门 / 移动墙 / 特殊食物 / 额外食物 / AI 蛇身
    QList<QPoint> blocked;
    blocked.reserve(m_walls.size() + m_water.size() + m_tempWater.size()
                    + m_specialFoods.size() + m_extraFood.size() + 8);
    blocked += m_walls;
    blocked += m_water;
    blocked += m_tempWater;
    blocked += m_extraFood;
    if (m_portalsActive) {
        blocked.append(m_portalA);
        blocked.append(m_portalB);
    }
    if (m_movingWallActive) {
        blocked.append(m_movingWall);
    }
    for (const SpecialFood &s : m_specialFoods) blocked.append(s.pos);
    for (const AISnake &ai : m_aiSnakes) blocked += ai.body;
    return blocked;
}

// 随机取一个可放置的空格
bool GameWidget::randomFreeCell(QPoint &out)
{
    for (int attempt = 0; attempt < 200; ++attempt) {
        const QPoint p(QRandomGenerator::global()->bounded(kColumns),
                       QRandomGenerator::global()->bounded(kRows));
        if (cellIsFree(p)) {
            out = p;
            return true;
        }
    }
    return false; // 没有合法位置就放弃，不强制生
}

// 从随机种子向邻格生长出一个连通块
QList<QPoint> GameWidget::growBlob(int minCells, int maxCells)
{
    QList<QPoint> blob;
    QPoint seed;
    if (!randomFreeCell(seed)) return blob; // 没有合法种子

    if (minCells < 1) minCells = 1;
    if (maxCells < minCells) maxCells = minCells;
    const int target = (maxCells > minCells)
        ? minCells + QRandomGenerator::global()->bounded(maxCells - minCells + 1)
        : maxCells;

    blob.append(seed);

    QList<QPoint> frontier;
    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};
    // 把四邻中尚未加入 frontier 的格子入队（生长用）
    auto pushNeighbors = [&](const QPoint &p) {
        for (int k = 0; k < 4; ++k) {
            frontier.append(QPoint(p.x() + dx[k], p.y() + dy[k]));
        }
    };
    pushNeighbors(seed);

    int guard = 0;
    while (blob.size() < target && !frontier.isEmpty() && guard++ < 500) {
        const int idx = QRandomGenerator::global()->bounded(frontier.size());
        const QPoint p = frontier.takeAt(idx);
        if (blob.contains(p) || !cellIsFree(p)) continue;
        blob.append(p);
        pushNeighbors(p);
    }
    return blob;
}

// 生成若干随机墙（连片，受难度上限约束）
void GameWidget::spawnWalls(int count)
{
    int placed = 0;
    int guard = 0;
    while (placed < count && m_walls.size() < m_config.wallCount && guard++ < 60) {
        const int remain = std::min(count - placed, int(m_config.wallCount - m_walls.size()));
        const QList<QPoint> blob = growBlob(2, std::min(4, remain)); // 墙每次 2~4 块
        if (blob.isEmpty()) break;
        for (const QPoint &p : blob) {
            if (m_walls.size() >= m_config.wallCount) break;
            m_walls.append(p);
            ++placed;
        }
    }
}

// 生成若干水潭（连片）
void GameWidget::spawnWater(int count)
{
    int placed = 0;
    int guard = 0;
    while (placed < count && guard++ < 60) {
        const int remain = count - placed;
        const QList<QPoint> blob = growBlob(2, std::min(5, remain)); // 水潭每团 2~5 
        if (blob.isEmpty()) break;
        for (const QPoint &p : blob) {
            m_water.append(p);
            ++placed;
        }
    }
}

// 随机生成一颗特殊食物
void GameWidget::spawnSpecialFood()
{
    if (m_specialFoods.size() >= 6) return;
    QPoint p;
    if (!randomFreeCell(p)) return;

    QList<SpecialFoodType> types;
    types << SpecialFoodType::Gem << SpecialFoodType::SpeedUp << SpecialFoodType::SlowDown;
    if (m_difficulty == Difficulty::Easy) {
        types << SpecialFoodType::Shield;
    } else {
        types << SpecialFoodType::Shield << SpecialFoodType::Poison << SpecialFoodType::Teleport;
    }

    SpecialFood sf;
    sf.pos = p;
    sf.type = types.at(QRandomGenerator::global()->bounded(types.size()));
    sf.ttl = (sf.type == SpecialFoodType::Gem) ? 5.0 : 10.0;
    m_specialFoods.append(sf);
}

// 随机生成一对传送门 A / B
void GameWidget::spawnPortals()
{
    QPoint a, b;
    if (!randomFreeCell(a)) return;
    if (!randomFreeCell(b)) return;
    if (a == b) return;
    m_portalA = a;
    m_portalB = b;
    m_portalsActive = true;
    m_exitLockActive = false; // 传送门重开，旧锁作废
}

// 生成一条 AI 蛇
void GameWidget::spawnAISnake()
{
    if (m_aiSnakes.size() >= 2) return;
    QPoint h;
    if (!randomFreeCell(h)) return;

    AISnake ai;
    ai.dir = Direction::Right;
    ai.body.clear();
    for (int i = 0; i < 3; ++i) {
        QPoint seg = h;
        seg.rx() = std::max(0, h.x() - i);
        ai.body.append(seg);
    }
    ai.intervalMs = 280 + QRandomGenerator::global()->bounded(160);
    m_aiSnakes.append(ai);
}

// 应用特殊食物效果
void GameWidget::applySpecialFood(SpecialFoodType type)
{
    switch (type) {
    case SpecialFoodType::SpeedUp:
        m_speedBoostTimer = kSpeedEffectSec;
        showBanner(QStringLiteral("⚡ 加速果！"));
        break;
    case SpecialFoodType::SlowDown:
        m_slowTimer = kSpeedEffectSec;
        showBanner(QStringLiteral("🐢 减速果！"));
        break;
    case SpecialFoodType::Gem:
        m_score += kGemScore;
        emit scoreChanged(m_score);
        showBanner(QStringLiteral("💎 宝石 +50"));
        break;
    case SpecialFoodType::Poison:
        m_snake.shrink(2);
        showBanner(QStringLiteral("☠ 毒果！长度 -2"));
        break;
    case SpecialFoodType::Shield:
        m_hasShield = true;
        showBanner(QStringLiteral("🛡 获得护盾！"));
        break;
    case SpecialFoodType::Teleport: {
        // 与传送门共用统一传送逻辑（合法性 / 是否穿墙 / 是否消耗护盾）。
        // 只有真正传送成功才显示成功提示；失败时（落点非法）明确提示，
        // 果实按原设计在 gameUpdate 中已消耗。
        QPoint p;
        bool ok = false;
        if (randomFreeCell(p)) {
            ok = executeTeleport(p, /*animated=*/false, TeleportSource::Fruit);
        } else {
            tpLog(QStringLiteral("[Teleport] fail source=Fruit from=(%1,%2) reason=NoFreeCell")
                      .arg(m_snake.head().x()).arg(m_snake.head().y()));
        }
        showBanner(ok ? QStringLiteral("✨ 传送果！")
                      : QStringLiteral("✨ 传送受阻，果实失效"));
        break; }
    }
}

// ---------------- 每帧更新 ----------------

// 推进特殊食物的存在时间，超时（ttl<=0）移除
void GameWidget::updateSpecialFoods(qreal dt)
{
    for (int i = m_specialFoods.size() - 1; i >= 0; --i) {
        m_specialFoods[i].ttl -= dt;
        if (m_specialFoods.at(i).ttl <= 0.0) m_specialFoods.removeAt(i);
    }
    // 概率生成（与难度相关；分母越小频率越高）
    if (QRandomGenerator::global()->generateDouble() < m_config.specialFoodChance * dt / 3.0) {
        spawnSpecialFood();
    }
}

// 推进 AI 蛇移动、觅食与碰撞结算
void GameWidget::updateAISnakes(qreal dt)
{
    for (int idx = m_aiSnakes.size() - 1; idx >= 0; --idx) {
        AISnake &ai = m_aiSnakes[idx];
        if (ai.body.isEmpty()) { m_aiSnakes.removeAt(idx); continue; }

        ai.moveAccum += dt * 1000.0;
        if (ai.moveAccum < ai.intervalMs) continue;
        ai.moveAccum -= ai.intervalMs;

        const QPoint head = ai.body.first();

        // 找最近食物作为目
        QPoint target;
        bool hasTarget = false;
        int best = INT_MAX;
        // 用曼哈顿距离更新「最近食物」目标
        auto consider = [&](const QPoint &p) {
            const int d = manhattan(p, head);
            if (d < best) { best = d; target = p; hasTarget = true; }
        };
        for (const QPoint &p : m_food.positions()) consider(p);
        for (const QPoint &p : m_extraFood) consider(p);

        Direction nd = ai.dir;
        if (hasTarget) {
            const int dx = target.x() - head.x();
            const int dy = target.y() - head.y();
            QList<Direction> cands;
            if (qAbs(dx) >= qAbs(dy)) {
                if (dx > 0) cands << Direction::Right; else if (dx < 0) cands << Direction::Left;
                if (dy > 0) cands << Direction::Down;  else if (dy < 0) cands << Direction::Up;
            } else {
                if (dy > 0) cands << Direction::Down;  else if (dy < 0) cands << Direction::Up;
                if (dx > 0) cands << Direction::Right; else if (dx < 0) cands << Direction::Left;
            }
            for (Direction c : cands) {
                if (isReverse(ai.dir, c)) continue;
                QPoint np = head;
                stepPoint(np, c);
                if (np.x() < 0 || np.x() >= kColumns || np.y() < 0 || np.y() >= kRows) continue;
                if (m_walls.contains(np) || ai.body.contains(np)) continue;
                nd = c;
                break;
            }
        }
        ai.dir = nd;

        QPoint nh = head;
        stepPoint(nh, ai.dir);
        if (nh.x() < 0 || nh.x() >= kColumns || nh.y() < 0 || nh.y() >= kRows
            || m_walls.contains(nh)) {
            m_aiSnakes.removeAt(idx); // 撞墙/出界，AI 消失
            continue;
        }

        // AI 撞到玩家
        if (m_snake.occupies(nh)) {
            const bool headOn = (nh == m_snake.head());
            const int aiLen = ai.body.size();
            if (headOn && aiLen > m_snake.length()) {
                // 头部对碰：AI 更长 → 玩家本应死亡（护盾/无敌可挡）
                if (!surviveLethalHit()) {
                    m_pendingGameOver = true;
                    return;
                }
                // 玩家靠护盾存活 → AI 撞上护盾，AI 死亡
            }
            // 其余：AI 撞玩家身体、AI 较短/等长、护盾反伤 → AI 死亡并变食物
            killAISnake(idx);
            continue;
        }

        // AI 吃食物（与玩家抢夺）：两个列表各只查一次，避免重复线性查找
        const bool ateNormal = m_food.contains(nh);
        const bool ateExtra = m_extraFood.contains(nh);
        const bool aiEat = ateNormal || ateExtra;
        if (ateNormal) m_food.consume(nh);
        if (ateExtra) m_extraFood.removeAll(nh);

        ai.body.prepend(nh);
        if (!aiEat || ai.body.size() >= kAIMaxLen) {
            ai.body.removeLast();
        }
    }
}

// 统一的护盾消耗入口（清护盾 + 触发短暂无敌）
void GameWidget::consumeShield()
{
    m_hasShield = false;
    m_invincibleTimer = kShieldInvincibleSec;
}

// 护盾 / 无敌抵挡致命碰撞，返回是否存活
bool GameWidget::surviveLethalHit()
{
    if (m_invincibleTimer > 0.0) {
        return true;
    }
    if (m_hasShield) {
        consumeShield();
        showBanner(QStringLiteral("🛡 护盾抵挡！"));
        return true;
    }
    return false;
}

// 移动碰撞判定（穿墙 / 自撞 / AI 蛇），返回是否死亡
bool GameWidget::resolveMovementCollision(const QPoint &next, bool grow)
{
    bool dead = false;

    // 穿墙：有护盾则穿墙并消耗护盾（+短暂无敌），否则死亡；
    // 护盾消耗后本次移动正常完成，不会再重复触发普通墙壁碰撞
    const bool hitWall = m_walls.contains(next);
    const bool hitMovingWall = m_movingWallActive && next == m_movingWall;
    if (hitWall || hitMovingWall) {
        if (m_hasShield) {
            consumeShield();
            showBanner(QStringLiteral("🛡 穿墙！"));
        } else {
            dead = true;
        }
    }

    // 自撞（未增长时尾巴会移开，不算撞）
    if (!dead && m_snake.occupies(next)) {
        const QPoint tail = m_snake.body().last();
        if (grow || next != tail) {
            dead = true;
        }
    }

    // AI 蛇（头部对碰：更小的蛇死亡；等长同归于尽）
    if (!dead) {
        for (int i = 0; i < m_aiSnakes.size(); ++i) {
            const AISnake &ai = m_aiSnakes.at(i);
            if (!ai.body.contains(next)) continue;
            const bool headOn = (next == ai.body.first());
            const int aiLen = ai.body.size();
            if (headOn && m_snake.length() > aiLen) {
                killAISnake(i);   // 玩家更长 → AI 死亡并变食物
            } else if (headOn && m_snake.length() < aiLen) {
                dead = true;      // 玩家更短 → 玩家死亡
            } else if (headOn) {
                killAISnake(i);   // 等长 → 同归于尽
                dead = true;
            } else {
                dead = true;      // 撞到 AI 身体 → 玩家死亡
            }
            break;
        }
    }

    // 护盾 / 无敌抵挡致命碰撞
    if (dead && surviveLethalHit()) {
        dead = false;
    }
    return dead;
}

// AI 死亡：各节变普通食物并加分
void GameWidget::killAISnake(int idx)
{
    if (idx < 0 || idx >= m_aiSnakes.size()) return;
    const AISnake &ai = m_aiSnakes.at(idx); // 只读引用，避免拷贝整条身体
    int bonus = 0;
    for (const QPoint &p : ai.body) {
        m_food.add(p);   // 每一节变成普通食物
        bonus += kAICellScore;     // 加分
    }
    m_aiSnakes.removeAt(idx);
    if (bonus > 0) {
        m_score += bonus;
        emit scoreChanged(m_score);
    }
    showBanner(QStringLiteral("💥 AI 蛇化作食物 +%1").arg(bonus));
}

// 移动墙每 0.5s 前进一格，受阻则掉头
void GameWidget::updateMovingWall(qreal dt)
{
    if (!m_movingWallActive) return;
    m_movingWallAccum += dt * 1000.0;
    if (m_movingWallAccum < 500.0) return;
    m_movingWallAccum -= 500.0;

    QPoint np = m_movingWall;
    stepPoint(np, m_movingWallDir);

    const bool blocked = (np.x() < 0 || np.x() >= kColumns || np.y() < 0 || np.y() >= kRows
                          || m_walls.contains(np) || m_snake.occupies(np));
    if (blocked) {
        // 掉头
        switch (m_movingWallDir) {
        case Direction::Up:    m_movingWallDir = Direction::Down; break;
        case Direction::Down:  m_movingWallDir = Direction::Up; break;
        case Direction::Left:  m_movingWallDir = Direction::Right; break;
        case Direction::Right: m_movingWallDir = Direction::Left; break;
        }
        return;
    }
    m_movingWall = np;
}

// 推进护盾无敌、传送特效、迷雾、横幅等计时
void GameWidget::updateTimers(qreal dt)
{
    // 倒计时递减：把非负计时器按 dt 衰减到 0
    auto tick = [dt](qreal &t) { if (t > 0.0) t = std::max<qreal>(0.0, t - dt); };
    tick(m_speedBoostTimer);
    tick(m_slowTimer);
    tick(m_invincibleTimer);
    tick(m_portalBurstTimer);
    tick(m_portalRippleTimer);
    tick(m_fogTimer);
    tick(m_bannerTimer);

    if (m_extraFoodTimer > 0.0) {
        m_extraFoodTimer = std::max<qreal>(0.0, m_extraFoodTimer - dt);
        if (m_extraFoodTimer <= 0.0) m_extraFood.clear();
    }
    if (m_tempWaterTimer > 0.0) {
        m_tempWaterTimer = std::max<qreal>(0.0, m_tempWaterTimer - dt);
        if (m_tempWaterTimer <= 0.0) m_tempWater.clear();
    }
}

// 判断传送目标是否合法：越界 / 压墙（含移动墙）仅在允许穿墙时放行，蛇身与 AI 蛇一律拒绝
bool GameWidget::canTeleportTo(const QPoint &target, bool allowWallPass,
                               TeleportFailReason *reason) const
{
    // 记录传送失败原因并返回 false
    auto fail = [&](TeleportFailReason r) {
        if (reason) *reason = r;
        return false;
    };

    const bool outOfBounds = target.x() < 0 || target.x() >= kColumns
                          || target.y() < 0 || target.y() >= kRows;

    // 越界 / 墙体（含移动墙）：仅当允许穿墙（持盾）时可达，穿墙成功后才消耗护盾
    if (outOfBounds) {
        return allowWallPass ? true : fail(TeleportFailReason::OutOfBounds);
    }
    if (m_walls.contains(target)) {
        return allowWallPass ? true : fail(TeleportFailReason::BlockedByWall);
    }
    if (m_movingWallActive && target == m_movingWall) {
        return allowWallPass ? true : fail(TeleportFailReason::BlockedByMovingWall);
    }

    // 蛇身 / AI 蛇：无论是否有护盾都不可进入
    if (m_snake.occupies(target)) {
        return fail(TeleportFailReason::BlockedBySnake);
    }
    for (const AISnake &ai : m_aiSnakes) {
        if (ai.body.contains(target)) {
            return fail(TeleportFailReason::BlockedByAISnake);
        }
    }
    return true;
}

// 传送门统一触发入口（即将进入门格 / 已停在门上）
bool GameWidget::tryPortalTeleport(const QPoint &next)
{
    if (!m_portalsActive || m_tpState != TeleportState::None) {
        return false;
    }

    const QPoint head = m_snake.head();

    // 入口判定：蛇头即将进入传送门格，或蛇头正停在传送门格上
    // （例如上一次传送因落点非法被拒绝后走上了门格）。两者走同一条通路。
    QPoint entry;
    QPoint exitCell;
    if (next == m_portalA)      { entry = m_portalA; exitCell = m_portalB; }
    else if (next == m_portalB) { entry = m_portalB; exitCell = m_portalA; }
    else if (head == m_portalA) { entry = m_portalA; exitCell = m_portalB; }
    else if (head == m_portalB) { entry = m_portalB; exitCell = m_portalA; }
    else {
        return false;
    }

    // 蛇头已在出口格（两门相邻时从出口门走进入口门）→ 零位移传送无意义，跳过
    if (exitCell == head) {
        return false;
    }

    // 防回传锁：刚传送到出口格且蛇头尚未离开时，不得再次从该格触发，
    // 阻止 A→B→A→B 连环传送；蛇头离开出口格后锁自动解除（见 gameUpdate）。
    if (m_exitLockActive && entry == m_lastTeleportExit) {
        tpLog(QStringLiteral("[Teleport] skip source=Portal entry=(%1,%2) reason=ExitLocked")
                  .arg(entry.x()).arg(entry.y()));
        return false;
    }

    if (!executeTeleport(exitCell, /*animated=*/true, TeleportSource::Portal)) {
        return false; // 落点非法且无法穿墙 → 本次不传送，按普通移动继续
    }
    m_tpEntryCell = entry; // 入口格（供爆发特效与绘制）
    return true;
}

// 统一传送：目标合法性 + 穿墙判定 + 护盾 / 防回传结算
bool GameWidget::executeTeleport(const QPoint &target, bool animated, TeleportSource source)
{
    // 状态保护：传送动画期间不接受新的传送，避免状态机被覆盖卡死
    if (m_tpState != TeleportState::None) {
        tpLog(QStringLiteral("[Teleport] fail source=%1 to=(%2,%3) reason=AlreadyTeleporting")
                  .arg(tpSourceText(source)).arg(target.x()).arg(target.y()));
        return false;
    }

    const QPoint from = m_snake.head();
    const QPoint delta = target - from;

    // 统一目标检查：蛇头落点合法性（越界/墙/移动墙按护盾放行，蛇身/AI 一律拒绝）
    TeleportFailReason reason = TeleportFailReason::None;
    if (!canTeleportTo(target, /*allowWallPass=*/m_hasShield, &reason)) {
        tpLog(QStringLiteral("[Teleport] fail source=%1 from=(%2,%3) to=(%4,%5) shield=%6 allowWallPass=%7 reason=%8")
                  .arg(tpSourceText(source))
                  .arg(from.x()).arg(from.y())
                  .arg(target.x()).arg(target.y())
                  .arg(m_hasShield).arg(m_hasShield)
                  .arg(tpReasonText(reason)));
        return false;
    }

    // 全身落点扫描：任一节越界或压墙（含移动墙）→ 属于「穿墙传送」，需要护盾。
    // （整体平移不会与自身重叠，故蛇身只查墙/越界；AI 蛇仅约束蛇头落点，保持原有宽松行为。）
    bool needsShield = false;
    for (const QPoint &seg : m_snake.body()) {
        const QPoint t = seg + delta;
        if (t.x() < 0 || t.x() >= kColumns || t.y() < 0 || t.y() >= kRows
            || m_walls.contains(t) || (m_movingWallActive && t == m_movingWall)) {
            needsShield = true;
            break;
        }
    }

    // 需要穿墙但没有护盾 → 非法，拒绝（不做任何兜底）
    if (needsShield && !m_hasShield) {
        tpLog(QStringLiteral("[Teleport] fail source=%1 from=(%2,%3) to=(%4,%5) shield=0 reason=NeedsShieldNoShield")
                  .arg(tpSourceText(source))
                  .arg(from.x()).arg(from.y())
                  .arg(target.x()).arg(target.y()));
        return false;
    }

    if (animated) {
        // 传送门：启动「缩小消失 / 出现」动画，真正的坐标平移在动画切换时进行
        m_tpState = TeleportState::Exit;
        m_tpProgress = 0.0;
        m_tpDelta = delta;
        m_tpNeedsShield = needsShield;
        m_tpExitCell = target;
        m_lastTeleportExit = target;   // 防回传锁：锁住出口格
        m_exitLockActive = true;
        m_portalBurstTimer = kPortalFxSec;   // 入口爆发特效
        showBanner(QStringLiteral("🌀 传送！"));
    } else {
        // 传送果：立即整体平移；果实落点不与传送门重合，清除防回传锁
        m_exitLockActive = false;
        applyTeleport(delta, needsShield);
    }

    tpLog(QStringLiteral("[Teleport] success source=%1 from=(%2,%3) to=(%4,%5) shield=%6 allowWallPass=%7 needsShield=%8")
              .arg(tpSourceText(source))
              .arg(from.x()).arg(from.y())
              .arg(target.x()).arg(target.y())
              .arg(m_hasShield).arg(m_hasShield).arg(needsShield));
    return true;
}

// 整体平移蛇身并结算护盾消耗
void GameWidget::applyTeleport(const QPoint &delta, bool needsShield)
{
    // 整条蛇一起平移（越界的节环绕），避免只剩蛇头移动 / 拉伸
    m_snake.translateWrapped(delta, kColumns, kRows);
    m_previousBody = m_snake.body();

    // 只有实际穿墙才消耗护盾；普通传送保留护
    if (needsShield && m_hasShield) {
        consumeShield();
        showBanner(QStringLiteral("🛡 穿墙传送！"));
    }
}

// 推进传送动画状态机（Exit → Enter → None）
void GameWidget::updateTeleport(qreal dt)
{
    m_tpProgress += dt / kTeleportPhaseSec;
    if (m_tpProgress < 1.0) {
        return;
    }

    if (m_tpState == TeleportState::Exit) {
        // 消失完成：整条蛇整体平移到出口（穿墙传送则在此消耗护盾）
        applyTeleport(m_tpDelta, m_tpNeedsShield);
        m_tpState = TeleportState::Enter;
        m_tpProgress = 0.0;
        m_portalRippleTimer = kPortalFxSec; // 出口扩散波纹
    } else if (m_tpState == TeleportState::Enter) {
        // 出现完成：恢复正常移
        m_tpState = TeleportState::None;
        m_tpProgress = 0.0;
        m_moveAccumulator = 0;
    }
}

// ---------------- 布局 ----------------

// 计算棋盘绘制区域（保持 3:2 比例在窗口内居中）
QRect GameWidget::boardRect() const
{
    const int cell = cellSize();
    const int bw = cell * kColumns;
    const int bh = cell * kRows;
    const int x = (width() - bw) / 2;
    const int y = (height() - bh) / 2;
    return QRect(x, y, bw, bh);
}

// 计算格子边长（保持棋盘 3:2 且居中）
int GameWidget::cellSize() const
{
    return std::max(1, std::min(width() / kColumns, height() / kRows));
}

// 绘制一帧：底板缓存 + 各类动态元素
void GameWidget::paintEvent(QPaintEvent *)
{
    if (m_boardCacheSize != size()) {
        rebuildBoardCache();
        m_boardCacheSize = size();
    }

    const QRect board = boardRect();
    const int cell = cellSize();

    QPainter painter(this);
    painter.drawPixmap(0, 0, m_boardCache);

    painter.setRenderHint(QPainter::Antialiasing);

    drawWater(painter, board, cell);
    drawWalls(painter, board, cell);
    drawPortals(painter, board, cell);
    drawSpecialFoods(painter, board, cell);
    drawFood(painter, board, cell);
    drawAISnakes(painter, board, cell);
    drawSnake(painter, board, cell);
    drawFog(painter, board);
    drawBanner(painter, board);
    drawOverlayText(painter, board);
}

// 窗口尺寸变化时使棋盘缓存失效
void GameWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_boardCacheSize = QSize();
    update();
}

// 就绪 / 结束状态点击画面即可开始游戏
void GameWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton
        && (m_state == GameState::Ready || m_state == GameState::GameOver)) {
        emit startRequested();
    }
    QWidget::mousePressEvent(event);
}

// 键盘输入：方向 / 空格 / R / Esc / Shift 加速
void GameWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_W:
        requestDirection(Direction::Up);
        break;
    case Qt::Key_Down:
    case Qt::Key_S:
        requestDirection(Direction::Down);
        break;
    case Qt::Key_Left:
    case Qt::Key_A:
        requestDirection(Direction::Left);
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        requestDirection(Direction::Right);
        break;
    case Qt::Key_Space:
        if (m_state == GameState::Ready || m_state == GameState::GameOver) {
            startGame();
        } else {
            togglePause();
        }
        break;
    case Qt::Key_R:
        startGame();
        break;
    case Qt::Key_Escape:
        emit exitRequested();
        break;
    case Qt::Key_Shift:
        if (!m_shiftHeld) {
            m_shiftHeld = true;
            resetGameTiming();
        }
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
}

// 松开 Shift 结束加速
void GameWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift) {
        if (m_shiftHeld && !event->isAutoRepeat()) {
            m_shiftHeld = false;
            resetGameTiming();
        }
        return;
    }
    QWidget::keyReleaseEvent(event);
}

// 重建棋盘底板缓存（背景 + 边框 + 网格）
void GameWidget::rebuildBoardCache()
{
    if (size().isEmpty()) {
        return;
    }

    const QRect board = boardRect();
    const int cell = cellSize();

    const qreal dpr = devicePixelRatioF();
    m_boardCache = QPixmap(size() * dpr);
    m_boardCache.setDevicePixelRatio(dpr);
    m_boardCache.fill(Qt::transparent);

    QPainter painter(&m_boardCache);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(70, 80, 100), 2));
    painter.setBrush(QColor(20, 24, 32, 210));
    painter.drawRoundedRect(board, 12, 12);

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(QColor(46, 52, 66), 1));

    for (int x = 1; x < kColumns; ++x) {
        const int px = board.left() + x * cell;
        painter.drawLine(px, board.top(), px, board.bottom());
    }
    for (int y = 1; y < kRows; ++y) {
        const int py = board.top() + y * cell;
        painter.drawLine(board.left(), py, board.right(), py);
    }
}

// ---------------- 绘制 ----------------

// 绘制水潭（经过时减速的格子）
void GameWidget::drawWater(QPainter &painter, const QRect &board, int cell)
{
    painter.setPen(Qt::NoPen);
    // 把一组格子按给定透明度绘制
    auto drawSet = [&](const QList<QPoint> &cells, int alpha) {
        painter.setBrush(QColor(60, 150, 230, alpha));
        for (const QPoint &p : cells) {
            const QRectF r(board.left() + p.x() * cell, board.top() + p.y() * cell, cell, cell);
            painter.drawRoundedRect(r.adjusted(cell * 0.06, cell * 0.06, -cell * 0.06, -cell * 0.06),
                                    cell * 0.2, cell * 0.2);
        }
    };
    drawSet(m_water, 150);
    drawSet(m_tempWater, 120); // 临时水潭略浅
}

// 绘制随机墙与移动墙
void GameWidget::drawWalls(QPainter &painter, const QRect &board, int cell)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(140, 130, 120));
    for (const QPoint &p : m_walls) {
        const QRectF r(board.left() + p.x() * cell, board.top() + p.y() * cell, cell, cell);
        painter.drawRoundedRect(r.adjusted(cell * 0.05, cell * 0.05, -cell * 0.05, -cell * 0.05),
                                cell * 0.15, cell * 0.15);
    }
    if (m_movingWallActive) {
        const QRectF r(board.left() + m_movingWall.x() * cell,
                       board.top() + m_movingWall.y() * cell, cell, cell);
        painter.setBrush(QColor(220, 120, 60));
        painter.drawRoundedRect(r.adjusted(cell * 0.05, cell * 0.05, -cell * 0.05, -cell * 0.05),
                                cell * 0.15, cell * 0.15);
    }
}

// 绘制传送门及传送特效（入口爆发 / 出口波纹）
void GameWidget::drawPortals(QPainter &painter, const QRect &board, int cell)
{
    if (!m_portalsActive) return;

    // 格子中心坐标
    auto centerOf = [&](const QPoint &p) {
        return QPointF(board.left() + (p.x() + 0.5) * cell,
                       board.top() + (p.y() + 0.5) * cell);
    };

    // 绘制单个传送门（圆环 + 两个旋转光点）
    auto drawPortal = [&](const QPoint &p, const QColor &c) {
        const QPointF center = centerOf(p);
        painter.setPen(QPen(c, std::max(2.0, cell * 0.12)));
        painter.setBrush(QColor(c.red(), c.green(), c.blue(), 60));
        const qreal rad = cell * 0.36;
        painter.drawEllipse(center, rad, rad);
        // 两个旋转的点，表示旋转闪烁
        const qreal ang = m_survivalSec * 4.0;
        painter.setBrush(c);
        painter.setPen(Qt::NoPen);
        for (int k = 0; k < 2; ++k) {
            const qreal a = ang + k * kPi;
            const QPointF dot(center.x() + std::cos(a) * rad,
                              center.y() + std::sin(a) * rad);
            painter.drawEllipse(dot, cell * 0.09, cell * 0.09);
        }
    };
    drawPortal(m_portalA, QColor(180, 100, 240));
    drawPortal(m_portalB, QColor(100, 200, 240));

    // 入口爆发（消失阶段）：圆环快速放大一次 + 轻微粒子
    if (m_portalBurstTimer > 0.0) {
        const qreal k = m_portalBurstTimer / kPortalFxSec; // 1 → 0
        const QPointF c = centerOf(m_tpEntryCell);
        const qreal rad = cell * (0.36 + (1.0 - k) * 0.9);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(210, 160, 255, int(210 * k)), std::max(2.0, cell * 0.14)));
        painter.drawEllipse(c, rad, rad);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(225, 190, 255, int(220 * k)));
        for (int i = 0; i < 6; ++i) {
            const qreal a = i * (kPi / 3.0) + (1.0 - k) * 2.0;
            const QPointF pt(c.x() + std::cos(a) * rad, c.y() + std::sin(a) * rad);
            painter.drawEllipse(pt, cell * 0.07, cell * 0.07);
        }
    }

    // 出口扩散波纹（出现阶段）
    if (m_portalRippleTimer > 0.0) {
        const qreal k = m_portalRippleTimer / kPortalFxSec;
        const QPointF c = centerOf(m_tpExitCell);
        const qreal rad = cell * (0.36 + (1.0 - k) * 1.1);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(140, 220, 255, int(210 * k)), std::max(2.0, cell * 0.14)));
        painter.drawEllipse(c, rad, rad);
    }
}

// 绘制特殊食物（彩色圆 + 类型符号）
void GameWidget::drawSpecialFoods(QPainter &painter, const QRect &board, int cell)
{
    painter.setPen(Qt::NoPen);
    // 符号字号只与格子大小有关，循环外构建一次即可（避免每颗食物重复设置字体）
    QFont markFont = painter.font();
    markFont.setPointSizeF(std::max(6.0, cell * 0.32));
    painter.setFont(markFont);
    for (const SpecialFood &s : m_specialFoods) {
        const QPointF c(board.left() + (s.pos.x() + 0.5) * cell,
                        board.top() + (s.pos.y() + 0.5) * cell);
        const qreal r = cell * 0.3;
        QColor col;
        QString mark;
        switch (s.type) {
        case SpecialFoodType::SpeedUp:  col = QColor(80, 200, 255);  mark = QStringLiteral("⚡"); break;
        case SpecialFoodType::SlowDown: col = QColor(120, 180, 90);  mark = QStringLiteral("🐢"); break;
        case SpecialFoodType::Gem:      col = QColor(120, 220, 255); mark = QStringLiteral("◆"); break;
        case SpecialFoodType::Poison:   col = QColor(150, 80, 190);  mark = QStringLiteral("☠"); break;
        case SpecialFoodType::Shield:   col = QColor(255, 210, 80);  mark = QStringLiteral("🛡"); break;
        case SpecialFoodType::Teleport: col = QColor(200, 120, 240); mark = QStringLiteral("✨"); break;
        }
        painter.setBrush(col);
        painter.drawEllipse(c, r, r);
        painter.setPen(QColor(20, 20, 20));
        painter.drawText(QRectF(c.x() - r, c.y() - r, r * 2, r * 2), Qt::AlignCenter, mark);
        painter.setPen(Qt::NoPen);
    }
}

// 绘制普通食物与事件额外食物
void GameWidget::drawFood(QPainter &painter, const QRect &board, int cell)
{
    painter.setPen(Qt::NoPen);
    const qreal r = cell * 0.32;

    // 绘制一颗带发光效果的食物
    auto drawDot = [&](const QPoint &p, const QColor &main, const QColor &glowCol) {
        const QPointF c(board.left() + (p.x() + 0.5) * cell,
                        board.top() + (p.y() + 0.5) * cell);
        QRadialGradient glow(c, cell * 0.65);
        glow.setColorAt(0.0, glowCol);
        glow.setColorAt(1.0, QColor(glowCol.red(), glowCol.green(), glowCol.blue(), 0));
        painter.setBrush(glow);
        painter.drawEllipse(c, cell * 0.65, cell * 0.65);
        painter.setBrush(main);
        painter.drawEllipse(c, r, r);
        painter.setBrush(QColor(255, 255, 255, 200));
        painter.drawEllipse(QPointF(c.x() - r * 0.35, c.y() - r * 0.35), r * 0.28, r * 0.28);
    };

    for (const QPoint &p : m_food.positions()) {
        drawDot(p, QColor(255, 70, 70), QColor(255, 90, 90, 160));
    }
    // 事件产生的额外食物：用橙色区
    for (const QPoint &p : m_extraFood) {
        drawDot(p, QColor(255, 150, 40), QColor(255, 170, 60, 160));
    }
}

// 绘制 AI 蛇
void GameWidget::drawAISnakes(QPainter &painter, const QRect &board, int cell)
{
    if (m_aiSnakes.isEmpty()) return;
    painter.setPen(Qt::NoPen);
    for (const AISnake &ai : m_aiSnakes) {
        for (int i = ai.body.size() - 1; i >= 0; --i) {
            const QPoint &p = ai.body.at(i);
            const QRectF r(board.left() + p.x() * cell, board.top() + p.y() * cell, cell, cell);
            painter.setBrush(i == 0 ? QColor(255, 140, 60) : QColor(210, 110, 50));
            painter.drawRoundedRect(r.adjusted(cell * 0.1, cell * 0.1, -cell * 0.1, -cell * 0.1),
                                    cell * 0.3, cell * 0.3);
        }
    }
}

// 绘制迷雾遮罩
void GameWidget::drawFog(QPainter &painter, const QRect &board)
{
    if (m_fogTimer <= 0.0) return;
    const qreal k = std::min<qreal>(1.0, m_fogTimer / 1.0); // 淡出
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(200, 205, 215, int(150 * k)));
    painter.drawRoundedRect(board, 12, 12);
}

// 绘制提示横幅
void GameWidget::drawBanner(QPainter &painter, const QRect &board)
{
    if (m_bannerTimer <= 0.0 || m_banner.isEmpty()) return;

    QFont font = painter.font();
    font.setPointSize(16);
    font.setBold(true);
    painter.setFont(font);

    const QFontMetrics fm(font);
    const int tw = fm.horizontalAdvance(m_banner) + 40;
    const int th = fm.height() + 18;
    const QRectF box(board.center().x() - tw / 2.0, board.top() + board.height() * 0.10,
                     tw, th);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 170));
    painter.drawRoundedRect(box, 10, 10);

    painter.setPen(QColor(255, 240, 180));
    painter.drawText(box, Qt::AlignCenter, m_banner);
}

// 绘制蛇（位置插值 + 传送动画整体缩放 / 透明度）
void GameWidget::drawSnake(QPainter &painter, const QRect &board, int cell)
{
    const auto &body = m_snake.body();
    const int n = body.size();

    // 插值系数：0 = 上一逻辑位置，1 = 当前逻辑位置
    // 优先复用本帧已算好的有效间隔；极少数未开局/未计算的场景再回退现算
    const int intervalMs = (m_frameIntervalMs > 0) ? m_frameIntervalMs : effectiveInterval();
    const qreal interval = static_cast<qreal>(intervalMs);
    const qreal alpha = (interval > 0.0)
        ? qBound<qreal>(0.0, static_cast<qreal>(m_moveAccumulator) / interval, 1.0)
        : 1.0;

    QList<QPointF> pos;
    pos.reserve(n);
    for (int i = 0; i < n; ++i) {
        const QPoint cur = body.at(i);
        QPoint prev = cur;
        if (i < m_previousBody.size()) {
            prev = m_previousBody.at(i);
        } else if (!m_previousBody.isEmpty()) {
            prev = m_previousBody.last();
        }
        // 若该节发生环绕（跨边界跳变，previous 与 current 不相邻）→ 直接吸附，避免长距离插值拉伸
        if (manhattan(prev, cur) > 1) {
            pos.append(QPointF(cur));
        } else {
            pos.append(QPointF(prev) * (1.0 - alpha) + QPointF(cur) * alpha);
        }
    }

    // 参考图配色：黄色身体 + 浅色肚子 + 深色背 + 棕色斑 + 绿色单眼
    // 固定配色（static const，避免每帧重复构造）
    static const QColor bodyColor(245, 205, 70);
    static const QColor headColor(250, 215, 90);
    static const QColor bellyColor(250, 232, 150);
    static const QColor beakColor(120, 80, 35);
    static const QColor footColor(160, 105, 50);
    static const QColor eyeIris(70, 170, 95);
    static const QColor eyeDark(25, 30, 25);
    static const QColor eyeShine(255, 255, 255);

    const qreal bodyMargin = cell * 0.08;
    const qreal headMargin = cell * 0.06;
    const qreal radius = cell * 0.35;

    // 按浮点坐标取格子矩形
    auto cellRectOf = [&](const QPointF &p) {
        return QRectF(board.left() + p.x() * cell,
                      board.top() + p.y() * cell,
                      cell, cell);
    };

    QPointF front(0, 0);
    switch (m_snake.direction()) {
    case Direction::Up:    front = QPointF(0, -1); break;
    case Direction::Down:  front = QPointF(0,  1); break;
    case Direction::Left:  front = QPointF(-1, 0); break;
    case Direction::Right: front = QPointF(1,  0); break;
    }
    const QPointF perp(-front.y(), front.x());

    // 传送动画：整体缩放 / 透明度 / 可见节数（动画渲染与逻辑坐标分离）
    qreal teleScale = 1.0;
    qreal teleOpacity = 1.0;
    int   visible = n;
    QPointF teleCenter;
    if (m_tpState == TeleportState::Exit) {
        const qreal p = qBound<qreal>(0.0, m_tpProgress, 1.0);
        teleScale = 1.0 + (kTeleportMinScale - 1.0) * p; // 1.0 → 0.12
        teleOpacity = 1.0 - p;                        // 1 → 0
        visible = int(std::ceil(n * (1.0 - p)));      // 尾巴先消
        teleCenter = cellRectOf(QPointF(m_tpEntryCell)).center();
    } else if (m_tpState == TeleportState::Enter) {
        const qreal p = qBound<qreal>(0.0, m_tpProgress, 1.0);
        teleScale = kTeleportMinScale + (1.0 - kTeleportMinScale) * p; // 0.12 → 1.0
        teleOpacity = p;                              // 0 → 1
        visible = int(std::ceil(n * p));              // 蛇头先出
        teleCenter = cellRectOf(QPointF(m_tpExitCell)).center();
    }
    visible = std::max(0, std::min(n, visible));

    painter.save();
    if (teleOpacity < 1.0) painter.setOpacity(teleOpacity);
    if (teleScale != 1.0) {
        painter.translate(teleCenter);
        painter.scale(teleScale, teleScale);
        painter.translate(-teleCenter);
    }

    painter.setPen(Qt::NoPen);

    // 护盾光环
    if ((m_hasShield || m_invincibleTimer > 0.0) && visible >= 1) {
        const QPointF hc = cellRectOf(pos.at(0)).center();
        painter.setBrush(QColor(120, 200, 255, m_hasShield ? 70 : 40));
        painter.drawEllipse(hc, cell * 0.62, cell * 0.62);
    }

    // 1) 身体：简单黄色圆角方块（省略身材细节），从尾到头绘制
    painter.setBrush(bodyColor);
    for (int i = std::min(n - 1, visible - 1); i >= 1; --i) {
        const QRectF r = cellRectOf(pos.at(i));
        painter.drawRoundedRect(r.adjusted(bodyMargin, bodyMargin, -bodyMargin, -bodyMargin),
                                radius, radius);
    }

    // 2) 肚子色块：连续浅色带；遇到不相邻的节时断开（防止拉出长线）
    if (visible >= 2) {
        QPainterPath bellyPath;
        bellyPath.moveTo(cellRectOf(pos.at(1)).center());
        for (int i = 2; i < visible; ++i) {
            const QPoint a = body.at(i - 1);
            const QPoint b = body.at(i);
            if (manhattan(a, b) == 1) {
                bellyPath.lineTo(cellRectOf(pos.at(i)).center());
            } else {
                bellyPath.moveTo(cellRectOf(pos.at(i)).center());
            }
        }
        const QPen bellyPen(bellyColor, cell * 0.46, Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(bellyPen);
        painter.drawPath(bellyPath);
        painter.setPen(Qt::NoPen);
    }

    // 3) 蛇头
    if (visible >= 1) {
        const QRectF headRect = cellRectOf(pos.at(0));
        const QPointF c = headRect.center();

        painter.setBrush(headColor);
        painter.drawRoundedRect(headRect.adjusted(headMargin, headMargin, -headMargin, -headMargin),
                                radius, radius);

        // 3a) 舌头 / 分叉
        {
            const QPointF tip = c + front * (cell * 0.44);
            const QPointF b1 = c + front * (cell * 0.14) + perp * (cell * 0.17);
            const QPointF b2 = c + front * (cell * 0.14) - perp * (cell * 0.17);
            QPolygonF beak;
            beak << tip << b1 << b2;
            painter.setBrush(beakColor);
            painter.drawPolygon(beak);
        }

        // 3b) 单只
        {
            const QPointF e = c + front * (cell * 0.06) + perp * (cell * 0.20);
            const qreal eyeR = cell * 0.15;
            painter.setBrush(eyeIris);
            painter.drawEllipse(e, eyeR, eyeR);
            painter.setBrush(eyeDark);
            painter.drawEllipse(e, eyeR * 0.52, eyeR * 0.52);
            painter.setBrush(eyeShine);
            painter.drawEllipse(e + QPointF(-eyeR * 0.30, -eyeR * 0.30), eyeR * 0.24, eyeR * 0.24);
        }
    }

    // 4) 脚：蛇尾两侧各一只棕色小脚（仅完整显示时
    if (visible >= n && n >= 2) {
        const QPointF tc = cellRectOf(pos.at(n - 1)).center();
        const qreal footW = cell * 0.24;
        const qreal footH = cell * 0.16;
        const QPointF base = tc - front * (cell * 0.04);
        painter.setBrush(footColor);
        painter.drawRoundedRect(QRectF(base + perp * (cell * 0.30)
                                           - QPointF(footW / 2, footH / 2),
                                       QSizeF(footW, footH)),
                                footH * 0.5, footH * 0.5);
        painter.drawRoundedRect(QRectF(base - perp * (cell * 0.30)
                                           - QPointF(footW / 2, footH / 2),
                                       QSizeF(footW, footH)),
                                footH * 0.5, footH * 0.5);
    }

    painter.restore();
}

// 非运行状态绘制覆盖提示文字（就绪 / 暂停 / 结束）
void GameWidget::drawOverlayText(QPainter &painter, const QRect &board)
{
    if (m_state == GameState::Running) {
        return;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRoundedRect(board, 12, 12);

    QString text;
    switch (m_state) {
    case GameState::Ready:
        text = QStringLiteral("贪吃蛇\n难度：%1\n按 空格 或点击画面开始游戏")
                   .arg(m_config.name);
        break;
    case GameState::Paused:
        text = QStringLiteral("已暂停\n按 空格 继续");
        break;
    case GameState::GameOver:
        text = QStringLiteral("游戏结束\n\n分数：%1\n长度：%2\n存活时间：%3\n难度：%4\n\n按 R 或点击画面重新开始")
                   .arg(m_score)
                   .arg(m_snake.length())
                   .arg(formatTime(int(m_survivalSec)))
                   .arg(m_config.name);
        break;
    default:
        break;
    }

    painter.setPen(QColor(240, 240, 240));
    QFont font = painter.font();
    font.setPointSize(16);
    font.setBold(true);
    painter.setFont(font);

    painter.drawText(board, Qt::AlignCenter, text);
}
