#include "VideoProcessor.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QApplication>

VideoProcessor::VideoProcessor(QObject *parent) : QObject(parent),
    m_realesrganProcess(nullptr),
    m_ffmpegProcess(nullptr),
    m_ffprobeProcess(nullptr),
    m_totalFrames(0),
    m_processedFrames(0),
    m_cancelled(false)
{
    // 默认路径
#ifdef Q_OS_WIN
    m_realesrganPath = "realesrgan-ncnn-vulkan.exe";
    m_ffmpegPath = "ffmpeg.exe";
    m_ffprobePath = "ffprobe.exe";
#else
    m_realesrganPath = "realesrgan-ncnn-vulkan";
    m_ffmpegPath = "ffmpeg";
    m_ffprobePath = "ffprobe";
#endif
}

VideoProcessor::~VideoProcessor()
{
    cancelProcessing();
    cleanupTempFiles();
}

void VideoProcessor::setExecutablePaths(const QString &realesrganPath, const QString &ffmpegPath, const QString &ffprobePath)
{
    m_realesrganPath = realesrganPath;
    m_ffmpegPath = ffmpegPath;
    m_ffprobePath = ffprobePath;
}

void VideoProcessor::processVideo(const QString &inputPath, const QString &modelName,
                                  int scaleFactor, const QString &outputFormat,
                                  bool openOutputDirectory)
{
    m_options.inputPath = inputPath;
    m_options.modelName = modelName;
    m_options.scaleFactor = scaleFactor;
    m_options.outputFormat = outputFormat;
    m_options.openOutputDirectory = openOutputDirectory;
    m_cancelled = false;

    // 创建临时目录
    m_tempDir = createTempDirectory();
    m_frameDir = QDir(m_tempDir).filePath("frames");
    m_enhancedDir = QDir(m_tempDir).filePath("enhanced");

    QDir().mkpath(m_frameDir);
    QDir().mkpath(m_enhancedDir);

    executePipeline();
}

void VideoProcessor::cancelProcessing()
{
    m_cancelled = true;

    if (m_realesrganProcess && m_realesrganProcess->state() == QProcess::Running) {
        m_realesrganProcess->terminate();
        m_realesrganProcess->waitForFinished(1000);
        m_realesrganProcess->kill();
    }

    if (m_ffmpegProcess && m_ffmpegProcess->state() == QProcess::Running) {
        m_ffmpegProcess->kill();
    }

    if (m_ffprobeProcess && m_ffprobeProcess->state() == QProcess::Running) {
        m_ffprobeProcess->kill();
    }

    if (m_progressTimer) {
        m_progressTimer->stop();
        m_progressTimer->deleteLater();
        m_progressTimer = nullptr;
    }
}

void VideoProcessor::executePipeline()
{
    emit progressUpdated("正在提取视频元数据...");
    m_fps = getVideoMetadata();

    if (m_fps.isEmpty()) {
        emit errorOccurred("无法获取视频帧率信息");
        return;
    }

    extractVideoFrames();
}

void VideoProcessor::extractVideoFrames()
{
    emit progressUpdated("正在提取视频帧...");

    if (m_ffmpegProcess) {
        m_ffmpegProcess->deleteLater();
    }

    m_ffmpegProcess = new QProcess(this);
    connect(m_ffmpegProcess, &QProcess::readyReadStandardOutput, this, &VideoProcessor::handleFfmpegOutput);
    connect(m_ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VideoProcessor::handleFfmpegFinished);

    QStringList args;
    args << "-i" << m_options.inputPath
         << "-qscale:v" << "1"
         << "-qmin" << "1"
         << "-qmax" << "1"
         << "-vsync" << "0"
         << QDir(m_frameDir).filePath("frame%08d.png");

    m_ffmpegProcess->start(m_ffmpegPath, args);
}

