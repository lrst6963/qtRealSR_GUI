# include "mainwindow.h"
# include "ui_mainwindow.h"
# include "VideoProcessor.h"
# include <QFileDialog>
# include <QMessageBox>
# include <QDir>
# include <QTimer>
# include <QDesktopServices>
# include <QUrl>
# include <QStandardPaths>
# include <QProcessEnvironment>
# include <QDebug>
# include <QTextStream>
# include <QApplication>
# include <QWindow>
# include <QMimeData>
# include <QDragEnterEvent>
# include <QDropEvent>
# include <QStyleHints>


MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
	, m_imageProcessor(new ImageProcessor(this)) // 初始化 ImageProcessor
	, m_videoProcessor(new VideoProcessor(this))
{
	ui->setupUi(this);

	setAcceptDrops(true);

	// 初始化UI组件
	initializeModules();
	validateDependencies();

}

MainWindow::~MainWindow()
{
	delete ui;
	delete m_imageProcessor;
	delete m_videoProcessor;
}

// 初始化模块和下拉框
void MainWindow::initializeModules()
{
	// 图片处理模块
	QStringList imageModules = {
		"realesrgan-x4plus-anime",
		"realesrgan-x4plus",
		"realesr-animevideov3-x2",
		"realesr-animevideov3-x3",
		"realesr-animevideov3-x4"
	};
	ui->comboBox_module->addItems(imageModules);
	ui->comboBox_module->setCurrentIndex(0);

	QStringList videoModules = {
		"realesr-animevideov3-x2",
		"realesr-animevideov3-x3",
		"realesr-animevideov3-x4",
		"realesrgan-x4plus-anime",
		"realesrgan-x4plus"
	};
	ui->video_comboBox_module->addItems(videoModules);
	ui->video_comboBox_module->setCurrentIndex(0);

	// 图像类型
	QStringList imageTypes = { "JPG", "PNG", "WEBP" };
	ui->comboBox_imgType->addItems(imageTypes);
	ui->comboBox_imgType->setCurrentIndex(0);
	//StatusBar
	ui->progressBar->setValue(0);
	ui->video_progressBar->setValue(0);
	ui->progressBar->setTextVisible(true);
	ui->progressBar->setFormat("%p%");
	ui->status_label->setText("就绪");
	ui->video_status->setText("就绪");
}

// 验证依赖项是否存在
void MainWindow::validateDependencies()
{
	// 定义要检查的依赖项
	struct Dependency
	{
		QString name;
		QString displayName;
		QString downloadUrl;
		QString installHint;
		bool required;
	};

	// 配置各平台的依赖项信息
	QVector<Dependency> dependencies = {
			{
				"realesrgan-ncnn-vulkan",
				"RealESRGAN",
				"https://github.com/xinntao/Real-ESRGAN/releases",
				"",
				true
			},
			{
				"ffmpeg",
				"FFmpeg",
				"https://ffmpeg.org/download.html",
				"",
				true
			},
			{
				"ffprobe",
				"FFprobe",
				"https://ffmpeg.org/download.html",
				"",
				true
			}
	};

	// 设置各平台的安装提示
# ifdef Q_OS_WIN
	dependencies[0].installHint = "下载预编译版本并解压到程序目录或系统PATH";
	dependencies[1].installHint = "从官网下载Windows版本并添加到PATH";
	dependencies[2].installHint = "通常与FFmpeg一起安装";
#elif defined(Q_OS_LINUX)
	dependencies[0].installHint = "sudo apt install realesrgan-ncnn-vulkan 或从源码编译";
	dependencies[1].installHint = "sudo apt install ffmpeg";
	dependencies[2].installHint = "sudo apt install ffmpeg";
#elif defined(Q_OS_MACOS)
	dependencies[0].installHint = "brew install realesrgan-ncnn-vulkan";
	dependencies[1].installHint = "brew install ffmpeg";
	dependencies[2].installHint = "brew install ffmpeg";
#endif

	// 检查每个依赖项
	QMap<QString, QString> foundPaths;
	QStringList missingDeps;
	QStringList warningDeps;

	for (const auto& dep : dependencies) {
		QString path = findExecutable(dep.name);
		foundPaths.insert(dep.name, path);

		if (path.isEmpty())
		{
			if (dep.required)
			{
				missingDeps << dep.displayName;
			}
			else
			{
				warningDeps << dep.displayName;
			}
		}
		// 调试输出
		qDebug() << "[Dependency]" << dep.displayName << ":"
			<< (path.isEmpty() ? "Not found" : path);
	}

	// 更新UI状态
	bool allRequiredFound = missingDeps.isEmpty();
	ui->btn_start->setEnabled(allRequiredFound);
	ui->comboBox_imgType->setEnabled(allRequiredFound);
	ui->btn_start_video->setEnabled(allRequiredFound);

	// 显示错误信息
	if (!missingDeps.isEmpty())
	{
		QString message = tr("缺少必要的依赖项:\n\n");
		for (const auto& dep : dependencies) {
			if (foundPaths[dep.name].isEmpty() && dep.required)
			{
				message += QString("• %1\n  安装方法: %2\n  下载地址: %3\n\n")
					.arg(dep.displayName)
					.arg(dep.installHint)
					.arg(dep.downloadUrl);
			}
		}

		QMessageBox::critical(this, tr("依赖项缺失"), message);
	}

	// 显示警告信息（可选依赖项）
	if (!warningDeps.isEmpty())
	{
		QString message = tr("缺少可选依赖项:\n\n");
		for (const auto& dep : dependencies) {
			if (foundPaths[dep.name].isEmpty() && !dep.required)
			{
				message += QString("• %1\n  安装方法: %2\n\n")
					.arg(dep.displayName)
					.arg(dep.installHint);
			}
		}

		QMessageBox::information(this, tr("可选依赖项缺失"), message);
	}

	// 保存找到的路径供后续使用
	m_realesrganPath = foundPaths["realesrgan-ncnn-vulkan"];
	m_ffmpegPath = foundPaths["ffmpeg"];
	m_ffprobePath = foundPaths["ffprobe"];
}


