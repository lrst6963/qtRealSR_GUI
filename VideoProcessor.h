#ifndef VIDEOPROCESSOR_H
#define VIDEOPROCESSOR_H

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QUuid>

class VideoProcessor : public QObject
{
    Q_OBJECT

public:
    explicit VideoProcessor(QObject *parent = nullptr);
    ~VideoProcessor();

    void setExecutablePaths(const QString &realesrganPath, const QString &ffmpegPath, const QString &ffprobePath);
    void processVideo(const QString &inputPath, const QString &modelName, int scaleFactor,
                      const QString &outputFormat, bool openOutputDirectory);

    void cancelProcessing();

signals:
    void progressUpdated(const QString &message);
    void progressPercentageChanged(double percent);
    void errorOccurred(const QString &error);
    void processingFinished(const QString &outputPath);

private slots:
    void handleRealesrganOutput();
    void handleRealesrganFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleFfmpegOutput();
    void handleFfmpegFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    struct VideoProcessingOptions {
        QString inputPath;
        QString modelName;
        int scaleFactor;
        QString outputFormat;
        bool openOutputDirectory;
    };

    void executePipeline();
    void extractVideoFrames();
    void enhanceFrames();
    void rebuildVideo();
    void cleanupTempFiles();

    QString getVideoMetadata();
    QString parseFrameRate(const QString &output);
    QString createTempDirectory();
    QString generateOutputPath();
    void updateProgress(int processed, int total);

    QProcess *m_realesrganProcess;
    QProcess *m_ffmpegProcess;
    QProcess *m_ffprobeProcess;

    QString m_realesrganPath;
    QString m_ffmpegPath;
    QString m_ffprobePath;

    QString m_tempDir;
    QString m_frameDir;
    QString m_enhancedDir;
    QString m_outputPath;
    QString m_fps;

    int m_totalFrames;
    int m_processedFrames;

    VideoProcessingOptions m_options;
    bool m_cancelled;

    QDateTime m_lastProgressTime;
    bool m_processingCompleted = false;
    QTimer* m_progressTimer = nullptr;
};

#endif // VIDEOPROCESSOR_H
