#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <QObject>
#include <QProcess>
#include <QRegularExpression>

class ImageProcessor : public QObject
{
    Q_OBJECT
public:
    explicit ImageProcessor(QObject *parent = nullptr, bool noWindow = false);
    void setNoWindow(bool noWindow);
    void processImages(const QStringList &inputPaths,
                       const QString &modelName,
                       const QString &outputFormat,
                       bool openOutputDirectory);

signals:
    void processingFinished(const QStringList &outputFiles);
    void errorOccurred(const QString &message);
    void progressUpdate(int percentage, const QString &status);
    void fileProcessed();

private slots:
    void handleRealESRGANFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void handleProcessOutput();


private:
    void processNextImage();
    void convertImageFormat(const QString &inputPath, const QString &outputPath);
    void openOutputDirectory(const QString &path);

    QString m_realESRGANExecutable = "realesrgan-ncnn-vulkan.exe";
    QString m_ffmpegExecutable = "ffmpeg.exe";
    bool m_noWindow;
    QStringList m_remainingInputPaths;
    QStringList m_outputFiles;
    QString m_currentModelName;
    QString m_currentOutputFormat;
    bool m_openOutputDirectory;
    QProcess *m_currentProcess;

};

#endif // IMAGEPROCESSOR_H
