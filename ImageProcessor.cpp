#include "ImageProcessor.h"
#include <QFileInfo>
#include <QDebug>
#include <QDir>

ImageProcessor::ImageProcessor(QObject *parent, bool noWindow)
    : QObject(parent), m_noWindow(noWindow), m_currentProcess(nullptr)
{
#ifdef Q_OS_WIN
    m_realESRGANExecutable = "realesrgan-ncnn-vulkan.exe";
    m_ffmpegExecutable = "ffmpeg.exe";
#else
    m_realESRGANExecutable = "realesrgan-ncnn-vulkan"; // 或完整路径
    m_ffmpegExecutable = "ffmpeg";
#endif
}

void ImageProcessor::setNoWindow(bool noWindow)
{
    m_noWindow = noWindow;
}

void ImageProcessor::processImages(const QStringList &inputPaths,
                                   const QString &modelName,
                                   const QString &outputFormat,
                                   bool openOutputDirectory)
{
    if (inputPaths.isEmpty()) {
        emit errorOccurred("No input files provided");
        return;
    }

    m_outputFiles.clear();
    m_currentModelName = modelName;
    m_currentOutputFormat = outputFormat;
    m_openOutputDirectory = openOutputDirectory;
    m_remainingInputPaths = inputPaths;

    processNextImage();
}

void ImageProcessor::processNextImage()
{
    if (m_remainingInputPaths.isEmpty()) {
        emit processingFinished(m_outputFiles);
        if (m_openOutputDirectory && !m_outputFiles.isEmpty()) {
            QFileInfo fileInfo(m_outputFiles.first());
            openOutputDirectory(fileInfo.absolutePath());
        }
        return;
    }

    QString inputPath = m_remainingInputPaths.takeFirst();
    QFileInfo inputFileInfo(inputPath);
    QString outputDir = inputFileInfo.absolutePath();
    QString fileNameWithoutExt = inputFileInfo.completeBaseName();

    QString tempOutput = QDir(outputDir).filePath(fileNameWithoutExt + "_temp.png");
    QString finalOutput = QDir(outputDir).filePath(fileNameWithoutExt + "-ENLARGE." + m_currentOutputFormat.toLower());

    // Validate input file
    if (!QFile::exists(inputPath)) {
        emit errorOccurred(QString("Input file not found: %1").arg(inputPath));
        return;
    }

    // Create output directory if needed
    QDir outputDirInfo = QFileInfo(tempOutput).absoluteDir();
    if (!outputDirInfo.exists() && !outputDirInfo.mkpath(".")) {
        emit errorOccurred(QString("Failed to create directory: %1").arg(outputDirInfo.path()));
        return;
    }

    // Start RealESRGAN process
    if (m_currentProcess) {
        m_currentProcess->deleteLater();
    }

    m_currentProcess = new QProcess(this);
    m_currentProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_currentProcess, &QProcess::readyReadStandardOutput,
            this, &ImageProcessor::handleProcessOutput);
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ImageProcessor::handleRealESRGANFinished);

    QStringList args;
    args << "-i" << inputPath
         << "-o" << tempOutput
         << "-n" << m_currentModelName;

    qDebug() << "Executing RealESRGAN:" << m_realESRGANExecutable << args;
    m_currentProcess->start(m_realESRGANExecutable, args);
}


