#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QProcess>
#include "ImageProcessor.h"
#include "VideoProcessor.h"
#include <QMessageBox>
#include <QCloseEvent>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "退出程序", "确定要退出程序吗？",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            event->accept();
        } else {
            event->ignore();
        }
    }


private slots:
    // 图片处理相关槽函数
    void on_btn_browse_clicked();
    void on_btn_start_clicked();
    void on_btn_openDir_clicked();
    void on_video_btn_openDir_clicked();
    void on_checkBox_multiSelect_stateChanged(int state);
    void on_comboBox_imgType_currentIndexChanged(const QString &text);

    // 工具函数
    void showToast(const QString &message, int durationMs = 2000);
    void updateFileDisplay();
    void toggleImageControls(bool enabled);
    void toggleVideoControls(bool enabled);
    QString findExecutable(const QString& name);
    void on_btn_start_video_clicked();
    void on_video_btn_browse_clicked();

private:
    Ui::MainWindow *ui;
    QStringList m_selectedImageFiles;
    QString m_currentImageType = "png";
    ImageProcessor *m_imageProcessor; // 添加 ImageProcessor 成员变量
    VideoProcessor *m_videoProcessor;

    bool isSupportedImageFile(const QString &filePath);
    bool isSupportedVideoFile(const QString &filePath);
    void handleDroppedImage(const QString &filePath);
    void handleDroppedVideo(const QString &filePath);

    QString m_realesrganPath;
    QString m_ffmpegPath;
    QString m_ffprobePath;

    int m_filesProcessed = 0;
    int m_currentFileProgress = 0;
    int m_totalFiles = 0;
    // 初始化函数
    void initializeModules();
    void validateDependencies();

};
#endif // MAINWINDOW_H