QString MainWindow::findExecutable(const QString& name)
{
	QString path = QStandardPaths::findExecutable(name);
	if (!path.isEmpty()) return path;

	QString localPath = QDir::currentPath() + QDir::separator() + name;
# ifdef Q_OS_WIN
	if (QFile::exists(localPath + ".exe"))
	{
		return localPath + ".exe";
	}
#else
	if (QFile::exists(localPath) && QFileInfo(localPath).isExecutable())
	{
		return localPath;
	}
#endif

# ifdef Q_OS_UNIX
	QStringList unixPaths = {
			"/usr/local/bin/" + name,
			"/usr/bin/" + name,
			QDir::homePath() + "/.local/bin/" + name,
			"/opt/homebrew/bin/" + name  // macOS Homebrew新路径
	};

	for (const auto& p : unixPaths) {
		if (QFile::exists(p) && QFileInfo(p).isExecutable()) {
			return p;
		}
	}
#endif
	return QString(); // 未找到
}


// 浏览图片文件
void MainWindow::on_btn_browse_clicked()
{
	QFileDialog dialog(this);
	dialog.setWindowTitle("选择图片文件");
	dialog.setNameFilter("图片文件 (*.jpg *.jpeg *.png *.bmp);;所有文件 (*.*)");
	dialog.setFileMode(ui->checkBox_multiSelect->isChecked() ?
		QFileDialog::ExistingFiles : QFileDialog::ExistingFile);

	if (dialog.exec())
	{
		m_selectedImageFiles = dialog.selectedFiles();
		ui->progressBar->setValue(0);
		updateFileDisplay();
	}
}



void MainWindow::on_video_btn_browse_clicked()
{

	QString videoFile = QFileDialog::getOpenFileName(this, "选择视频文件", "",
		"视频文件 (*.mp4 *.avi *.mov *.mkv *.flv *.webm);;所有文件 (*.*)");
	if (!videoFile.isEmpty())
	{
		ui->video_lineEdit_input->setText(videoFile);
		ui->video_progressBar->setValue(0);
	}
}

// 更新文件显示
void MainWindow::updateFileDisplay()
{
	int count = m_selectedImageFiles.size();
	if (count > 1)
	{
		ui->lineEdit_input->setText(QString("(%1个文件)").arg(count));
	}
	else if (count == 1)
	{
		ui->lineEdit_input->setText(m_selectedImageFiles.first());
	}
	else
	{
		ui->lineEdit_input->clear();
	}
}