void ImageProcessor::handleRealESRGANFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus)

    QString output = QString::fromUtf8(m_currentProcess->readAllStandardOutput());
    qDebug() << "RealESRGAN output:" << output;

    static QRegularExpression progressRegex(R"((\d+\.\d+)%)");
    QRegularExpressionMatchIterator i = progressRegex.globalMatch(output);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        double progress = match.captured(1).toDouble();
        emit progressUpdate(static_cast<int>(progress), "正在处理图像...");
    }

    if (exitCode != 0) {
        emit errorOccurred(QString("RealESRGAN failed (code %1): %2").arg(exitCode).arg(output));
        return;
    }

    // 解析临时输出路径
    QString tempOutput;
    QStringList args = m_currentProcess->arguments();
    for (int i = 0; i < args.size(); ++i) {
        if (args[i] == "-o" && i + 1 < args.size()) {
            tempOutput = args[i + 1];
            break;
        }
    }

    if (tempOutput.isEmpty()) {
        emit errorOccurred("Failed to parse output path");
        return;
    }

    QFileInfo tempFileInfo(tempOutput);
    QString finalOutput = QDir(tempFileInfo.absolutePath()).filePath(
        tempFileInfo.completeBaseName().replace("_temp", "-ENLARGE") +
        "." + m_currentOutputFormat.toLower());

    if (m_currentOutputFormat.toLower() != "png") {
        QProcess *ffmpegProcess = new QProcess(this);
        QString format = m_currentOutputFormat.toLower(); // 捕获输出格式

        connect(ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, tempOutput, finalOutput, ffmpegProcess, format](int code, QProcess::ExitStatus status) {
                    Q_UNUSED(status)
                    if (code != 0)
                    {
                        QString error = QString::fromUtf8(ffmpegProcess->readAllStandardError());
                        // 如果是JPG/JPEG格式，尝试备用方案
                        if (format == "jpg" || format == "jpeg")
                        {
                            QProcess *fallbackProcess = new QProcess(this);
                            connect(fallbackProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                                    this, [this, tempOutput, finalOutput, fallbackProcess](int fallbackCode, QProcess::ExitStatus fallbackStatus) {
                                        Q_UNUSED(fallbackStatus)
                                        if (fallbackCode != 0)
                                        {
                                            QString fallbackError = QString::fromUtf8(fallbackProcess->readAllStandardError());
                                            emit errorOccurred(QString("Fallback FFmpeg failed (code %1): %2").arg(fallbackCode).arg(fallbackError));
                                        }
                                        else
                                        {
                                            if (QFile::exists(tempOutput))
                                            {
                                                QFile::remove(tempOutput);
                                            }
                                            m_outputFiles.append(finalOutput);
                                            processNextImage();
                                        }
                                        fallbackProcess->deleteLater();
                                    });

                            QStringList fallbackArgs;
                            fallbackArgs << "-y"
                                         << "-i" << tempOutput
                                         << "-vf" << "scale=iw/1.3:ih/1.3"
                                         << "-q:v" << "2"
                                         << finalOutput;

                            qDebug() << "Executing fallback FFmpeg command:" << m_ffmpegExecutable << fallbackArgs;
                            fallbackProcess->start(m_ffmpegExecutable, fallbackArgs);
                        }
                        else
                        {
                            emit errorOccurred(QString("FFmpeg failed (code %1): %2").arg(code).arg(error));
                        }
                    } else
                    {
                        if (QFile::exists(tempOutput))
                        {
                            QFile::remove(tempOutput);
                        }
                        m_outputFiles.append(finalOutput);
                        emit fileProcessed();
                        processNextImage();
                    }
                    ffmpegProcess->deleteLater();
                });

        try {
            QStringList ffmpegArgs;
            if (format == "jpg" || format == "jpeg")
            {
                ffmpegArgs << "-y"
                           << "-i" << tempOutput
                           << "-q:v" << "2"
                           << finalOutput;
            }
            else if (format == "webp")
            {
                ffmpegArgs << "-y"
                           << "-i" << tempOutput
                           << "-quality" << "90"
                           << "-compression_level" << "6"
                           << finalOutput;
            } else {
                throw std::runtime_error("Unsupported format: " + format.toStdString());
            }

            qDebug() << "Executing FFmpeg:" << m_ffmpegExecutable << ffmpegArgs;
            ffmpegProcess->start(m_ffmpegExecutable, ffmpegArgs);
        } catch (const std::exception &e) {
            emit errorOccurred(QString("FFmpeg error: %1").arg(e.what()));
            ffmpegProcess->deleteLater();
        }
    } else {
        if (QFile::rename(tempOutput, finalOutput)) {
            m_outputFiles.append(finalOutput);
            processNextImage();
        } else {
            emit errorOccurred("Failed to rename output file");
        }
    }
}

void ImageProcessor::handleProcessOutput()
{
    QString output = QString::fromUtf8(m_currentProcess->readAllStandardOutput());
    static QRegularExpression progressRegex(R"((\d+\.\d+)%)");
    QRegularExpressionMatchIterator i = progressRegex.globalMatch(output);

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        double progress = match.captured(1).toDouble();
        qDebug() << "Processing progress:" << progress << "%";
        emit progressUpdate(static_cast<int>(progress), "正在处理图像...");
    }

    // 输出调试信息到控制台
    qDebug() << "Process output:" << output;
}



void ImageProcessor::openOutputDirectory(const QString &path)
{
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {QDir::toNativeSeparators(path)});
#elif defined(Q_OS_LINUX)
    QProcess::startDetached("xdg-open", {QDir::toNativeSeparators(path)});
#elif defined(Q_OS_MACOS)
    QProcess::startDetached("open", {QDir::toNativeSeparators(path)});
#endif
}
