#include "splashscreen.h"

#include <QApplication>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSoundEffect>
#include <QTimer>
#include <QUrl>

namespace {
constexpr int kWindowWidth   = 560;     // 启动画面宽度（高度按 logo 等比）
constexpr int kMinDurationMs = 1500;    // 最短停留时间
constexpr int kTailMs        = 350;     // 语音播完后的尾部缓冲
constexpr int kTimeoutMs     = 8000;    // 兜底超时（仅防止卡死，不截断语音）
constexpr int kFadeInMs      = 400;     // 淡入时长
constexpr int kFadeOutMs     = 300;     // 淡出时长
}

// 开场动画：加载柚子社 logo 图与开场语音
SplashScreen::SplashScreen(QWidget *parent)
    : QWidget(parent)
{
    // 无边框、置顶、不进任务栏的启动画面
    setWindowFlags(Qt::SplashScreen | Qt::WindowStaysOnTopHint);

    m_logo.load(QStringLiteral(":/res/yuzusoft_logo.png"));

    // 按 logo 比例确定窗口尺寸
    QSize windowSize(kWindowWidth, kWindowWidth);
    if (!m_logo.isNull() && m_logo.width() > 0) {
        const int h = qRound(double(kWindowWidth) * m_logo.height() / m_logo.width());
        windowSize = QSize(kWindowWidth, h);
    }
    setFixedSize(windowSize);

    // 居中到主屏幕
    if (QScreen *screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.center() - QPoint(width() / 2, height() / 2));
    }

    // 用 windowOpacity 做淡入淡出（顶层窗口最稳妥）
    setWindowOpacity(0.0);

    m_fadeIn = new QPropertyAnimation(this, "windowOpacity", this);
    m_fadeIn->setDuration(kFadeInMs);
    m_fadeIn->setStartValue(0.0);
    m_fadeIn->setEndValue(1.0);

    m_fadeOut = new QPropertyAnimation(this, "windowOpacity", this);
    m_fadeOut->setDuration(kFadeOutMs);
    m_fadeOut->setStartValue(1.0);
    m_fadeOut->setEndValue(0.0);
    connect(m_fadeOut, &QPropertyAnimation::finished, this, &SplashScreen::finished);

    // 品牌语音：使用 QSoundEffect 播放短 WAV，低延迟且会完整播放
    m_voiceEffect = new QSoundEffect(this);
    m_voiceEffect->setSource(QUrl(QStringLiteral("qrc:/res/yuzusoft_voice.wav")));
    m_voiceEffect->setVolume(1.0);
    connect(m_voiceEffect, &QSoundEffect::playingChanged, this, [this]() {
        if (m_voiceEffect->isPlaying()) {
            m_voiceWasPlaying = true;
        } else if (m_voiceWasPlaying) {
            // 语音真正播放结束，再等一小段尾部缓冲
            m_tailTimer->start();
        }
    });
    connect(m_voiceEffect, &QSoundEffect::statusChanged, this, [this]() {
        if (m_voiceEffect->status() == QSoundEffect::Error) {
            m_voiceDone = true;
            maybeFinish();
        }
    });

    // 最短停留时间
    m_minDurationTimer = new QTimer(this);
    m_minDurationTimer->setSingleShot(true);
    m_minDurationTimer->setInterval(kMinDurationMs);
    connect(m_minDurationTimer, &QTimer::timeout, this, [this]() {
        m_minElapsed = true;
        maybeFinish();
    });

    // 语音播完后的尾部缓冲
    m_tailTimer = new QTimer(this);
    m_tailTimer->setSingleShot(true);
    m_tailTimer->setInterval(kTailMs);
    connect(m_tailTimer, &QTimer::timeout, this, [this]() {
        m_voiceDone = true;
        maybeFinish();
    });

    // 兜底超时
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(kTimeoutMs);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        m_voiceDone = true;
        m_minElapsed = true;
        maybeFinish();
    });

    start();
}

// 默认析构
SplashScreen::~SplashScreen() = default;

// 启动开场动画：显示 logo 并播放语音
void SplashScreen::start()
{
    m_fadeIn->start();
    m_minDurationTimer->start();
    m_timeoutTimer->start();
    m_voiceEffect->play();
}

// 语音播放完毕后判断是否可以进入淡出
void SplashScreen::maybeFinish()
{
    if (m_finishing || !m_voiceDone || !m_minElapsed) {
        return;
    }
    beginFadeOut();
}

// 开始整体淡出，结束后关闭开场窗口
void SplashScreen::beginFadeOut()
{
    if (m_finishing) {
        return;
    }
    m_finishing = true;
    m_minDurationTimer->stop();
    m_tailTimer->stop();
    m_timeoutTimer->stop();
    m_fadeOut->start();
}

// 绘制 logo 图片（等比缩放居中）
void SplashScreen::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 不透明深色背景
    painter.fillRect(rect(), QColor(16, 18, 24));

    if (m_logo.isNull()) {
        painter.setPen(QColor(230, 230, 230));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("logo load failed"));
        return;
    }

    const QSize target = m_logo.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect targetRect(QPoint((width() - target.width()) / 2,
                                  (height() - target.height()) / 2),
                           target);
    painter.drawPixmap(targetRect, m_logo);
}
