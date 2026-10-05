#include "mainwindow.h"

#include "gamewidget.h"
#include "soundmanager.h"

#include <QComboBox>
#include <QDialog>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

// 主窗口：两层界面（开始菜单 + 游戏界面），QStackedWidget 管理页面切换；
// 背景图片、声音系统、最高分、难度与规则等功能保持不变
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 加载背景图片（来自资源）
    m_background.load(QStringLiteral(":/res/background_menu.jpg"));

    setupUi();
    setupAudio();
    setupConnections();

    setWindowTitle(QStringLiteral("贪吃蛇 - Qt 6"));
    setWindowIcon(QIcon(QStringLiteral(":/res/snake.ico")));
    resize(1000, 720);

    // 全局深色样式（不设置背景色，让窗口背景图片透出）；
    // 末尾追加开始菜单样式：标题、副标题、半透明面板与放大的圆角按钮
    setStyleSheet(QStringLiteral(
        "QWidget { color: #e8e8e8; }"
        "QPushButton {"
        "  background-color: rgba(42, 47, 58, 220);"
        "  border: 1px solid #3a4050;"
        "  border-radius: 6px;"
        "  padding: 6px 14px;"
        "  color: #e8e8e8;"
        "}"
        "QPushButton:hover { background-color: rgba(52, 59, 72, 230); }"
        "QPushButton:pressed { background-color: rgba(64, 74, 92, 240); }"
        "QComboBox {"
        "  background-color: rgba(42, 47, 58, 220);"
        "  border: 1px solid #3a4050;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  color: #e8e8e8;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #2a2f3a;"
        "  color: #e8e8e8;"
        "  selection-background-color: #404a5c;"
        "}"
        "#infoBar { background-color: rgba(0, 0, 0, 110); border-radius: 8px; }"
        "#scoreLabel { font-size: 16px; font-weight: bold; color: #7ee08a; }"
        "#timeLabel { font-size: 16px; font-weight: bold; color: #7ec8ff; }"
        "#highScoreLabel { font-size: 16px; font-weight: bold; color: #f2c94c; }"
        // 开始菜单
        "#menuPanel { background-color: rgba(12, 15, 22, 175); border-radius: 16px; }"
        "#menuTitle { font-size: 42px; font-weight: bold; color: #7ee08a; }"
        "#menuSubtitle { font-size: 16px; letter-spacing: 5px; color: #8fb3d9; }"
        "#menuHighScore { font-size: 16px; font-weight: bold; color: #f2c94c; }"
        "#menuPanel QPushButton {"
        "  font-size: 18px;"
        "  min-width: 280px;"
        "  min-height: 38px;"
        "  border-radius: 9px;"
        "  padding: 8px 22px;"
        "}"
        "#menuPanel QPushButton:hover { background-color: rgba(52, 59, 72, 235); }"
        "#menuPanel QPushButton:pressed { background-color: rgba(64, 74, 92, 245); }"
        "#menuPrimaryButton {"
        "  font-size: 20px;"
        "  font-weight: bold;"
        "  background-color: rgba(46, 110, 62, 235);"
        "  border: 1px solid #4f9e63;"
        "}"
        "#menuPrimaryButton:hover { background-color: rgba(56, 132, 76, 245); }"
        "#menuPrimaryButton:pressed { background-color: rgba(38, 92, 52, 250); }"
        // 对话框使用浅色底 + 深色字（否则会继承上面的浅色文字而看不见）
        "QMessageBox { background-color: #f4f4f4; }"
        "QMessageBox QLabel { color: #202020; }"
        "QMessageBox QPushButton {"
        "  color: #202020; background-color: #e6e6e6;"
        "  border: 1px solid #b0b0b0; border-radius: 6px; padding: 4px 14px;"
        "}"));
}

// 组装页面容器：0=开始菜单，1=游戏界面
void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_pages = new QStackedWidget(central);
    m_menuPage = buildMenuPage();
    m_gamePage = buildGamePage();
    m_pages->addWidget(m_menuPage); // index 0
    m_pages->addWidget(m_gamePage); // index 1
    mainLayout->addWidget(m_pages);

    setCentralWidget(central);

    // 启动时显示开始菜单（不强制聚焦游戏区）
    m_pages->setCurrentWidget(m_menuPage);

    updateScoreLabels();
    updateMenuHighScore();
    // 声音按钮文字在 setupAudio() 中初始化（此时 SoundManager 尚未创建）
}

