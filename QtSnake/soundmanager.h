#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <QObject>
#include <QString>

class QMediaPlayer;
class QAudioOutput;
class QSoundEffect;

// 音频管理器：统一管理背景音乐与游戏音效
class SoundManager : public QObject
{
    Q_OBJECT

public:
    explicit SoundManager(QObject *parent = nullptr);

    void startMenuMusic();   // 主界面：循环播放菜单音乐（恋ひ恋ふ縁）
    void startGameMusic();   // 游戏内：循环播放原背景音乐
    void playEat();          // 吃到食物音效
    void playDeath();        // 游戏结束音效
    void playClick();        // 按钮点击音效

    void setMuted(bool muted); // 统一静音/恢复（背景音乐 + 音效）
    bool isMuted() const;

private:
    enum class Music { None, Menu, Game };
    void switchMusic(Music music, const QString &resource); // 切换当前背景音乐

    QMediaPlayer *m_musicPlayer = nullptr;  // 背景音乐播放器
    QAudioOutput *m_musicOutput = nullptr;  // 背景音乐音频输出

    QSoundEffect *m_eatEffect = nullptr;    // 吃食物音效
    QSoundEffect *m_deathEffect = nullptr;  // 游戏结束音效
    QSoundEffect *m_clickEffect = nullptr;  // 点击音效

    Music m_music = Music::None;            // 当前播放的背景音乐
    bool m_muted = false;
};

#endif // SOUNDMANAGER_H