// 开始处理图片
void MainWindow::on_btn_start_clicked()
{
	if (m_selectedImageFiles.isEmpty())
	{
		QMessageBox::warning(this, "提示", "请先选择要处理的图片文件");
		return;
	}
	if (ui->progressBar->value() == 100)
	{
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, "继续", "当前文件已完成，是否继续？", QMessageBox::Yes | QMessageBox::No);

		if (reply == QMessageBox::No)
		{
			return;
		}
	}
	// 初始化进度
	m_totalFiles = m_selectedImageFiles.size();
	m_filesProcessed = 0;
	m_currentFileProgress = 0;

	ui->progressBar->setValue(0);
	ui->progressBar->setMaximum(100);
	qApp->processEvents();

	// 禁用控件
	toggleImageControls(false);

	// 获取参数
	QString modelName = ui->comboBox_module->currentText();
	QString outputFormat = ui->comboBox_imgType->currentText();
	bool openOutputDirectory = ui->checkBox_openDir->isChecked();

	// 进度更新连接
	connect(m_imageProcessor, &ImageProcessor::progressUpdate, this,
		[this](int percent, const QString) {
			m_currentFileProgress = percent;

			// 计算整体进度
			int overall = (m_filesProcessed * 100 + m_currentFileProgress) / m_totalFiles;

			ui->progressBar->setValue(overall);
			ui->status_label->setText(
				QString("正在处理第%1/%2个文件")
				.arg(m_filesProcessed + 1)
				.arg(m_totalFiles)
			);
			qApp->processEvents();
		}, Qt::QueuedConnection);

	// 文件完成处理连接
	connect(m_imageProcessor, &ImageProcessor::fileProcessed, this,
		[this]() {
			m_filesProcessed++;
			m_currentFileProgress = 0;
		}, Qt::QueuedConnection);

	// 全部完成连接
	connect(m_imageProcessor, &ImageProcessor::processingFinished, this,
		[this](const QStringList& outputFiles) {
			ui->progressBar->setValue(100);
			if (!outputFiles.isEmpty())
			{
				ui->lineEdit_output->setText(outputFiles.last());
				ui->btn_openDir->setEnabled(true);
			}
			ui->status_label->setText(QString("已完成 %1 个文件").arg(outputFiles.size()));
			QMessageBox::information(this, "完成", QString("已完成 %1 个文件").arg(outputFiles.size()));
			toggleImageControls(true);
			disconnect(m_imageProcessor, nullptr, this, nullptr);
		}, Qt::QueuedConnection);

	// 错误处理
	connect(m_imageProcessor, &ImageProcessor::errorOccurred, this,
		[this](const QString& error) {
			QMessageBox::critical(this, "错误", error);
			toggleImageControls(true);
			disconnect(m_imageProcessor, nullptr, this, nullptr);
		}, Qt::QueuedConnection);

	// 开始处理
	m_imageProcessor->processImages(m_selectedImageFiles, modelName, outputFormat, openOutputDirectory);
}


void MainWindow::on_btn_start_video_clicked()
{
	if (ui->video_progressBar->value() == 100)
	{
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, "继续", "当前文件已完成，是否继续？", QMessageBox::Yes | QMessageBox::No);

		if (reply == QMessageBox::No)
		{
			return;
		}
	}
	QString videoPath = ui->video_lineEdit_input->text();
	if (videoPath.isEmpty())
	{
		QMessageBox::warning(this, "提示", "请先选择要处理的视频文件");
		return;
	}
	if (!QFile::exists(videoPath))
	{
		QMessageBox::warning(this, "错误", "视频文件不存在");
		return;
	}
	// 断开旧连接
	disconnect(m_videoProcessor, &VideoProcessor::processingFinished, this, nullptr);
	disconnect(m_videoProcessor, &VideoProcessor::progressUpdated, this, nullptr);
	disconnect(m_videoProcessor, &VideoProcessor::errorOccurred, this, nullptr);

	connect(m_videoProcessor, &VideoProcessor::progressUpdated,
		this, [this](const QString& msg) {
			ui->video_status->setText(msg);
		});
	connect(m_videoProcessor, &VideoProcessor::progressPercentageChanged,
		this, [this](double percent) {
			ui->video_progressBar->setValue(static_cast<int>(percent));
		});
	connect(m_videoProcessor, &VideoProcessor::errorOccurred,
		this, [this](const QString& error) {
			QMessageBox::critical(this, "错误", error);
			toggleVideoControls(true);
		});
	connect(m_videoProcessor, &VideoProcessor::processingFinished,
		this, [this](const QString& outputPath) {
			ui->video_lineEdit_input_2->setText(outputPath);
			ui->video_btn_openDir->setEnabled(true);
			toggleVideoControls(true);
		});

	QString modelName = ui->video_comboBox_module->currentText();
	bool openOutputDirectory = ui->video_checkBox_open->isChecked();
	// 重置UI状态
	ui->video_progressBar->setValue(0);
	ui->video_status->setText("正在处理...");
	ui->video_btn_openDir->setEnabled(false);
	toggleVideoControls(false);
	// 开始处理视频
	m_videoProcessor->processVideo(videoPath, modelName, 2, "png", openOutputDirectory);
}


