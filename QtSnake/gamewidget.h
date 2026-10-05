#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QElapsedTimer>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "food.h"
#include "gameconfig.h"
#include "snake.h"

class QTimer;

// 游戏状态
enum class GameState {
    Ready,    // 就绪，等待开始
    Running,  // 运行中
    Paused,   // 已暂停
    GameOver  // 游戏结束
};

// 特殊食物类型
enum class SpecialFoodType {
    SpeedUp,   // 加速果：短时间提高速度
    SlowDown,  // 减速果：短时间降低速度
    Gem,       // 宝石：高分，存在时间短
    Poison,    // 毒果：减少蛇长度
    Shield,    // 护盾：获得一次碰撞保护
    Teleport   // 传送果：随机传送蛇头
};

// 随机事件类型
enum class RandomEvent {
    ExtraFood,   // 额外食物
    WaterExpand, // 水潭扩大
    WallSpawn,   // 随机墙
    SpeedUp,     // 加速
    Portal,      // 传送门
    Fog,         // 迷雾
    Flood,       // 洪水
    MovingWall,  // 移动墙
    AISnake      // AI 蛇
};

// 场上的一颗特殊食物
struct SpecialFood {
    QPoint          pos;
    SpecialFoodType type = SpecialFoodType::Gem;
    qreal           ttl  = 0.0; // 剩余存在时间（秒），<=0 消失
};

// 传送来源：传送门 / 传送果共用统一传送入口，仅用于区分日志与结算细节
enum class TeleportSource {
    Portal,
    Fruit
};

// 传送失败原因（调试日志用，Release 下不输出）
enum class TeleportFailReason {
    None,
    OutOfBounds,        // 目标越界且无护盾
    BlockedByWall,      // 目标落在墙上且无护盾
    BlockedByMovingWall,// 目标落在移动墙上且无护盾
    BlockedBySnake,     // 目标落在蛇身上
    BlockedByAISnake,   // 目标落在 AI 蛇身上
    NeedsShieldNoShield,// 身体其它节落点越界/压墙但无护盾
    AlreadyTeleporting, // 已处于传送动画中
    NoFreeCell          // 传送果找不到合法落点
};

// 简单的 AI 蛇（只会朝附近食物前进）
struct AISnake {
    QList<QPoint> body;
    Direction     dir        = Direction::Right;
    qreal         moveAccum  = 0.0;
    int           intervalMs = 320;
};

// 游戏核心部件：负责绘制、计时、状态机、碰撞检测、计分与键盘输入
// 计时模型：QTimer 固定 ~16ms 只负责驱动画面刷新（渲染帧率），
// 蛇的逻辑移动由 QElapsedTimer 累计真实经过时间，达到 effectiveInterval() 才移动一格。
// 两者分离，逻辑速度不变，画面通过 previousBody/currentBody 插值实现平滑移动。
class GameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);

    // 网格尺寸
    static constexpr int kColumns = 30;
    static constexpr int kRows = 20;

    // 请求改变方向（由键盘事件调用，内部做反向/去重校验并排队）
    void requestDirection(Direction dir);

    // 当前游戏状态
    GameState state() const;

    // 当前分数
    int score() const;

    // 当前最高分
    int highScore() const;

    // 蛇长度
    int snakeLength() const;

    // 存活时间（秒，整数）
    int survivalTime() const;

    // 当前难度
    Difficulty difficulty() const;

public slots:
    void startGame();                          // 开始 / 重新开始
    void togglePause();                        // 暂停 / 继续
    void setDifficulty(Difficulty difficulty); // 设置难度（仅在本局开始前有效）
    void returnToMenu();                       // 返回就绪（菜单）状态