// 开始菜单页：标题 + 副标题 + 四个放大按钮 + 底部最高分
QWidget *MainWindow::buildMenuPage()
{
    auto *page = new QWidget;

    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(30, 24, 30, 16);
    outer->setSpacing(0);

    outer->addStretch(1);

    auto *panel = new QWidget(page);
    panel->setObjectName(QStringLiteral("menuPanel"));
    panel->setMaximumWidth(440);
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(32, 26, 32, 26);
    panelLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("贪 吃 蛇"), panel);
    title->setObjectName(QStringLiteral("menuTitle"));
    title->setAlignment(Qt::AlignCenter);

    auto *subtitle = new QLabel(QStringLiteral("SNAKE ADVENTURE"), panel);
    subtitle->setObjectName(QStringLiteral("menuSubtitle"));
    subtitle->setAlignment(Qt::AlignCenter);

    m_menuStartButton = new QPushButton(QStringLiteral("开始游戏"), panel);
    m_menuStartButton->setObjectName(QStringLiteral("menuPrimaryButton"));
    m_menuRulesButton = new QPushButton(QStringLiteral("游戏规则"), panel);
    m_menuSoundButton = new QPushButton(QStringLiteral("声音：开"), panel);
    m_menuExitButton = new QPushButton(QStringLiteral("退出"), panel);

    // 菜单按钮不抢占键盘焦点（保持与游戏页一致的输入习惯）
    m_menuStartButton->setFocusPolicy(Qt::NoFocus);
    m_menuRulesButton->setFocusPolicy(Qt::NoFocus);
    m_menuSoundButton->setFocusPolicy(Qt::NoFocus);
    m_menuExitButton->setFocusPolicy(Qt::NoFocus);
    m_menuStartButton->setCursor(Qt::PointingHandCursor);
    m_menuRulesButton->setCursor(Qt::PointingHandCursor);
    m_menuSoundButton->setCursor(Qt::PointingHandCursor);
    m_menuExitButton->setCursor(Qt::PointingHandCursor);

    panelLayout->addWidget(title);
    panelLayout->addWidget(subtitle);
    panelLayout->addSpacing(6);
    panelLayout->addWidget(m_menuStartButton);
    panelLayout->addWidget(m_menuRulesButton);
    panelLayout->addWidget(m_menuSoundButton);
    panelLayout->addWidget(m_menuExitButton);

    auto *panelRow = new QHBoxLayout();
    panelRow->addStretch(1);
    panelRow->addWidget(panel);
    panelRow->addStretch(1);
    outer->addLayout(panelRow);

    outer->addSpacing(18);

    m_menuHighScoreLabel = new QLabel(page);
    m_menuHighScoreLabel->setObjectName(QStringLiteral("menuHighScore"));
    m_menuHighScoreLabel->setAlignment(Qt::AlignCenter);
    outer->addWidget(m_menuHighScoreLabel);

    outer->addStretch(2);

    return page;
}

