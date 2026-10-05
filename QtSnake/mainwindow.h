#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPixmap>

class QLabel;
class QPushButton;
class QComboBox;
class QShowEvent;
class QPaintEvent;
class QStackedWidget;
class GameWidget;
class SoundManager;

// 主窗口：两层界面结构（开始菜单 + 游戏界面），QStackedWidget 管理页面切换；
// 背景图片、声音系统、最高分、难度与规则等功能保持不变
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;   // 显示时聚焦游戏区，避免键盘焦点丢失
    void paintEvent(QPaintEvent *event) override; // 绘制窗口背景图片

private:
    void setupUi();          // 构建界面（菜单页 + 游戏页）
    QWidget *buildMenuPage();// 构建开始菜单页
    QWidget *buildGamePage();// 构建游戏界面页（信息栏 + 游戏区）
    void setupAudio();       // 初始化音频
    void setupConnections(); // 建立信号槽连接
    void enterGamePage();    // 切换到游戏界面并聚焦游戏区
    void enterMenuPage();    // 切换回开始菜单
    void updateScoreLabels(); // 更新分数与最高分显示
    void updateSoundButton(); // 更新声音按钮文字（信息栏 + 菜单页同步）
    void updateMenuHighScore(); // 更新菜单页最高分显示
    void promptExit();        // Esc 退出确认对话框
    void showRules();         // 显示游戏规则对话框

    GameWidget *m_gameWidget = nullptr;
    SoundManager *m_soundManager = nullptr;

    QStackedWidget *m_pages = nullptr; // 页面容器：0=开始菜单，1=游戏界面
    QWidget *m_menuPage = nullptr;     // 开始菜单页
    QWidget *m_gamePage = nullptr;     // 游戏界面页（信息栏 + 游戏区）

    QLabel *m_menuHighScoreLabel = nullptr; // 菜单页底部最高分
    QPushButton *m_menuStartButton = nullptr;
    QPushButton *m_menuRulesButton = nullptr;
    QPushButton *m_menuSoundButton = nullptr;
    QPushButton *m_menuExitButton = nullptr;

    QLabel *m_scoreLabel = nullptr;
    QLabel *m_highScoreLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QComboBox *m_difficultyCombo = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_soundButton = nullptr;
    QPushButton *m_menuButton = nullptr;
    QPushButton *m_rulesButton = nullptr;

    QPixmap m_background;        // 原始背景图片
    QPixmap m_scaledBackground;  // 按窗口尺寸缩放后的背景图片缓存
    QSize m_scaledTarget;        // 缓存对应的目标尺寸
};

#endif // MAINWINDOW_H