signals:
    void scoreChanged(int score);          // 分数变化
    void highScoreChanged(int highScore);  // 最高分变化
    void stateChanged(GameState state);    // 状态变化
    void foodEaten();                      // 吃到食物（用于播放音效）
    void exitRequested();                  // 请求退出（按 Esc）
    void startRequested();                 // 请求开始/重开（点击画面）
    void gameStarted();                    // 新开一局
    void survivalTimeChanged(int seconds); // 存活时间变化（每秒一次）

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void gameUpdate();     // 每帧调用：累计时间，必要时移动蛇，并刷新画面
    void gameOver();       // 处理游戏结束
    void speedUp();        // 吃到普通食物后加速（短期）
    void awardEat();       // 吃食物的统一结算（加分/音效/提速）
    void applyConfig();    // 依据当前难度刷新配置
    void clearFieldState();// 清空场上元素与各类计时（开始/返回菜单共用）

    int effectiveInterval() const;                 // 结合提速/减速/水潭后的实际间隔（毫秒）
    void updateDynamicSpeed(qreal dt);             // 依据存活时间平滑提速
    void resetGameTiming();                        // 重置时间累计与帧时钟（不改变 QTimer 间隔）

    // 存活时间与奖励
    void updateSurvival(qreal dt);
    void checkSurvivalReward();

    // 随机事件
    void updateRandomEvents(qreal dt);
    void triggerEvent(RandomEvent event);
    void showBanner(const QString &text);

    // 场上元素生成
    bool  isWater(const QPoint &p) const;         // 该格是否为水潭（含临时水潭）
    bool  cellIsFree(const QPoint &p) const;      // 该格是否可放置（避开蛇/墙/水/食物/传送门等）
    QList<QPoint> blockedCells() const;           // 当前所有障碍格（食物生成时避开）
    bool  randomFreeCell(QPoint &out);            // 随机取一个可放置的空格
    QList<QPoint> growBlob(int minCells, int maxCells); // 从种子生长出连通块
    void  spawnWalls(int count);
    void  spawnWater(int count);
    void  spawnSpecialFood();
    void  spawnPortals();
    void  spawnAISnake();
    void  applySpecialFood(SpecialFoodType type); // 应用特殊食物效果

    // 每帧更新
    void updateSpecialFoods(qreal dt);
    void updateAISnakes(qreal dt);
    void updateMovingWall(qreal dt);
    void updateTimers(qreal dt);                  // 护盾无敌、迷雾、洪水、横幅等计时
    void updateTeleport(qreal dt);                // 推进传送动画状态机
    bool tryPortalTeleport(const QPoint &next);   // 传送门触发：即将进入 / 已停在门上，统一入口
    bool canTeleportTo(const QPoint &target, bool allowWallPass,
                       TeleportFailReason *reason = nullptr) const; // 统一目标合法性检查
    bool executeTeleport(const QPoint &target, bool animated, TeleportSource source); // 统一传送：合法性+穿墙+护盾结算
    void applyTeleport(const QPoint &delta, bool needsShield); // 执行整体平移并结算护盾
    bool surviveLethalHit();                      // 护盾/无敌抵挡致命碰撞，返回是否存活
    void consumeShield();                         // 统一的护盾消耗入口（清护盾 + 触发短暂无敌）
    bool resolveMovementCollision(const QPoint &next, bool grow); // 移动碰撞判定（含穿墙/自撞/AI）
    void killAISnake(int idx);                    // AI 死亡：各节变普通食物并加分

    // 计算棋盘区域与格子边长（保持 3:2 比例居中）
    QRect boardRect() const;
    int cellSize() const;

    // 重建棋盘缓存（棋盘底板 + 边框 + 网格），仅在窗口尺寸变化时调用
    void rebuildBoardCache();

    // 绘制动态内容
    void drawFood(QPainter &painter, const QRect &board, int cell);
    void drawSpecialFoods(QPainter &painter, const QRect &board, int cell);
    void drawWalls(QPainter &painter, const QRect &board, int cell);
    void drawWater(QPainter &painter, const QRect &board, int cell);
    void drawPortals(QPainter &painter, const QRect &board, int cell);
    void drawAISnakes(QPainter &painter, const QRect &board, int cell);
    void drawSnake(QPainter &painter, const QRect &board, int cell);
    void drawFog(QPainter &painter, const QRect &board);
    void drawBanner(QPainter &painter, const QRect &board);
    void drawOverlayText(QPainter &painter, const QRect &board);

    QTimer *m_timer = nullptr;       // 固定 ~16ms 的画面刷新定时器
    Snake m_snake;                   // 蛇
    Food m_food;                     // 普通食物

    GameState m_state = GameState::Ready;
    Difficulty m_difficulty = Difficulty::Normal;
    DifficultyConfig m_config = getDifficultyConfig(Difficulty::Normal);

    int m_score = 0;                 // 当前分数
    int m_highScore = 0;             // 最高分
    int m_foodSpeedupMs = 0;         // 吃普通食物累计的提速（毫秒）
    qreal m_speedProgress = 0.0;     // 动态提速进度 0..1
    bool m_shiftHeld = false;        // 是否按住 Shift 加速

    // 每帧计算一次的有效移动间隔（毫秒），供逻辑移动与插值共用，避免重复计算
    int m_frameIntervalMs = 0;

    // 动态速度 / 存活
    qreal m_survivalSec = 0.0;       // 本局存活时间（秒，浮点）
    int   m_lastSurvivalWhole = -1;  // 已上报的整秒
    int   m_nextRewardIndex = 0;     // 下一个存活奖励节点索引
    qreal m_speedBoostTimer = 0.0;   // 加速果剩余时间
    qreal m_slowTimer = 0.0;         // 减速果剩余时间

    // 护盾 / 无敌
    bool  m_hasShield = false;
    qreal m_invincibleTimer = 0.0;

    // 传送门
    QPoint m_portalA;
    QPoint m_portalB;
    bool   m_portalsActive = false;
    // 防回传锁：传送后锁住出口格，蛇头未离开该格前不得再次从该格触发，
    // 防止 A→B→A→B 连环传送；蛇头一旦离开出口格即解锁，可正常再次传送。
    QPoint m_lastTeleportExit;
    bool   m_exitLockActive = false;

    // 传送动画状态机（动画渲染与逻辑坐标分离）
    enum class TeleportState { None, Exit, Enter };
    TeleportState m_tpState = TeleportState::None;
    qreal  m_tpProgress = 0.0;   // 当前阶段 0..1
    QPoint m_tpDelta;            // 逻辑坐标整体平移量
    QPoint m_tpEntryCell;        // 入口传送门格
    QPoint m_tpExitCell;         // 出口传送门格
    bool   m_tpNeedsShield = false; // 本次传送是否需要穿墙（决定是否消耗护盾）
    qreal  m_portalBurstTimer = 0.0;   // 入口爆发特效计时
    qreal  m_portalRippleTimer = 0.0;  // 出口波纹特效计时

    // 迷雾 / 临时水潭（事件）
    qreal m_fogTimer = 0.0;
    qreal m_tempWaterTimer = 0.0;

    // 移动墙
    QPoint  m_movingWall;
    Direction m_movingWallDir = Direction::Right;
    bool    m_movingWallActive = false;
    qreal   m_movingWallAccum = 0.0;

    // 随机事件管理
    qreal m_eventCountdown = 0.0;

    // 场上障碍/元素
    QList<QPoint>      m_walls;      // 随机墙
    QList<QPoint>      m_water;      // 水潭（减速）
    QList<QPoint>      m_tempWater;  // 事件临时扩大的水潭
    QList<QPoint>      m_extraFood;  // 事件产生的临时额外食物
    QList<SpecialFood> m_specialFoods;
    QList<AISnake>     m_aiSnakes;
    bool               m_pendingGameOver = false; // AI 撞死玩家时延迟结算

    qreal m_extraFoodTimer = 0.0;    // 额外食物剩余时间
    int   m_spawnSafeDist = 0;       // 开局生成时的安全距离（避免堵死蛇头）

    // 事件提示横幅
    QString m_banner;
    qreal   m_bannerTimer = 0.0;

    QElapsedTimer m_frameClock;      // 高精度帧时钟
    qint64 m_moveAccumulator = 0;    // 累计的真实经过时间（毫秒），用于逻辑移动

    QList<QPoint> m_previousBody;    // 上一次逻辑移动前的蛇身，用于插值

    QPixmap m_boardCache;            // 棋盘背景 + 网格缓存
    QSize m_boardCacheSize;          // 缓存对应的窗口尺寸

    QList<Direction> m_directionQueue; // 待处理的方向队列（防连按异常）

    static constexpr int kFrameIntervalMs = 16; // 渲染帧间隔（约 60FPS）
    static constexpr int kFoodCount = 15;    // 棋盘上同时存在的普通食物数量
    static constexpr int kAIMaxLen = 12;     // AI 蛇最大长度
    static constexpr int kMinInterval = 50;  // 速度下限
    static constexpr int kSpeedStep = 5;     // 每吃一个食物加快的毫秒数
    static constexpr int kMaxQueueSize = 3;  // 方向队列最大长度
    static constexpr int kBoostDivisor = 3;  // Shift 加速时把间隔除以该值
};

#endif // GAMEWIDGET_H