// 游戏界面页：顶部信息栏（分数/最高分/存活/难度 + 按钮组）+ 游戏区域
QWidget *MainWindow::buildGamePage()
{
    auto *page = new QWidget;

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // 顶部信息栏
    auto *infoBar = new QWidget(page);
    infoBar->setObjectName(QStringLiteral("infoBar"));
    auto *infoLayout = new QHBoxLayout(infoBar);
    infoLayout->setContentsMargins(10, 6, 10, 6);
    infoLayout->setSpacing(10);

    m_scoreLabel = new QLabel(infoBar);
    m_scoreLabel->setObjectName(QStringLiteral("scoreLabel"));

    m_highScoreLabel = new QLabel(infoBar);
    m_highScoreLabel->setObjectName(QStringLiteral("highScoreLabel"));

    m_timeLabel = new QLabel(infoBar);
    m_timeLabel->setObjectName(QStringLiteral("timeLabel"));
    m_timeLabel->setText(QStringLiteral("存活 00:00"));

    m_difficultyCombo = new QComboBox(infoBar);
    m_difficultyCombo->addItem(QStringLiteral("简单"));
    m_difficultyCombo->addItem(QStringLiteral("普通"));
    m_difficultyCombo->addItem(QStringLiteral("困难"));
    m_difficultyCombo->setCurrentIndex(1); // 默认普通

    m_startButton = new QPushButton(QStringLiteral("开始"), infoBar);
    m_pauseButton = new QPushButton(QStringLiteral("暂停"), infoBar);
    m_menuButton = new QPushButton(QStringLiteral("返回菜单"), infoBar);
    m_rulesButton = new QPushButton(QStringLiteral("规则"), infoBar);
    m_soundButton = new QPushButton(QStringLiteral("声音：开"), infoBar);

    // 按钮与下拉框不抢占键盘焦点，保证方向键始终到达游戏区域
    m_difficultyCombo->setFocusPolicy(Qt::NoFocus);
    m_startButton->setFocusPolicy(Qt::NoFocus);
    m_pauseButton->setFocusPolicy(Qt::NoFocus);
    m_menuButton->setFocusPolicy(Qt::NoFocus);
    m_rulesButton->setFocusPolicy(Qt::NoFocus);
    m_soundButton->setFocusPolicy(Qt::NoFocus);

    infoLayout->addWidget(m_scoreLabel);
    infoLayout->addWidget(m_highScoreLabel);
    infoLayout->addWidget(m_timeLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_difficultyCombo);
    infoLayout->addWidget(m_startButton);
    infoLayout->addWidget(m_pauseButton);
    infoLayout->addWidget(m_menuButton);
    infoLayout->addWidget(m_rulesButton);
    infoLayout->addWidget(m_soundButton);

    // 游戏区域（只创建一次，不随页面切换重建）
    m_gameWidget = new GameWidget(page);

    layout->addWidget(infoBar);
    layout->addWidget(m_gameWidget, 1); // 占满剩余空间

    return page;
}

// 初始化音频管理并开始播放菜单音乐（同时刷新声音按钮文字）
void MainWindow::setupAudio()
{
    m_soundManager = new SoundManager(this);
    m_soundManager->startMenuMusic(); // 主界面播放菜单音乐（恋ひ恋ふ縁）
    updateSoundButton();
}

