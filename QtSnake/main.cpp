#include "mainwindow.h"
#include "splashscreen.h"

#include <QApplication>

// 程序入口：先展示启动画面（柚子社 logo + 语音），结束后再创建并显示主窗口
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 先显示启动画面（柚子社 logo + 日语语音），结束后再创建主窗口，
    // 这样背景音乐不会盖住 logo 语音。
    SplashScreen splash;
    splash.show();

    QObject::connect(&splash, &SplashScreen::finished, &app, [&splash]() {
        auto *window = new MainWindow;
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
        splash.close();
    });

    return app.exec();
}