// 打开输出目录
void MainWindow::on_btn_openDir_clicked()
{
	if (!ui->lineEdit_output->text().isEmpty() &&
		!ui->lineEdit_output->text().startsWith("("))
	{
		QFileInfo fileInfo(ui->lineEdit_output->text());
		QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
	}
}

void MainWindow::on_video_btn_openDir_clicked()
{
	QString outputPath = ui->video_lineEdit_input_2->text();
	if (!outputPath.isEmpty())
	{
		QFileInfo fileInfo(outputPath);
		QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
	}
}

// 多选文件复选框状态改变
void MainWindow::on_checkBox_multiSelect_stateChanged(int state)
{
	Q_UNUSED(state)
		// 可以在这里更新UI或执行其他操作
}

// 图像类型选择改变
void MainWindow::on_comboBox_imgType_currentIndexChanged(const QString& text)
{
	m_currentImageType = text.toLower();
}

// 显示Toast消息
void MainWindow::showToast(const QString& message, int durationMs)
{
	statusBar()->showMessage(message, durationMs);
}

// 切换控件可用状态
void MainWindow::toggleImageControls(bool enabled)
{
	ui->comboBox_module->setEnabled(enabled);
	ui->comboBox_imgType->setEnabled(enabled);
	ui->btn_browse->setEnabled(enabled);
	ui->checkBox_multiSelect->setEnabled(enabled);
	ui->checkBox_openDir->setEnabled(enabled);
	ui->btn_start->setEnabled(enabled && !m_selectedImageFiles.isEmpty());
}

void MainWindow::toggleVideoControls(bool enabled)
{
	ui->video_comboBox_module->setEnabled(enabled);
	ui->video_btn_browse->setEnabled(enabled);
	ui->video_checkBox_open->setEnabled(enabled);
	ui->btn_start_video->setEnabled(enabled);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasUrls()) {
		// 检查拖入的文件是否是我们支持的格式
		bool hasValidFile = false;
		foreach(const QUrl & url, event->mimeData()->urls()) {
			QString fileName = url.toLocalFile();
			if (isSupportedImageFile(fileName) || isSupportedVideoFile(fileName))
			{
				hasValidFile = true;
				break;
			}
		}

		if (hasValidFile)
		{
			event->acceptProposedAction();
			return;
		}
	}
	event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
	foreach(const QUrl & url, event->mimeData()->urls()) {
		QString filePath = url.toLocalFile();

		if (isSupportedImageFile(filePath))
		{
			handleDroppedImage(filePath);
		}
		else if (isSupportedVideoFile(filePath))
		{
			handleDroppedVideo(filePath);
		}
	}
}

bool MainWindow::isSupportedImageFile(const QString& filePath)
{
	QStringList supportedFormats = { "jpg", "jpeg", "png", "bmp", "webp" };
	QFileInfo fileInfo(filePath);
	QString suffix = fileInfo.suffix().toLower();

	return supportedFormats.contains(suffix);
}

bool MainWindow::isSupportedVideoFile(const QString& filePath)
{
	QStringList supportedFormats = { "mp4", "avi", "mov", "mkv", "flv", "webm" };
	QFileInfo fileInfo(filePath);
	QString suffix = fileInfo.suffix().toLower();

	return supportedFormats.contains(suffix);
}

void MainWindow::handleDroppedImage(const QString& filePath)
{
	if (ui->checkBox_multiSelect->isChecked()) {
		m_selectedImageFiles.append(filePath);
		ui->progressBar->setValue(0);
	}
	else
	{
		m_selectedImageFiles = QStringList{ filePath };
        ui->progressBar->setValue(0);
	}
	updateFileDisplay();
	statusBar()->showMessage(tr("已添加图片文件: %1").arg(QFileInfo(filePath).fileName()), 3000);
}

void MainWindow::handleDroppedVideo(const QString& filePath)
{
	ui->video_lineEdit_input->setText(filePath);
	ui->video_progressBar->setValue(0);
	statusBar()->showMessage(tr("已添加视频文件: %1").arg(QFileInfo(filePath).fileName()), 3000);
}

