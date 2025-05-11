TEMPLATE = app
TARGET = qtRealSR_GUI
QT += core gui widgets
CONFIG += c++17 static
CONFIG += static
DEFINES += QT_NO_NETWORK \
           QT_NO_MULTIMEDIA \
           QT_NO_SVG \
           QT_NO_QUICK


static {
    DEFINES += QT_STATICPLUGIN
    QMAKE_LFLAGS += -static
}

VERSION = 1.0
QT_MAJOR_VERSION = $$split(QT_VERSION, ".")0

# 源文件和头文件
SOURCES += \
    VideoProcessor.cpp \
    main.cpp \
    mainwindow.cpp \
    ImageProcessor.cpp

HEADERS += \
    VideoProcessor.h \
    mainwindow.h \
    ImageProcessor.h

# UI 文件
FORMS += mainwindow.ui


# 资源文件（包含图标）
RESOURCES += res.qrc

win32 {
    RC_ICONS = "icons/logo.ico"
    LIBS += -lwinmm -lws2_32 -liphlpapi -luser32 -lgdi32 -ladvapi32 -lshell32
}


greaterThan(QT_MAJOR_VERSION, 5) {
    message("Using Qt6...")
} else {
    message("Using Qt5...")
}