void VideoProcessor::enhanceFrames() {
    emit progressUpdated("正在增强视频帧...");

    // 重置状态
    m_processingCompleted = false;
    m_lastProgressTime = QDateTime::currentDateTime();

    // 清理旧进程
    if (m_realesrganProcess) {
        m_realesrganProcess->deleteLater();
    }

    // 初始化新进程
    m_realesrganProcess = new QProcess(this);
    connect(m_realesrganProcess, &QProcess::readyReadStandardOutput,
            this, &VideoProcessor::handleRealesrganOutput);
    connect(m_realesrganProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VideoProcessor::handleRealesrganFinished);

    // 准备参数
    QStringList args;
    args << "-i" << m_frameDir
         << "-o" << m_enhancedDir
         << "-n" << m_options.modelName
         << "-s" << QString::number(m_options.scaleFactor)
         << "-f" << m_options.outputFormat;

    m_realesrganProcess->start(m_realesrganPath, args);

    // 初始化帧数监控
    m_totalFrames = QDir(m_frameDir).entryList({"*.png"}, QDir::Files).count();
    m_processedFrames = 0;

    // 清理旧定时器
    if (m_progressTimer) {
        m_progressTimer->stop();
        m_progressTimer->deleteLater();
    }

    // 创建新定时器
    m_progressTimer = new QTimer(this);
    connect(m_progressTimer, &QTimer::timeout, this, [this]() {
        if (m_cancelled || m_processingCompleted) return;

        // 获取当前已处理帧数
        int newCount = QDir(m_enhancedDir)
                           .entryList({"*." + m_options.outputFormat}, QDir::Files)
                           .count();

        // 更新进度
        if (newCount > m_processedFrames) {
            m_processedFrames = newCount;
            m_lastProgressTime = QDateTime::currentDateTime();
            updateProgress(m_processedFrames, m_totalFrames);
        }

        // 检查是否完成
        if (m_processedFrames >= m_totalFrames) {
            m_progressTimer->stop();
            m_processingCompleted = true;
            return;
        }

        // 超时检测（30秒）
        if (m_lastProgressTime.secsTo(QDateTime::currentDateTime()) > 30) {
            emit errorOccurred("处理超时，30秒内无新进度");
            cancelProcessing();
            m_progressTimer->stop();
        }
    });

    m_progressTimer->start(500); // 每0.5秒检查一次
}


void VideoProcessor::rebuildVideo() {
    emit progressUpdated("正在合并视频...");

    if (m_ffmpegProcess) {
        m_ffmpegProcess->deleteLater();
    }

    m_ffmpegProcess = new QProcess(this);
    connect(m_ffmpegProcess, &QProcess::readyReadStandardOutput, this, &VideoProcessor::handleFfmpegOutput);
    connect(m_ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VideoProcessor::handleFfmpegFinished);

    m_outputPath = generateOutputPath();

    QStringList args;
    args << "-y"
         << "-r" << m_fps
         << "-i" << QDir(m_enhancedDir).filePath("frame%08d." + m_options.outputFormat)
         << "-i" << m_options.inputPath
         << "-map" << "0:v:0"
         << "-map" << "1:a:0?"
         << "-c:a" << "copy";

    // 检测 libx264
    QProcess encoderCheck;
    encoderCheck.start(m_ffmpegPath, QStringList() << "-encoders");
    encoderCheck.waitForFinished();
    QString encoderOutput = QString::fromUtf8(encoderCheck.readAllStandardOutput());
    bool hasLibx264 = encoderOutput.contains("libx264");

    if (hasLibx264) {
        args << "-c:v" << "libx264"
             << "-pix_fmt" << "yuv420p";
    } else {
        qWarning() << "libx264 not available, falling back to mpeg4";
        args << "-c:v" << "mpeg4"
             << "-q:v" << "2";
    }

    args << m_outputPath;
    qDebug() << "FFmpeg command:" << m_ffmpegPath << args;
    m_ffmpegProcess->start(m_ffmpegPath, args);
}


void VideoProcessor::cleanupTempFiles()
{
    if (!m_tempDir.isEmpty() && QDir(m_tempDir).exists()) {
        QDir(m_tempDir).removeRecursively();
    }
}

QString VideoProcessor::getVideoMetadata()
{
    if (m_ffprobeProcess) {
        m_ffprobeProcess->deleteLater();
    }

    m_ffprobeProcess = new QProcess(this);
    m_ffprobeProcess->start(m_ffprobePath, QStringList()
                                               << "-v" << "error"
                                               << "-select_streams" << "v:0"
                                               << "-show_entries" << "stream=r_frame_rate"
                                               << "-of" << "default=noprint_wrappers=1:nokey=1"
                                               << m_options.inputPath);

    if (!m_ffprobeProcess->waitForFinished(5000)) {
        return "30"; // 默认帧率
    }

    QString output = QString::fromUtf8(m_ffprobeProcess->readAllStandardOutput());
    return parseFrameRate(output);
}