// 建立控件与游戏/声音之间的信号槽（菜单页 + 游戏页；仅调用一次，避免重复连接）
void MainWindow::setupConnections()
{
    // 按钮统一处理：点击时先播放音效，再执行对应动作
    auto bindClick = [this](QPushButton *button, auto action) {
        connect(button, &QPushButton::clicked, this, [this, action]() {
            m_soundManager->playClick();
            action();
        });
    };

    // ---- 开始菜单 ----
    // 开始游戏：切到游戏页并立刻开局（不需要再点一次“开始”）
    bindClick(m_menuStartButton, [this]() {
        enterGamePage();
        m_gameWidget->startGame();
    });
    bindClick(m_menuRulesButton, [this]() { showRules(); });
    bindClick(m_menuExitButton, [this]() { close(); });

    // 菜单声音开关（与信息栏一致，统一静音逻辑 + 同步按钮文字）
    connect(m_menuSoundButton, &QPushButton::clicked, this, [this]() {
        const bool muted = !m_soundManager->isMuted();
        m_soundManager->setMuted(muted);
        if (!muted) {
            m_soundManager->playClick(); // 取消静音时给一个点击反馈
        }
        updateSoundButton();
    });

    // ---- 游戏信息栏 ----
    bindClick(m_startButton, [this]() { m_gameWidget->startGame(); });
    bindClick(m_pauseButton, [this]() { m_gameWidget->togglePause(); });

    // 声音开关（背景音乐 + 音效）
    connect(m_soundButton, &QPushButton::clicked, this, [this]() {
        const bool muted = !m_soundManager->isMuted();
        m_soundManager->setMuted(muted);
        if (!muted) {
            m_soundManager->playClick(); // 取消静音时给一个点击反馈
        }
        updateSoundButton();
    });

    // 难度切换
    connect(m_difficultyCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                const Difficulty d = (index == 0) ? Difficulty::Easy
                                   : (index == 2) ? Difficulty::Hard
                                                  : Difficulty::Normal;
                m_gameWidget->setDifficulty(d);
            });

    // 分数与最高分：任一变化都统一刷新两个标签；最高分额外同步到菜单页
    connect(m_gameWidget, &GameWidget::scoreChanged, this, &MainWindow::updateScoreLabels);
    connect(m_gameWidget, &GameWidget::highScoreChanged, this, &MainWindow::updateScoreLabels);
    connect(m_gameWidget, &GameWidget::highScoreChanged, this, &MainWindow::updateMenuHighScore);

    // 吃到食物音效
    connect(m_gameWidget, &GameWidget::foodEaten, this, [this]() {
        m_soundManager->playEat();
    });

    // Esc 退出确认
    connect(m_gameWidget, &GameWidget::exitRequested, this, &MainWindow::promptExit);

    // 点击画面开始/重开（播放点击音效）
    connect(m_gameWidget, &GameWidget::startRequested, this, [this]() {
        m_soundManager->playClick();
        m_gameWidget->startGame();
    });

    // 新开一局：切换到游戏背景音乐（按钮 / 点击画面 / 空格 / R 都会触发）
    connect(m_gameWidget, &GameWidget::gameStarted, this, [this]() {
        m_soundManager->startGameMusic();
    });

    // 状态变化：更新暂停按钮文字；锁定难度；游戏结束时播放音效
    connect(m_gameWidget, &GameWidget::stateChanged, this,
            [this](GameState state) {
                m_pauseButton->setText(state == GameState::Paused
                                           ? QStringLiteral("继续")
                                           : QStringLiteral("暂停"));
                // 进行中锁定难度选择，避免中途切换污染本局
                const bool locked = (state == GameState::Running || state == GameState::Paused);
                m_difficultyCombo->setEnabled(!locked);
                if (state == GameState::GameOver) {
                    m_soundManager->playDeath();
                }
            });

    // 存活时间显示（每秒刷新）
    connect(m_gameWidget, &GameWidget::survivalTimeChanged, this,
            [this](int seconds) {
                m_timeLabel->setText(QStringLiteral("存活 %1:%2")
                                         .arg(seconds / 60, 2, 10, QLatin1Char('0'))
                                         .arg(seconds % 60, 2, 10, QLatin1Char('0')));
            });

    // 返回菜单：回到就绪状态、切回菜单音乐并显示开始菜单页
    connect(m_menuButton, &QPushButton::clicked, this, [this]() {
        m_soundManager->playClick();
        enterMenuPage();
    });

    // 规则说明
    connect(m_rulesButton, &QPushButton::clicked, this, [this]() {
        m_soundManager->playClick();
        showRules();
    });
}

// 切换到游戏页并聚焦游戏区（保证方向键等键盘操作生效）
void MainWindow::enterGamePage()
{
    m_pages->setCurrentWidget(m_gamePage);
    m_gameWidget->setFocus();
}

// 返回开始菜单：回到就绪状态、切回菜单音乐并显示菜单页（不重建 GameWidget）
void MainWindow::enterMenuPage()
{
    m_gameWidget->returnToMenu();      // 回到就绪状态（不重建 GameWidget）
    m_soundManager->startMenuMusic();  // 切回菜单音乐
    updateMenuHighScore();             // 保留并刷新最高分
    m_pages->setCurrentWidget(m_menuPage);
}

// 窗口显示事件：仅当处于游戏页时才把焦点交给游戏区（菜单页不强制聚焦）
void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // 仅当处于游戏页时才聚焦游戏区，避免菜单页被强制聚焦导致键盘操作异常
    if (m_gameWidget && m_pages && m_pages->currentWidget() == m_gamePage) {
        m_gameWidget->setFocus();
    }
}

