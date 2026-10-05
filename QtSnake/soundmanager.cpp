#include "soundmanager.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QSoundEffect>
#include <QUrl>

// 创建各音效 / 音乐播放器并加载资源
SoundManager::SoundManager(QObject *parent)
    : QObject(parent)
{
    // 背景音乐：循环播放资源中的 MP3
    m_musicOutput = new QAudioOutput(this);
    m_musicOutput->setVolume(0.5);

    m_musicPlayer = new QMediaPlayer(this);
    m_musicPlayer->setAudioOutput(m_musicOutput);
    m_musicPlayer->setLoops(QMediaPlayer::Infinite);

    // 音效：使用 QSoundEffect 播放短 WAV，低延迟且可重叠
    m_eatEffect = new QSoundEffect(this);
    m_eatEffect->setSource(QUrl(QStringLiteral("qrc:/res/eat.wav")));
    m_eatEffect->setVolume(0.8);

    m_deathEffect = new QSoundEffect(this);
    m_deathEffect->setSource(QUrl(QStringLiteral("qrc:/res/death.wav")));
    m_deathEffect->setVolume(0.8);

    m_clickEffect = new QSoundEffect(this);
    m_clickEffect->setSource(QUrl(QStringLiteral("qrc:/res/click.wav")));
    m_clickEffect->setVolume(0.6);
}

// 播放菜单背景音乐（恋ひ恋ふ縁）
void SoundManager::startMenuMusic()
{
    switchMusic(Music::Menu, QStringLiteral("qrc:/res/menu_song.mp3"));
}

// 切换到游戏内背景音乐
void SoundManager::startGameMusic()
{
    switchMusic(Music::Game, QStringLiteral("qrc:/res/game_bgm.mp3"));
}

// 带淡出的音乐切换，避免声音戛然而止
void SoundManager::switchMusic(Music music, const QString &resource)
{
    if (m_music == music) {
        return; // 已经在播放同一首，避免重头开始
    }
    m_music = music;
    m_musicPlayer->setSource(QUrl(resource));
    if (!m_muted) {
        m_musicPlayer->play();
    }
}

// 播放吃食物音效（奶龙大笑，截取 2 秒）
void SoundManager::playEat()
{
    if (!m_muted) {
        m_eatEffect->play();
    }
}

// 播放游戏结束音效
void SoundManager::playDeath()
{
    if (!m_muted) {
        m_deathEffect->play();
    }
}

// 播放按钮点击音效
void SoundManager::playClick()
{
    if (!m_muted) {
        m_clickEffect->play();
    }
}

// 设置静音（同时作用于音乐与音效）
void SoundManager::setMuted(bool muted)
{
    m_muted = muted;

    // 背景音乐：暂停并静音输出
    m_musicOutput->setMuted(muted);
    if (muted) {
        m_musicPlayer->pause();
    } else {
        m_musicPlayer->play();
    }

    // 音效：统一静音
    m_eatEffect->setMuted(muted);
    m_deathEffect->setMuted(muted);
    m_clickEffect->setMuted(muted);
}

// 当前是否静音
bool SoundManager::isMuted() const
{
    return m_muted;
}