QString VideoProcessor::parseFrameRate(const QString &output)
{
    QString trimmed = output.trimmed();
    QStringList parts = trimmed.split('/');

    if (parts.size() != 2) {
        return "30";
    }

    bool ok1, ok2;
    double numerator = parts[0].toDouble(&ok1);
    double denominator = parts[1].toDouble(&ok2);

    if (ok1 && ok2 && denominator != 0) {
        return QString::number(numerator / denominator, 'f', 2);
    }

    return "30";
}

QString VideoProcessor::createTempDirectory()
{
    QFileInfo inputInfo(m_options.inputPath);
    QString baseDir = inputInfo.absolutePath();
    QString safeBase = baseDir.replace(" ", "_")
                           .replace("(", "")
                           .replace(")", "")
                           .replace("&", "");

    QString tempDirName = QString("tmp_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QString fullPath = QDir(safeBase).filePath(tempDirName);

    if (!QDir().mkpath(fullPath)) {
        emit errorOccurred(QString("无法创建临时目录: %1").arg(fullPath));
        return QString();
    }

    return fullPath;
}

QString VideoProcessor::generateOutputPath()
{
    QFileInfo inputInfo(m_options.inputPath);
    return QDir(inputInfo.absolutePath())
        .filePath(inputInfo.completeBaseName() + "_enhanced.mp4");
}

void VideoProcessor::updateProgress(int processed, int total)
{
    double percent = processed * 100.0 / total;
    emit progressPercentageChanged(percent);
    emit progressUpdated(QString("已处理: %1/%2").arg(processed).arg(total));
}

void VideoProcessor::handleRealesrganOutput()
{
    QString output = QString::fromUtf8(m_realesrganProcess->readAllStandardOutput());
    qDebug() << "RealESRGAN output:" << output;

    // 解析进度
    QRegularExpression progressRegex(R"((\d+\.\d+)%)");
    QRegularExpressionMatchIterator i = progressRegex.globalMatch(output);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        double progress = match.captured(1).toDouble();
        emit progressPercentageChanged(progress);
    }
}

void VideoProcessor::handleRealesrganFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)

    if (m_progressTimer) {
        m_progressTimer->stop();
        m_progressTimer->deleteLater();
        m_progressTimer = nullptr;
    }
    m_processingCompleted = true;

    if (m_cancelled) {
        return;
    }

    if (exitCode != 0) {
        QString error = QString::fromUtf8(m_realesrganProcess->readAllStandardError());
        emit errorOccurred(QString("RealESRGAN处理失败 (代码 %1): %2").arg(exitCode).arg(error));
        return;
    }

    // 检查处理后的帧数
    int enhancedCount = QDir(m_enhancedDir).entryList(QStringList() << "*." + m_options.outputFormat, QDir::Files).count();
    if (enhancedCount != m_totalFrames) {
        emit errorOccurred(QString("帧数不匹配，预期 %1，实际 %2").arg(m_totalFrames).arg(enhancedCount));
        return;
    }

    if (exitCode == 0) {
        rebuildVideo();
    } else {
        emit errorOccurred("帧增强失败");
    }
}

void VideoProcessor::handleFfmpegOutput()
{
    QString output = QString::fromUtf8(m_ffmpegProcess->readAllStandardOutput());
    qDebug() << "FFmpeg output:" << output;
}

void VideoProcessor::handleFfmpegFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)

    if (m_cancelled) {
        return;
    }

    if (exitCode != 0) {
        QString error = QString::fromUtf8(m_ffmpegProcess->readAllStandardError());
        emit errorOccurred(QString("FFmpeg处理失败 (代码 %1): %2").arg(exitCode).arg(error));
        return;
    }

    // 根据当前操作判断下一步
    if (m_ffmpegProcess->arguments().contains("-qscale:v")) {
        // 这是提取帧的操作
        enhanceFrames();
    } else {
        // 这是合并视频的操作
        emit progressUpdated(QString("视频处理完成，输出路径: %1").arg(m_outputPath));
        emit progressPercentageChanged(100);

        if (m_options.openOutputDirectory) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_outputPath).absolutePath()));
        }

        emit processingFinished(m_outputPath);
        cleanupTempFiles();
        QTimer::singleShot(0, this, []() {
            QWidget *parent = QApplication::activeWindow();
            QMessageBox::information(parent, "完成", "视频处理完成");
        });

    }
}