// 绘制窗口背景：按窗口尺寸缩放并居中裁剪铺满（缩放结果按目标尺寸缓存）
void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    if (m_background.isNull()) {
        // 背景图缺失时退回深色背景
        painter.fillRect(rect(), QColor(18, 20, 26));
        return;
    }

    // 仅在窗口尺寸变化时重新缩放（按目标尺寸判断，避免每帧重复缩放）
    if (m_scaledTarget != size()) {
        m_scaledBackground = m_background.scaled(size(),
                                                 Qt::KeepAspectRatioByExpanding,
                                                 Qt::SmoothTransformation);
        m_scaledTarget = size();
    }

    // 居中裁剪后绘制，铺满整个窗口
    const int x = (m_scaledBackground.width() - width()) / 2;
    const int y = (m_scaledBackground.height() - height()) / 2;
    painter.drawPixmap(rect(), m_scaledBackground, QRect(x, y, width(), height()));
}

// 刷新得分与最高分标签
void MainWindow::updateScoreLabels()
{
    m_scoreLabel->setText(QStringLiteral("得分 %1")
                              .arg(m_gameWidget->score(), 3, 10, QLatin1Char('0')));
    m_highScoreLabel->setText(QStringLiteral("最高分 %1")
                                  .arg(m_gameWidget->highScore(), 3, 10, QLatin1Char('0')));
}

// 刷新菜单页底部最高分（游戏过程中最高分变化也会同步）
void MainWindow::updateMenuHighScore()
{
    m_menuHighScoreLabel->setText(QStringLiteral("最高分：%1")
                                      .arg(m_gameWidget->highScore()));
}

// 依据静音状态刷新声音按钮文字（信息栏 + 菜单页同步）
void MainWindow::updateSoundButton()
{
    const QString text = m_soundManager->isMuted()
                             ? QStringLiteral("声音：关")
                             : QStringLiteral("声音：开");
    m_soundButton->setText(text);
    m_menuSoundButton->setText(text);
}

// Esc 退出确认弹窗（是关闭，否继续游戏）
void MainWindow::promptExit()
{
    // 弹窗期间暂停游戏，并记录弹窗前的运行状态
    const bool wasRunning = (m_gameWidget->state() == GameState::Running);
    if (wasRunning) {
        m_gameWidget->togglePause();
    }

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("退出游戏"));
    box.setIcon(QMessageBox::Question);
    box.setText(QStringLiteral("确定要退出游戏吗？"));

    QPushButton *closeButton = box.addButton(QStringLiteral("关闭"), QMessageBox::AcceptRole);
    QPushButton *continueButton = box.addButton(QStringLiteral("继续"), QMessageBox::RejectRole);
    box.setDefaultButton(continueButton);
    box.exec();

    if (box.clickedButton() == closeButton) {
        close(); // 关闭主窗口，退出程序
    } else {
        // 继续游戏：若之前正在运行则恢复运行
        if (wasRunning && m_gameWidget->state() == GameState::Paused) {
            m_gameWidget->togglePause();
        }
        if (m_pages->currentWidget() == m_gamePage) {
            m_gameWidget->setFocus(); // 焦点交还游戏区域，方向键继续可用
        }
    }
}

namespace {

// 与棋盘完全一致的特殊食物小图标（彩色圆 + 符号）
QPixmap makeSpecialFoodIcon(const QColor &color, const QString &mark, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    const qreal r = size * 0.5 - 1.0;
    p.drawEllipse(QPointF(size / 2.0, size / 2.0), r, r);
    p.setPen(QColor(20, 20, 20));
    QFont f = p.font();
    f.setPointSizeF(size * 0.42);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, mark);
    return pm;
}

} // namespace

