#include <QApplication>
#include <QSplashScreen>  // 启动画面类
#include <QPixmap>       // 图片加载
#include <QTimer>        // 延迟控制
#include <QElapsedTimer> // （可选）实际加载计时
#include <QScreen>
#include <QSoundEffect>
#include "soundmanager.h"
#include "mainwindow.h"  // 主窗口类

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    //背景音乐
    // 初始化音效管理器
    SoundManager::instance().init();

    // 播放背景音乐（循环播放）
    QSoundEffect* backgroundMusic = new QSoundEffect;
    backgroundMusic->setSource(QUrl("qrc:/sounds/background.wav"));
    backgroundMusic->setVolume(0.3);
    backgroundMusic->setLoopCount(QSoundEffect::Infinite);
    backgroundMusic->play();
    // 1. 加载启动图片并创建启动界面
    QPixmap splashPix(":/images/splash.png");  // 从资源文件加载图片
    if (splashPix.isNull()) {  // 检查图片是否加载成功（避免路径错误）
        qWarning() << "启动图片加载失败！请检查资源文件路径";
        return -1;
    }
    // 获取屏幕尺寸
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int screenWidth = screenGeometry.width();
    int screenHeight = screenGeometry.height();

    // 计算合适的Logo大小（例如屏幕宽度的15%）
    int maxLogoWidth = screenWidth * 0.55;
    int maxLogoHeight = screenHeight * 0.55;

    // 按比例调整大小
    QPixmap scaledLogo = splashPix.scaled(maxLogoWidth, maxLogoHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QSplashScreen splash(scaledLogo);  // 创建启动界面
    splash.setWindowTitle("斗地主 - 加载中");  // 窗口标题（可选）
    splash.show();  // 显示启动界面

    // 2. 让启动界面响应事件（避免界面卡顿）
    app.processEvents();  // 处理未完成的事件（如刷新界面）

    // 3. 执行实际的游戏初始化工作（代替单纯的延迟）
    // 例如：加载卡牌图片、初始化游戏规则、连接数据库等
    QElapsedTimer timer;
    timer.start();  // 计时，确保初始化完成后再关闭启动界面

    // 模拟初始化过程（实际项目中替换为真实逻辑）
    // 注意：若初始化耗时较长，建议放在子线程中，避免启动界面卡死
    while (timer.elapsed() < 2000) {  // 至少显示2秒（可根据实际初始化时间调整）
        app.processEvents();  // 持续响应事件，防止界面冻结
    }

    // 4. 初始化完成后，关闭启动界面并显示主窗口
    MainWindow w;  // 创建主窗口（此时不显示，等待启动界面关闭）

    // 关闭启动界面后立即显示主窗口
    QTimer::singleShot(0, &splash, [&]() {
        splash.close();  // 关闭启动界面
        w.show();        // 显示主窗口
    });
    return app.exec();
}
