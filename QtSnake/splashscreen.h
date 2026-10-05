#ifndef SPLASHSCREEN_H
#define SPLASHSCREEN_H

#include <QWidget>
#include <QPixmap>

class QSoundEffect;
class QPropertyAnimation;
class QTimer;
class QPaintEvent;

// 启动画面：显示柚子社 logo 并播放品牌语音，结束后发出 finished() 进入主窗口
class SplashScreen : public QWidget
{
    Q_OBJECT

public:
    explicit SplashScreen(QWidget *parent = nullptr);
    ~SplashScreen() override;

signals:
    void finished();   // 启动画面播放完毕

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void start();          // 开始播放（淡入 + 语音）
    void maybeFinish();    // 满足“语音播完 且 已停留最短时间”后淡出
    void beginFadeOut();   // 淡出并在结束后发出 finished()

    QPixmap m_logo;

    QSoundEffect *m_voiceEffect = nullptr;

    QPropertyAnimation *m_fadeIn = nullptr;
    QPropertyAnimation *m_fadeOut = nullptr;

    QTimer *m_minDurationTimer = nullptr;  // 最短停留时间
    QTimer *m_tailTimer = nullptr;         // 语音播完后的尾部缓冲
    QTimer *m_timeoutTimer = nullptr;      // 兜底超时，防止语音加载失败时卡死

    bool m_voiceWasPlaying = false;
    bool m_voiceDone = false;
    bool m_minElapsed = false;
    bool m_finishing = false;
};

#endif // SPLASHSCREEN_H