// 弹出游戏规则对话框（弹窗期间暂停游戏，关闭后按原状态恢复）
void MainWindow::showRules()
{
    // 弹窗期间暂停游戏，并记录弹窗前的运行状态
    const bool wasRunning = (m_gameWidget->state() == GameState::Running);
    if (wasRunning) {
        m_gameWidget->togglePause();
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("游戏规则"));
    dlg.setMinimumWidth(460);
    dlg.setStyleSheet(QStringLiteral(
        "QDialog { background-color: #f4f4f4; }"
        "QLabel { color: #202020; }"
        "QPushButton { color: #202020; background-color: #e6e6e6;"
        "  border: 1px solid #b0b0b0; border-radius: 6px; padding: 5px 16px; }"));

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    // 基础玩法 / 操作 / 难度 / 奖励 / 事件（保持原样）
    auto *text1 = new QLabel(QStringLiteral(
        "【基础玩法】\n"
        "  · 目标：操控蛇吃食物得分并变长，避免撞墙 / 撞自己 / 撞 AI 蛇。\n"
        "  · 普通食物 +10 分、长度 +1，并让蛇小幅提速。\n"
        "  · 蛇会随存活时间逐渐提速（平滑过渡），越来越有挑战。\n\n"
        "【操作】\n"
        "  方向键 / WASD：移动\n"
        "  空格：开始 / 暂停 / 继续\n"
        "  R：重新开始      Shift（按住）：加速冲刺\n"
        "  点击画面：开始 / 重开      规则按钮：查看本说明\n\n"
        "【三档难度】\n"
        "  简单：速度慢、事件少；无 AI 蛇 / 传送门，护盾较多。\n"
        "  普通：速度中等、事件增多；有传送门，偶尔出现 AI 蛇。\n"
        "  困难：速度快、事件频繁；AI 蛇 / 移动墙 / 迷雾 / 洪水全开。\n\n"
        "【存活奖励】\n"
        "  30s +50、60s +100、90s +200、120s +500，之后每 30s 继续递增。\n\n"
        "【随机事件（按难度开放）】\n"
        "  额外食物 / 水潭扩大 / 随机墙 / 加速 / 传送门 /\n"
        "  迷雾 / 洪水 / 移动墙 / AI 蛇；\n"
        "  存活越久，危险事件出现概率越高。\n"), &dlg);
    text1->setTextFormat(Qt::PlainText);
    text1->setWordWrap(true);
    layout->addWidget(text1);

    // 特殊食物（道具）：图标 + 说明（图标与棋盘一致）
    layout->addWidget(new QLabel(QStringLiteral("【特殊食物（道具）】"), &dlg));

    struct Item { QColor color; QString mark; QString text; };
    const QList<Item> items = {
        { QColor(80, 200, 255),  QStringLiteral("⚡"), QStringLiteral("加速果：短时间提速") },
        { QColor(120, 180, 90),  QStringLiteral("🐢"), QStringLiteral("减速果：短时间降速") },
        { QColor(120, 220, 255), QStringLiteral("◆"), QStringLiteral("宝石：+50 分（存在时间短）") },
        { QColor(150, 80, 190),  QStringLiteral("☠"), QStringLiteral("毒果：长度 -2") },
        { QColor(255, 210, 80),  QStringLiteral("🛡"), QStringLiteral("护盾：抵挡一次致命碰撞") },
        { QColor(200, 120, 240), QStringLiteral("✨"), QStringLiteral("传送果：随机传送蛇头") },
    };

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(6);
    for (int i = 0; i < items.size(); ++i) {
        auto *icon = new QLabel(&dlg);
        icon->setPixmap(makeSpecialFoodIcon(items[i].color, items[i].mark, 26));
        icon->setFixedSize(26, 26);
        auto *lbl = new QLabel(items[i].text, &dlg);
        grid->addWidget(icon, i, 0, Qt::AlignVCenter);
        grid->addWidget(lbl, i, 1, Qt::AlignVCenter);
    }
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);

    // 场上元素
    auto *text2 = new QLabel(QStringLiteral(
        "【场上元素】\n"
        "  水潭：经过时减速（不改变方向操作）\n"
        "  随机墙 / 移动墙：撞到即失败\n"
        "  传送门：从一端进入，瞬间从另一端出现\n"
        "  AI 蛇：撞到它的身体会失败"), &dlg);
    text2->setTextFormat(Qt::PlainText);
    text2->setWordWrap(true);
    layout->addWidget(text2);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *ok = new QPushButton(QStringLiteral("知道了"), &dlg);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    buttons->addWidget(ok);
    layout->addLayout(buttons);

    dlg.exec();

    if (wasRunning && m_gameWidget->state() == GameState::Paused) {
        m_gameWidget->togglePause();
    }
    if (m_pages->currentWidget() == m_gamePage) {
        m_gameWidget->setFocus();
    }
}
