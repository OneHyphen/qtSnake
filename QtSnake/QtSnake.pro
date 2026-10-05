QT += widgets multimedia

CONFIG += c++17

TARGET = QtSnake
TEMPLATE = app

# 关闭已废弃 API 警告（Qt 6 专用）
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    gamewidget.cpp \
    snake.cpp \
    food.cpp \
    soundmanager.cpp \
    splashscreen.cpp

HEADERS += \
    gameconfig.h \
    mainwindow.h \
    gamewidget.h \
    snake.h \
    food.h \
    soundmanager.h \
    splashscreen.h

RESOURCES += \
    resources.qrc

# Windows 可执行文件图标
win32: RC_ICONS = res/snake.ico

# 默认部署规则
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
