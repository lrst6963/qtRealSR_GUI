#include "mainwindow.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QToolBar>
#include <QMessageBox>
#include <QComboBox>
#include <QGuiApplication>
#include <QStyleHints>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    MainWindow w;
    w.setWindowIcon(QIcon(":/icons/logo.ico"));

    QToolBar *toolBar = new QToolBar(&w);
    w.addToolBar(toolBar);
    toolBar->setMovable(false);

    QComboBox *themeComboBox = new QComboBox(&w);
    themeComboBox->addItem("深色模式");
    themeComboBox->addItem("浅色模式");
    int initialThemeIndex = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? 0 : 1;
    themeComboBox->setCurrentIndex(initialThemeIndex);
    #if defined(Q_OS_WIN)
        if (QSysInfo::productVersion() <= "10.0.22000") {
            themeComboBox->setVisible(false);
        } else {
            themeComboBox->setVisible(true);
        }
    #elif defined(Q_OS_LINUX)
        themeComboBox->setVisible(false);
    #else
        themeComboBox->setVisible(false);
    #endif

    QObject::connect(themeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     [](int index) {
                         bool darkModeEnabled = (index == 0);
                         QGuiApplication::styleHints()->setColorScheme(
                             darkModeEnabled ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light
                             );
                     });

    QAction *exitAction = new QAction("退出", &w);
    QAction *aboutAction = new QAction("关于", &w);

    toolBar->addWidget(themeComboBox);
    toolBar->addSeparator();
    toolBar->addAction(exitAction);
    toolBar->addAction(aboutAction);

    QObject::connect(exitAction, &QAction::triggered, &w, [&w]() {
            QApplication::quit();
    });

    QObject::connect(aboutAction, &QAction::triggered, &w, [&w]() {
        QMessageBox msgBox(&w);
        msgBox.setWindowTitle("关于 RealSR_GUI");
        msgBox.setTextFormat(Qt::RichText);

        QString aboutText =
            "<div style='text-align: center;'>"
            "<img src=':/icons/logo.ico' width='128' height='128'><br>"
            "<h1 style='font-size: 24pt; font-weight: bold; margin: 5px;'>RealSR_GUI</h1>"
            "<p style='font-size: 14pt; margin: 5px;'>版本 1.0.0</p>"
            "<p style='font-size: 14pt; margin: 5px;'>基于 Real-ESRGAN 的图像/视频超分辨率工具</p>"
            "</div>";
        msgBox.setText(aboutText);
        msgBox.setIconPixmap(QPixmap());
        msgBox.exec();
    });

    w.show();
    return a.exec();
}
