#include "soundmanager.h"
#include <QDebug>

SoundManager::SoundManager(QObject* parent)
    : QObject(parent) {
}

SoundManager::~SoundManager() {
    qDeleteAll(m_sounds.values());
    m_sounds.clear();
    if (m_initBackgroundSound) {
        m_initBackgroundSound->deleteLater();
    }
}

SoundManager& SoundManager::instance() {
    static SoundManager manager;
    return manager;
}

void SoundManager::init() {
    // 1. 加载初始化背景音并淡入
    loadInitBackgroundSound();
    fadeInInitBackground();

    // 2. 加载游戏内音效
    loadSound("deal", ":/sounds/deal.wav");          // 发牌音效
    loadSound("bid", ":/sounds/bid.wav");            // 叫地主音效
    loadSound("pass", ":/sounds/pass.wav");          // 不要音效
    loadSound("play", ":/sounds/play.wav");          // 出牌音效
    loadSound("win", ":/sounds/win.wav");            // 胜利音效
    loadSound("lose", ":/sounds/lose.wav");          // 失败音效
    loadSound("background", ":/sounds/background.wav"); // 游戏背景音（先不自动播放）
}

void SoundManager::loadSound(const QString& soundName, const QString& filePath) {
    QSoundEffect* effect = new QSoundEffect(this);
    effect->setSource(QUrl(filePath));  // 修正：使用qrc资源路径，而非本地文件
    effect->setVolume(m_volume);
    m_sounds[soundName] = effect;
}

void SoundManager::loadInitBackgroundSound() {
    m_initBackgroundSound = new QSoundEffect(this);
    m_initBackgroundSound->setSource(QUrl("qrc:/sounds/init_background.wav"));
    m_initBackgroundSound->setLoopCount(QSoundEffect::Infinite);  // 循环播放
    m_initBackgroundSound->setVolume(0.0);  // 初始音量0，准备淡入
}

void SoundManager::fadeInInitBackground(qreal duration) {
    if (!m_initBackgroundSound) return;
    QPropertyAnimation* fadeAnim = new QPropertyAnimation(m_initBackgroundSound, "volume");
    fadeAnim->setDuration(duration);
    fadeAnim->setStartValue(0.0);
    fadeAnim->setEndValue(0.3);  // 初始化背景音音量较低，营造轻柔氛围
    fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
    m_initBackgroundSound->play();
}

void SoundManager::fadeOutInitBackground(qreal duration) {
    if (!m_initBackgroundSound) return;
    QPropertyAnimation* fadeAnim = new QPropertyAnimation(m_initBackgroundSound, "volume");
    fadeAnim->setDuration(duration);
    fadeAnim->setStartValue(m_initBackgroundSound->volume());
    fadeAnim->setEndValue(0.0);
    fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
    connect(fadeAnim, &QPropertyAnimation::finished,
            m_initBackgroundSound, &QSoundEffect::stop);
}

void SoundManager::playSound(const QString& soundName) {
    if (!m_enabled) return;
    if (m_sounds.contains(soundName)) {
        m_sounds[soundName]->play();
    } else {
        qWarning() << "Sound not found:" << soundName;
    }
}

void SoundManager::playLoadTick() {
    QSoundEffect* tick = new QSoundEffect(this);
    tick->setSource(QUrl("qrc:/sounds/load_tick.wav"));
    tick->setVolume(m_volume * 0.5);  // 加载反馈音音量稍低
    tick->play();
    connect(tick, &QSoundEffect::playingChanged, [tick]() {
        if (!tick->isPlaying()) {
            tick->deleteLater();
        }
    });
}

void SoundManager::switchToGameBackground() {
    if (!m_sounds.contains("background")) {
        qWarning() << "Game background sound not loaded!";
        return;
    }
    m_gameBackgroundSound = m_sounds["background"];
    m_gameBackgroundSound->setLoopCount(QSoundEffect::Infinite);
    m_gameBackgroundSound->setVolume(0.0);
    m_gameBackgroundSound->play();

    // 淡入游戏背景音
    QPropertyAnimation* gameFadeIn = new QPropertyAnimation(m_gameBackgroundSound, "volume");
    gameFadeIn->setDuration(3000);
    gameFadeIn->setStartValue(0.0);
    gameFadeIn->setEndValue(0.5);
    gameFadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    // 淡出初始化背景音
    fadeOutInitBackground();
}

void SoundManager::setVolume(qreal volume) {
    m_volume = volume;
    foreach (QSoundEffect* effect, m_sounds.values()) {
        effect->setVolume(volume);
    }
    if (m_initBackgroundSound) {
        m_initBackgroundSound->setVolume(m_initBackgroundSound->volume() * volume / 0.7);
    }
}

void SoundManager::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled) {
        if (m_initBackgroundSound) m_initBackgroundSound->stop();
        if (m_gameBackgroundSound) m_gameBackgroundSound->stop();
    } else {
        if (m_initBackgroundSound) m_initBackgroundSound->play();
        if (m_gameBackgroundSound) m_gameBackgroundSound->play();
    }
}
