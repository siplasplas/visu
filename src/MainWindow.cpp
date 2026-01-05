#include "MainWindow.h"
#include "BrowserWidget.h"
#include "ImageWidget.h"
#include "DoubleThresholdDialog.h"
#include "ShadowCompressionDialog.h"
#include "image_metrics.h"

#include <array>
#include <algorithm>

#include <QStatusBar>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QMenuBar>
#include <QAction>
#include <QKeyEvent>
#include <QCursor>
#include <QDialog>
#include <QVBoxLayout>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QCheckBox>
#include <QPushButton>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

#include <opencv2/opencv.hpp>
#include <QtConcurrent/qtconcurrentrun.h>

#include "avif_opencv.h"
#include "jpeg_lossless.h"
#include "config.h"

MainWindow::MainWindow(const QString& startPath, QWidget* parent)
    : QMainWindow(parent)
{
    initUi();
    resize(1024, 768);

    if (startPath.isEmpty()) {
        switchToBrowserMode();
        return;
    }

    QFileInfo fi(startPath);
    if (!fi.exists()) {
        QMessageBox::warning(this, tr("Invalid path"),
                             tr("Path \"%1\" does not exist.").arg(startPath));
        switchToBrowserMode();
        return;
    }

    if (fi.isDir()) {
        switchToBrowserMode();
        browserWidget_->setRootDirectory(fi.absoluteFilePath());
        return;
    }

    QString suffix = fi.suffix().toLower();
    const QStringList imageExt = {
        "png","jpg","jpeg","jp2", "bmp","gif","webp","tif","tiff", "avif"
    };

    if (!imageExt.contains(suffix)) {
        QMessageBox::warning(this, tr("Unsupported file"),
                             tr("File \"%1\" is not a supported image.").arg(startPath));
        switchToBrowserMode();
        return;
    }

    // ok – wczytać obraz i wejść w tryb single-image
    QString dir  = fi.absolutePath();
    QString file = fi.absoluteFilePath();

    scanDirectory(dir, file);   // set imageFiles_, currentIndex_
    browserWidget_->ensureDirectoryLoaded(dir);
    if (!imageFiles_.isEmpty()) {
        loadImageAt(currentIndex_);
        switchToSingleMode();
    } else {
        switchToBrowserMode();
    }
    previewWatcher_ = new QFutureWatcher<bool>(this);
    connect(previewWatcher_, &QFutureWatcher<bool>::finished,
            this, &MainWindow::onPreviewJobFinished);
}

void MainWindow::initUi()
{
    setAcceptDrops(true);

    stacked_ = new QStackedWidget(this);

    browserWidget_ = new BrowserWidget(this);
    imageWidget_   = new ImageWidget(this);
    stacked_->addWidget(browserWidget_);
    stacked_->addWidget(imageWidget_);
    setCentralWidget(stacked_);

    connect(browserWidget_, &BrowserWidget::thumbnailActivated,
            this, &MainWindow::onThumbnailActivated);

    coordLabel_ = new QLabel(this);
    rgbLabel_   = new QLabel(this);
    rgbHexLabel_   = new QLabel(this);
    indexLabel_ = new QLabel(this);

    statusBar()->addPermanentWidget(indexLabel_);
    statusBar()->addPermanentWidget(coordLabel_);
    statusBar()->addPermanentWidget(rgbLabel_);
    statusBar()->addPermanentWidget(rgbHexLabel_);

    coordLabel_->setText("X: -, Y: -");
    rgbLabel_->setText("R: -, G: -, B: -");
    rgbHexLabel_->setText("#######");

    indexLabel_->setText("0/0");

    connect(imageWidget_, &ImageWidget::pixelInfoChanged,
            this, &MainWindow::onPixelInfoChanged);

    contextMenu_ = new QMenu(this);
    contextMenu_->addAction("Copy RGB (#RRGGBB)", this, &MainWindow::copyRgbHex);
    contextMenu_->addAction("Copy RGB (R, G, B)", this, &MainWindow::copyRgbDecimal);
    contextMenu_->addSeparator();
    contextMenu_->addAction("Copy position", this, &MainWindow::copyPosition);

    connect(imageWidget_, &ImageWidget::contextMenuRequested,
            this, [this](const QPoint& globalPos) {
                contextMenu_->exec(globalPos);
            });

    auto fileMenu = menuBar()->addMenu(tr("&File"));

    // NEW: Open File...
    auto openFileAct = new QAction(tr("Open File..."), this);
    openFileAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    connect(openFileAct, &QAction::triggered,
            this, &MainWindow::openFile);
    fileMenu->addAction(openFileAct);

    // NEW: Open Directory...
    auto openDirAct = new QAction(tr("Open Directory..."), this);
    openDirAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(openDirAct, &QAction::triggered,
            this, &MainWindow::openDirectory);
    fileMenu->addAction(openDirAct);

    auto saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcut(QKeySequence::SaveAs);   // zwykle Ctrl+Shift+S
    connect(saveAsAct, &QAction::triggered,
            this, &MainWindow::openSaveAsDialog);
    fileMenu->addAction(saveAsAct);

    // Move...
    moveAct_ = new QAction(tr("Move..."), this);
    moveAct_->setShortcut(Qt::Key_M);
    connect(moveAct_, &QAction::triggered,
            this, &MainWindow::moveCurrentImage);
    fileMenu->addAction(moveAct_);

    auto quitAct = new QAction(tr("Quit"), this);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAct);

    auto viewMenu = menuBar()->addMenu(tr("&View"));
    auto browserAct = new QAction(tr("Browser mode"), this);
    auto singleAct  = new QAction(tr("Single image mode"), this);
    connect(browserAct, &QAction::triggered, this, &MainWindow::switchToBrowserMode);
    connect(singleAct,  &QAction::triggered, this, &MainWindow::switchToSingleMode);
    viewMenu->addAction(browserAct);
    viewMenu->addAction(singleAct);

    auto imageMenu = menuBar()->addMenu(tr("&Image"));
    doubleThAct_ = new QAction(tr("Double Threshold..."), this);
    doubleThAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(doubleThAct_, &QAction::triggered,
            this, &MainWindow::openDoubleThresholdDialog);
    imageMenu->addAction(doubleThAct_);

    shadowCompAct_ = new QAction(tr("Shadow Compression..."), this);
    shadowCompAct_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(shadowCompAct_, &QAction::triggered,
            this, &MainWindow::openShadowCompressionDialog);
    imageMenu->addAction(shadowCompAct_);

    auto helpMenu = menuBar()->addMenu(tr("&Help"));
    auto aboutAct = new QAction(tr("About"), this);
    connect(aboutAct, &QAction::triggered, this, [this]() {
        QString aboutText = QString(
            "<h2>Visu</h2>"
            "<p>Version: %1</p>"
            "<p>Git commit: %2</p>"
            "<hr>"
            "<p>Qt version: %3</p>"
            "<p>OpenCV version: %4</p>"
        ).arg(VISU_VERSION)
         .arg(VISU_GIT_SHA)
         .arg(qVersion())
         .arg(CV_VERSION);

        QMessageBox::about(this, tr("About Visu"), aboutText);
    });
    helpMenu->addAction(aboutAct);
}
template <typename> constexpr auto MainWindow::qt_create_metaobjectdata() {}

void MainWindow::updateActionsForMode()
{
    const bool single = (stacked_->currentIndex() == 1);
    doubleThAct_->setEnabled(single);
    shadowCompAct_->setEnabled(single);
    moveAct_->setEnabled(single);
}


bool MainWindow::isImageFile(const QString& filePath) const
{
    static const QStringList exts = {
        "jpg", "jpeg", "jp2","png", "bmp", "gif", "tif", "tiff", "webp", "avif"
    };

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    return exts.contains(ext);
}

void MainWindow::scanDirectory(const QString& directory, const QString& startFilePath)
{
    currentImageDir_ = directory;
    imageFiles_.clear();
    currentIndex_ = -1;

    baseDirectory_ = directory;

    QDir dir(directory);
    QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase
    );

    for (const QFileInfo& fi : entries) {
        if (!isImageFile(fi.absoluteFilePath()))
            continue;
        imageFiles_.push_back(fi.absoluteFilePath());
    }

    if (imageFiles_.isEmpty())
        return;

    if (!startFilePath.isEmpty()) {
        QString target = QFileInfo(startFilePath).absoluteFilePath();
        for (int i = 0; i < imageFiles_.size(); ++i) {
            if (QFileInfo(imageFiles_[i]).absoluteFilePath() == target) {
                currentIndex_ = i;
                break;
            }
        }
    }

    // if start file not found , currentIndex_ will set later
}

cv::Mat MainWindow::loadAnyImage(const QString& path)
{
    const std::string fn = path.toStdString();
    QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "avif") {
        return imreadAvif(fn, cv::IMREAD_COLOR);
    } else {
        return cv::imread(fn, cv::IMREAD_COLOR);
    }
}

void MainWindow::loadImageAt(int index)
{
    if (index < 0 || index >= imageFiles_.size())
        return;

    const QString& path = imageFiles_[index];
    cv::Mat img = loadAnyImage(path);
    if (img.empty()) {
        return;
    }
    currentIndex_ = index;
    bool preserveZoom = img.cols == originalMat_.cols && img.rows == originalMat_.rows;
    originalMat_ = img.clone();
    imageDirty_    = false;
    safeOnlyDirty_ = true;
    QString ext = QFileInfo(path).suffix().toLower();
    isCurrentFileJpeg_   = (ext == "jpg" || ext == "jpeg");
    pendingJpegRotSteps_ = 0;
    currentMat_  = img.clone();
    imageWidget_->setImage(currentMat_, !preserveZoom);

    cv::Mat gray;
    if (originalMat_.channels() == 3)
        cv::cvtColor(originalMat_, gray, cv::COLOR_BGR2GRAY);
    else
        gray = originalMat_;
    computeHistogram(gray);

    // przelicz piksel pod kursorem
    {
        QPoint globalPos = QCursor::pos();
        QPoint widgetPos = imageWidget_->mapFromGlobal(globalPos);
        imageWidget_->updatePixelInfoAt(widgetPos);
    }

    QFileInfo fi(path);
    originalFileTime_ = fi.lastModified();
    hasOriginalFileTime_ = originalFileTime_.isValid();
    imageDirty_ = false;
    setWindowTitle(QString("visu - %1").arg(fi.fileName()));
    updateIndexLabel();
}


void MainWindow::updateIndexLabel()
{
    if (imageFiles_.isEmpty() || currentIndex_ < 0) {
        indexLabel_->setText("0/0");
        return;
    }

    QFileInfo fi(imageFiles_[currentIndex_]);
    indexLabel_->setText(
        QString("%1/%2  %3")
            .arg(currentIndex_ + 1)
            .arg(imageFiles_.size())
            .arg(fi.fileName())
    );
}

void MainWindow::goToIndex(int index) {
    if (imageFiles_.isEmpty())
        return;

    if (!maybeSaveCurrentImage())
        return; // chosen Cancel

    if (index < 0)
        index = 0;
    if (index >= imageFiles_.size())
        index = imageFiles_.size() - 1;

    if (index == currentIndex_)
        return;

    loadImageAt(index);
}

void MainWindow::onPixelInfoChanged(int x, int y, int r, int g, int b)
{
    lastPixelX_ = x;
    lastPixelY_ = y;
    lastPixelR_ = r;
    lastPixelG_ = g;
    lastPixelB_ = b;

    if (x < 0 || y < 0) {
        coordLabel_->setText("X: -, Y: -");
        rgbLabel_->setText("R: -, G: -, B: -");
        return;
    }

    coordLabel_->setText(
        QString("X: %1, Y: %2")
            .arg(x)
            .arg(y)
    );

    if (r < 0 || g < 0 || b < 0) {
        rgbLabel_->setText("R: -, G: -, B: -");
        rgbHexLabel_->setText("#######");

    } else {
        rgbLabel_->setText(
            QString("R: %1, G: %2, B: %3")
                .arg(r)
                .arg(g)
                .arg(b)
        );
        rgbHexLabel_->setText(
             QString("#%1%2%3")
                .arg(r, 2, 16, QLatin1Char('0'))
                .arg(g, 2, 16, QLatin1Char('0'))
                .arg(b, 2, 16, QLatin1Char('0'))
                .toUpper()
        );
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (imageFiles_.isEmpty()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    bool handled = false;

    if (stacked_->currentWidget() == imageWidget_) {
        if (stacked_->currentWidget() == imageWidget_) {
            switch (event->key()) {
                case Qt::Key_Plus:
                case Qt::Key_Equal:   // często '+' to Shift+'='
                    imageWidget_->zoomIn();
                    return;
                case Qt::Key_Minus:
                    imageWidget_->zoomOut();
                    return;
                case Qt::Key_0:
                    imageWidget_->zoomResetTo100();
                    return;
                case Qt::Key_F:
                    imageWidget_->zoomFit();
                    return;
                default:
                    break;
            }
        }

        switch (event->key()) {
            case Qt::Key_L:
                rotateCurrentImageLeft();
                return;
            case Qt::Key_R:
                rotateCurrentImageRight();
                return;
            case Qt::Key_Delete:
                deleteCurrentImageToTrash();
                return;
            case Qt::Key_Escape:
                if (!currentImageDir_.isEmpty())
                    browserWidget_->ensureDirectoryLoaded(currentImageDir_);
                switchToBrowserMode();
                handled = true;
                break;
            case Qt::Key_Left:
            case Qt::Key_PageUp:
                goToIndex(currentIndex_ - 1);
                handled = true;
                break;

            case Qt::Key_Right:
            case Qt::Key_PageDown:
                goToIndex(currentIndex_ + 1);
                handled = true;
                break;

            case Qt::Key_Home:
                goToIndex(0);
                handled = true;
                break;

            case Qt::Key_End:
                goToIndex(imageFiles_.size() - 1);
                handled = true;
                break;

            default:
                break;
        }
    } else
        handled = true;

    if (!handled) {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::moveCurrentImage()
{
    if (imageFiles_.isEmpty() || currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
        return;
    if (!maybeSaveCurrentImage())
        return; // Cancel

    const QString currentFile = imageFiles_[currentIndex_];
    QFileInfo curInfo(currentFile);
    const QString currentDir = curInfo.absolutePath();

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Move image"));

    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    layout->addWidget(new QLabel(tr("Move current image to directory:"), &dlg));

    QComboBox* combo = new QComboBox(&dlg);
    combo->setEditable(true);

    for (const QString& dir : recentMoveDirs_)
        combo->addItem(dir);

    combo->setEditText("");   // NEW: brak domyślnej propozycji
    layout->addWidget(combo);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, &dlg
    );
    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString text = combo->currentText().trimmed();
    if (text.isEmpty())
        return;

    if (text == "~") text = QDir::homePath();
    else if (text.startsWith("~/")) text.replace(0, 1, QDir::homePath());

    QString targetDirPath;
    if (QDir::isAbsolutePath(text))
        targetDirPath = QDir::cleanPath(text);
    else
        targetDirPath = QDir::cleanPath(QDir(baseDirectory_).absoluteFilePath(text));

    QString normalizedCurrentDir = QDir::cleanPath(currentDir);
    QString normalizedTargetDir  = QDir::cleanPath(targetDirPath);

    QDir checkDir;
    if (!checkDir.exists(normalizedTargetDir)) {
        if (!checkDir.mkpath(normalizedTargetDir)) {
            QMessageBox::warning(this, tr("Move image"),
                                 tr("Failed to create directory:\n%1")
                                     .arg(normalizedTargetDir));
            return;
        }
    }

    if (normalizedCurrentDir == normalizedTargetDir) {
        QMessageBox::warning(this, tr("Move image"),
                             tr("Target directory is the current directory.\nNothing to do."));
        return;
    }

    QString targetFilePath = QDir(normalizedTargetDir).filePath(curInfo.fileName());

    if (QFile::exists(targetFilePath)) {
        QMessageBox::warning(this, tr("Move image"),
                             tr("File already exists:\n%1").arg(targetFilePath));
        return;
    }

    if (!QFile::rename(currentFile, targetFilePath)) {
        QMessageBox::warning(this, tr("Move image"),
                             tr("Failed to move file:\n%1\n→\n%2")
                                 .arg(currentFile, targetFilePath));
        return;
    }

    recentMoveDirs_.removeAll(normalizedTargetDir);
    recentMoveDirs_.prepend(normalizedTargetDir);
    if (recentMoveDirs_.size() > 10)
        recentMoveDirs_.removeLast();

    imageFiles_.removeAt(currentIndex_);

    if (imageFiles_.isEmpty()) {
        currentIndex_ = -1;
        originalMat_.release();
        currentMat_.release();
        imageWidget_->setImage(cv::Mat(), true);
        setWindowTitle("visu - (no images)");
        updateIndexLabel();
        onPixelInfoChanged(-1, -1, -1, -1, -1);
        return;
    }

    if (currentIndex_ >= imageFiles_.size())
        currentIndex_ = imageFiles_.size() - 1;

    loadImageAt(currentIndex_);
}

void MainWindow::openDoubleThresholdDialog()
{
    if (originalMat_.empty())
        return;

    auto* dlg = new DoubleThresholdDialog(this);

    // PREVIEW – uses colors depending on checkboxes
    connect(dlg, &DoubleThresholdDialog::previewRequested,
            this, [this](int low, int high, bool lowColor, bool highColor) {
                cv::Mat preview = applyDoubleThresholdPreview(originalMat_, low, high,
                                                              lowColor, highColor);
                currentMat_ = preview.clone();
                imageWidget_->setImage(currentMat_, false);
            });

    connect(dlg, &QDialog::accepted,
        this, [this, dlg]() {

            // 1) Loading spinboxes
            int low  = dlg->low();
            int high = dlg->high();

            if (!dlg->lowApplyChecked())  low  = 0;
            if (!dlg->highApplyChecked()) high = 255;

            // 3) We do NOT take colors into account → BW
            cv::Mat result = applyDoubleThresholdBW(originalMat_, low, high);

            originalMat_ = result.clone();
            currentMat_  = originalMat_.clone();
            imageWidget_->setImage(currentMat_, false);

            imageDirty_ = true;
            safeOnlyDirty_ = false;

            cv::Mat gray;
            if (originalMat_.channels() == 3)
                cv::cvtColor(originalMat_, gray, cv::COLOR_BGR2GRAY);
            else
                gray = originalMat_;
            computeHistogram(gray);

            dlg->close();
        });

    // Cancel – powrót do oryginału
    connect(dlg, &QDialog::rejected,
            this, [this, dlg]() {
                if (!originalMat_.empty()) {
                    currentMat_ = originalMat_.clone();
                    imageWidget_->setImage(currentMat_, false);
                }
                dlg->close();
            });

    dlg->show();
}

cv::Mat MainWindow::applyDoubleThresholdBW(const cv::Mat& src, int low, int high)
{
    int L = std::clamp(low,  0, 255);
    int H = std::clamp(high, 0, 255);
    if (L > H)
        std::swap(L, H);

    // For grayscale images - work directly
    if (src.channels() == 1) {
        cv::Mat dst = src.clone();
        for (int y = 0; y < dst.rows; ++y) {
            uchar* row = dst.ptr<uchar>(y);
            for (int x = 0; x < dst.cols; ++x) {
                uchar v = row[x];
                if (v <= L)
                    row[x] = 0;
                else if (v >= H)
                    row[x] = 255;
            }
        }
        return dst;
    }

    // For color images - use gray for threshold check, modify color output
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::Mat dst = src.clone();

    for (int y = 0; y < dst.rows; ++y) {
        const uchar* grayRow = gray.ptr<uchar>(y);
        cv::Vec3b* dstRow = dst.ptr<cv::Vec3b>(y);
        for (int x = 0; x < dst.cols; ++x) {
            uchar v = grayRow[x];
            if (v <= L)
                dstRow[x] = cv::Vec3b(0, 0, 0);       // black
            else if (v >= H)
                dstRow[x] = cv::Vec3b(255, 255, 255); // white
            // middle: keep original color
        }
    }

    return dst;
}

cv::Mat MainWindow::applyDoubleThresholdPreview(const cv::Mat& src,
int low, int high,
bool lowColorize,
bool highColorize) {
    int L = std::clamp(low, 0, 255);
    int H = std::clamp(high, 0, 255);
    if (L > H)
        std::swap(L, H);

    // For grayscale images
    if (src.channels() == 1) {
        cv::Mat dst(src.size(), CV_8UC3);
        for (int y = 0; y < src.rows; ++y) {
            const uchar* srcRow = src.ptr<uchar>(y);
            cv::Vec3b* dstRow = dst.ptr<cv::Vec3b>(y);
            for (int x = 0; x < src.cols; ++x) {
                uchar v = srcRow[x];
                cv::Vec3b color(v, v, v);

                if (v <= L) {
                    color = lowColorize ? cv::Vec3b(0, 255, 255) : cv::Vec3b(0, 0, 0);
                } else if (v >= H) {
                    color = highColorize ? cv::Vec3b(0, 0, 255) : cv::Vec3b(255, 255, 255);
                }
                dstRow[x] = color;
            }
        }
        return dst;
    }

    // For color images - use gray for threshold check, preserve original colors
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::Mat dst = src.clone();

    for (int y = 0; y < dst.rows; ++y) {
        const uchar* grayRow = gray.ptr<uchar>(y);
        cv::Vec3b* dstRow = dst.ptr<cv::Vec3b>(y);

        for (int x = 0; x < dst.cols; ++x) {
            uchar v = grayRow[x];

            if (v <= L) {
                dstRow[x] = lowColorize ? cv::Vec3b(0, 255, 255) : cv::Vec3b(0, 0, 0);
            } else if (v >= H) {
                dstRow[x] = highColorize ? cv::Vec3b(0, 0, 255) : cv::Vec3b(255, 255, 255);
            }
            // middle: keep original color (already in dst from clone)
        }
    }

    return dst;
}

void MainWindow::openDirectory()
{
    if (!maybeSaveCurrentImage())
        return;
    QFileDialog dlg(this, tr("Select directory"));
    dlg.setFileMode(QFileDialog::Directory);
    dlg.setOption(QFileDialog::ShowDirsOnly, true);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);

    if (!baseDirectory_.isEmpty())
        dlg.setDirectory(baseDirectory_);
    else
        dlg.setDirectory(QDir::currentPath());

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString dir = dlg.selectedFiles().value(0);
    if (dir.isEmpty())
        return;

    scanDirectory(dir, QString());

    if (imageFiles_.isEmpty()) {
        // czyścimy widok
        currentIndex_ = -1;
        originalMat_.release();
        currentMat_.release();
        imageWidget_->setImage(cv::Mat(), true);
        setWindowTitle("visu - (no images)");
        updateIndexLabel();
        onPixelInfoChanged(-1, -1, -1, -1, -1);
        return;
    }

    currentIndex_ = 0;
    loadImageAt(currentIndex_);
}

void MainWindow::openFile()
{
    if (!maybeSaveCurrentImage())
        return;
    QFileDialog dlg(this, tr("Select image"));
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    dlg.setNameFilter(
        tr("Images (*.png *.jpg *.jpeg *.jp2 *.bmp *.gif *.tif *.tiff *.webp *.avif);;All files (*.*)")
    );

    if (!baseDirectory_.isEmpty())
        dlg.setDirectory(baseDirectory_);
    else
        dlg.setDirectory(QDir::currentPath());

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString path = dlg.selectedFiles().value(0);
    if (path.isEmpty())
        return;

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())
        return;

    const QString dir  = fi.absolutePath();
    const QString file = fi.absoluteFilePath();

    // przeskanuj katalog pliku i ustaw się na nim
    scanDirectory(dir, file);

    if (imageFiles_.isEmpty()) {
        currentIndex_ = -1;
        originalMat_.release();
        currentMat_.release();
        imageWidget_->setImage(cv::Mat(), true);
        setWindowTitle("visu - (no images)");
        updateIndexLabel();
        onPixelInfoChanged(-1, -1, -1, -1, -1);
        return;
    }

    if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
        currentIndex_ = 0;

    loadImageAt(currentIndex_);
}

void MainWindow::computeHistogram(const cv::Mat& gray)
{
    histogram_.fill(0);
    CV_Assert(gray.type() == CV_8UC1);

    for (int y = 0; y < gray.rows; ++y) {
        const uchar* row = gray.ptr<uchar>(y);
        for (int x = 0; x < gray.cols; ++x) {
            ++histogram_[row[x]];
        }
    }
}

int MainWindow::estimateTFromHistogram() const
{
    // znajdź ostatnią wartość <255, gdzie histogram_ > 0
    for (int i = 254; i >= 0; --i) {
        if (histogram_[static_cast<size_t>(i)] > 0)
            return i;
    }
    // fallback
    return 242;
}

cv::Mat MainWindow::applyShadowCompression(const cv::Mat& src,
                                       int T, double gamma, int maxOut)
{
    // parametry w rozsądnych granicach
    T      = std::clamp(T, 1, 254);
    maxOut = std::clamp(maxOut, 1, 254);
    if (gamma <= 0.0) gamma = 1.0;

    // LUT
    cv::Mat lut(1, 256, CV_8UC1);
    uchar* p = lut.ptr<uchar>();

    for (int v = 0; v < 256; ++v) {
        uchar out = 0;

        if (v == 255) {
            // background stays white
            out = 255;
        } else {
            int vClamped = std::min(v, T);
            double x = static_cast<double>(vClamped) / static_cast<double>(T); // [0,1]
            double y = std::pow(x, gamma);                                     // [0,1]
            double scaled = y * maxOut;                                        // [0,maxOut]

            if (scaled < 0.0)   scaled = 0.0;
            if (scaled > 255.0) scaled = 255.0;

            out = static_cast<uchar>(std::lround(scaled));
        }

        p[v] = out;
    }

    // For grayscale images - apply LUT directly
    if (src.channels() == 1) {
        cv::Mat out;
        cv::LUT(src, lut, out);
        return out;
    }

    // For color images - apply LUT to each channel separately
    std::vector<cv::Mat> channels;
    cv::split(src, channels);

    for (auto& ch : channels) {
        cv::Mat processed;
        cv::LUT(ch, lut, processed);
        ch = processed;
    }

    cv::Mat out;
    cv::merge(channels, out);
    return out;
}

void MainWindow::openShadowCompressionDialog()
{
    if (originalMat_.empty())
        return;

    auto* dlg = new ShadowCompressionDialog(this);

    // startowe T z histogramu
    int tFromHist = estimateTFromHistogram();
    dlg->setT(tFromHist);

    // Preview
    connect(dlg, &ShadowCompressionDialog::previewRequested,
            this, [this](int T, double gamma, int maxOut) {
                cv::Mat preview = applyShadowCompression(originalMat_, T, gamma, maxOut);
                currentMat_ = preview.clone();
                imageWidget_->setImage(currentMat_, false);
            });

    connect(dlg, &ShadowCompressionDialog::requestTFromHistogram,
            this, [this, dlg]() {
                int tFromHist = estimateTFromHistogram();
                dlg->setT(tFromHist);
                dlg->onPreviewClicked();
            });

    connect(dlg, &QDialog::accepted,
        this, [this, dlg]() {
            int T      = dlg->tValue();
            double g   = dlg->gammaValue();
            int maxOut = dlg->maxOutValue();

            cv::Mat result = applyShadowCompression(originalMat_, T, g, maxOut);

            originalMat_ = result.clone();
            currentMat_  = originalMat_.clone();
            imageWidget_->setImage(currentMat_,false);

            imageDirty_ = true;
            safeOnlyDirty_ = false;

            cv::Mat gray;
            if (originalMat_.channels() == 3)
                cv::cvtColor(originalMat_, gray, cv::COLOR_BGR2GRAY);
            else
                gray = originalMat_;
            computeHistogram(gray);

            dlg->close();
        });

    // Cancel – przywróć oryginał
    connect(dlg, &QDialog::rejected,
            this, [this, dlg]() {
                if (!originalMat_.empty()) {
                    currentMat_ = originalMat_.clone();
                    imageWidget_->setImage(currentMat_, false);
                }
                dlg->close();
            });

    dlg->show();
}


void MainWindow::switchToBrowserMode()
{
    if (!maybeSaveCurrentImage())
        return; // Cancel
    stacked_->setCurrentWidget(browserWidget_);
    updateActionsForMode();
}

void MainWindow::switchToSingleMode()
{
    stacked_->setCurrentWidget(imageWidget_);
    updateActionsForMode();
}

void MainWindow::onThumbnailActivated(const QString& filePath)
{
    QFileInfo fi(filePath);
    const QString dir  = fi.absolutePath();
    const QString file = fi.absoluteFilePath();

    scanDirectory(dir, file);

    if (!imageFiles_.isEmpty()) {
        if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
            currentIndex_ = 0;
        loadImageAt(currentIndex_);
        switchToSingleMode();
    }
}

bool MainWindow::maybeSaveCurrentImage()
{
    if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
        return true;
    if (!imageDirty_)
        return true;

    const QString path = imageFiles_[currentIndex_];
    QFileInfo fi(path);
    const QString name = fi.fileName();

    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Question);
    msg.setWindowTitle(tr("Save changes"));
    msg.setText(tr("Picture \"%1\" changed.").arg(name));
    msg.setInformativeText(tr("Do you want to save your changes?"));

    QPushButton* saveBtn    = msg.addButton(tr("Save"), QMessageBox::AcceptRole);
    QPushButton* discardBtn = msg.addButton(tr("Discard"), QMessageBox::DestructiveRole);
    QPushButton* cancelBtn  = msg.addButton(tr("Cancel"), QMessageBox::RejectRole);

    // checkbox: restore original timestamp
    QCheckBox* tsCheck = new QCheckBox(tr("Restore original timestamp"), &msg);
    tsCheck->setChecked(safeOnlyDirty_);
    msg.setCheckBox(tsCheck);

    msg.exec();
    bool restoreTs = tsCheck->isChecked();
    delete tsCheck;

    QAbstractButton* clicked = msg.clickedButton();
    if (clicked == cancelBtn) {
        // interrupt action (PageUp/PageDown, exit, go to Browser, etc.)
        return false;
    } else     if (clicked == saveBtn) {
        if (canDoLosslessJpegRotation()) {
            // ONLY transformation on disk, without cv::imwrite
            return applyPendingJpegRotationOnDisk(restoreTs);
        } else {
            // normal writing (e.g., cv::imwrite), potentially lossy operation
            return saveCurrentImage(restoreTs);
        }
    } else if (clicked == discardBtn) {
        revertCurrentImage();
        return true;
    }

    // na wszelki wypadek
    return true;
}

bool MainWindow::saveCurrentImage(bool restoreTimestamp)
{
    if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
        return true;

    const QString path = imageFiles_[currentIndex_];

    if (originalMat_.empty()) {
        return true;
    }

    bool ok = cv::imwrite(path.toStdString(), originalMat_);
    if (!ok) {
        QMessageBox::warning(this, tr("Save failed"),
                             tr("Could not save picture to \"%1\".").arg(path));
        return false;
    }

    if (restoreTimestamp && hasOriginalFileTime_) {
        QFile f(path);
        if (!f.setFileTime(originalFileTime_, QFileDevice::FileModificationTime)) {
            QMessageBox::warning(this, tr("Timestamp not restored"),
                                 tr("Could not restore original timestamp for \"%1\".").arg(path));
        }
    }

    imageDirty_ = false;
    safeOnlyDirty_ = true;
    pendingJpegRotSteps_ = 0;

    cv::Mat gray;
    if (originalMat_.channels() == 3)
        cv::cvtColor(originalMat_, gray, cv::COLOR_BGR2GRAY);
    else
        gray = originalMat_;
    computeHistogram(gray);

    return true;
}

void MainWindow::revertCurrentImage()
{
    if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
        return;

    const QString path = imageFiles_[currentIndex_];

    cv::Mat img = loadAnyImage(path);

    if (img.empty()) {
        QMessageBox::warning(this, tr("Reload failed"),
                             tr("Could not reload picture from \"%1\".").arg(path));
        return;
    }

    originalMat_ = img.clone();
    imageDirty_    = false;
    safeOnlyDirty_ = true;
    pendingJpegRotSteps_ = 0;
    currentMat_  = img.clone();
    imageWidget_->setImage(currentMat_, false);

    imageDirty_ = false;

    cv::Mat gray;
    if (originalMat_.channels() == 3)
        cv::cvtColor(originalMat_, gray, cv::COLOR_BGR2GRAY);
    else
        gray = originalMat_;
    computeHistogram(gray);

    QFileInfo fi(path);
    originalFileTime_ = fi.lastModified();
    hasOriginalFileTime_ = originalFileTime_.isValid();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!maybeSaveCurrentImage()) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::isInTrash(const QString& path) const
{
#ifdef Q_OS_UNIX
    QFileInfo fi(path);
    const QString filePath = fi.absoluteFilePath();

    // Typical XDG: ~/.local/share/Trash/files
    const QString trashRoot =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + "/Trash/files";

    QDir trashDir(trashRoot);
    const QString trashPrefix = trashDir.absolutePath();

    return filePath.startsWith(trashPrefix);
#else
    Q_UNUSED(path);
    return false;
#endif
}

void MainWindow::deleteCurrentImageToTrash()
{
    if (stacked_->currentWidget() != imageWidget_)
        return;

    if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
        return;

    const QString path = imageFiles_[currentIndex_];

    // 1) Are we in the trash? → lock
    if (isInTrash(path)) {
        QMessageBox::warning(this,
                             tr("Cannot delete"),
                             tr("You are viewing a file in the Trash.\n"
                                "Deleting files from Trash is not allowed."));
        return;
    }

    QFileInfo fi(path);
    const QString name = fi.fileName();

    QMessageBox msg(this);
    msg.setIcon(QMessageBox::Question);
    msg.setWindowTitle(tr("Move to trash"));
    msg.setText(tr("Move \"%1\" to trash?").arg(name));
    msg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.setDefaultButton(QMessageBox::Yes);

    const int ret = msg.exec();
    if (ret != QMessageBox::Yes)
        return;

    // 4) Move to trash
    QString error;
    if (!QFile::moveToTrash(path, &error)) {
        QMessageBox::warning(this,
                             tr("Move to trash failed"),
                             tr("Could not move \"%1\" to trash.\n%2")
                                 .arg(path, error));
        return;
    }

    // 5) Usuwamy z listy i przechodzimy do następnego obrazka
    const int removedIndex = currentIndex_;
    imageFiles_.removeAt(removedIndex);
    imageDirty_ = false;

    if (imageFiles_.isEmpty()) {
        // brak kolejnych obrazów
        currentIndex_ = -1;
        originalMat_.release();
        currentMat_.release();
        imageWidget_->clearImage();
        updateIndexLabel();   // np. "0/0" albo pusty
        switchToBrowserMode(); // możesz chcieć wrócić do Browser
        return;
    }

    int nextIndex = removedIndex;
    if (nextIndex >= imageFiles_.size())
        nextIndex = imageFiles_.size() - 1;

    currentIndex_ = nextIndex;
    loadImageAt(currentIndex_);
}

bool MainWindow::canDoLosslessJpegRotation() const
{
    if (!imageDirty_)
        return false;

    if (!isCurrentFileJpeg_)
        return false;

    if (!safeOnlyDirty_)   // była jakakolwiek niebezpieczna operacja
        return false;

    int steps = pendingJpegRotSteps_ % 4;
    if (steps == 0)
        return false;      // netto żadnej rotacji, nie ma co robić

    if (originalMat_.empty())
        return false;

    int w = originalMat_.cols;
    int h = originalMat_.rows;

    if ((w % 8) != 0 || (h % 8) != 0)
        return false;

    return true;
}

void MainWindow::rotateCurrentImageLeft()
{
    if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
        return;
    if (originalMat_.empty())
        return;

    cv::Mat rotated;
    cv::rotate(originalMat_, rotated, cv::ROTATE_90_COUNTERCLOCKWISE);

    originalMat_ = rotated.clone();
    currentMat_  = rotated.clone();
    imageWidget_->setImage(currentMat_, true);

    if (isCurrentFileJpeg_) {
        // +90°
        pendingJpegRotSteps_ = (pendingJpegRotSteps_ + 1) % 4;
        // if rotations cancel each other out (steps 0), there are no real changes
        if (safeOnlyDirty_ && pendingJpegRotSteps_ == 0)
            imageDirty_ = false;
        else {
            imageDirty_ = true;
            if (!canDoLosslessJpegRotation())
                safeOnlyDirty_ = false;
        }
    } else {
        // non-JPEG: only in memory – dirty always true
        imageDirty_ = true;
    }

    cv::Mat gray;
    if (originalMat_.channels() == 3)
        cv::cvtColor(originalMat_, gray, cv::COLOR_BGR2GRAY);
    else
        gray = originalMat_;
    computeHistogram(gray);
}

void MainWindow::rotateCurrentImageRight()
{
    if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
        return;
    if (originalMat_.empty())
        return;

    cv::Mat rotated;
    cv::rotate(originalMat_, rotated, cv::ROTATE_90_CLOCKWISE);

    originalMat_ = rotated.clone();
    currentMat_  = rotated.clone();
    imageWidget_->setImage(currentMat_, true);

    if (isCurrentFileJpeg_) {
        // +90°
        pendingJpegRotSteps_ = (pendingJpegRotSteps_ + 1) % 4;
        // if rotations cancel each other out (steps 0), there are no real changes
        if (safeOnlyDirty_ && pendingJpegRotSteps_ == 0)
            imageDirty_ = false;
        else {
            imageDirty_ = true;
            if (!canDoLosslessJpegRotation())
                safeOnlyDirty_ = false;
        }
    } else {
        // non-JPEG: only in memory – dirty always true
        imageDirty_ = true;
    }

    cv::Mat gray;
    if (originalMat_.channels() == 3)
        cv::cvtColor(originalMat_, gray, cv::COLOR_BGR2GRAY);
    else
        gray = originalMat_;
    computeHistogram(gray);
}

void MainWindow::restoreOriginalTimestamp(const QString& path)
{
    if (!originalFileTime_.isValid())
        return;

    QFile f(path);
    if (!f.exists())
        return;

    // Na Linuksie FileBirthTime często jest niewspierany, ale nie szkodzi spróbować
    f.setFileTime(originalFileTime_, QFileDevice::FileModificationTime);
    f.setFileTime(originalFileTime_, QFileDevice::FileAccessTime);

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    f.setFileTime(originalFileTime_, QFileDevice::FileBirthTime);
#endif
}

bool MainWindow::applyPendingJpegRotationOnDisk(bool restoreTimestamp)
{
    int steps = pendingJpegRotSteps_ % 4;
    if (steps == 0)
        return true;

    const QString path = imageFiles_[currentIndex_];

    // wykonaj bezstratną rotację na pliku
    if (!rotateJpegLossless(path, steps)) {
        QMessageBox::warning(this,
                             tr("Error"),
                             tr("Failed to apply lossless JPEG rotation on disk."));
        return false;
    }

    // ewentualne odtworzenie timestampu (masz już do tego kod)
    if (restoreTimestamp) {
        restoreOriginalTimestamp(path); // Twoja funkcja
    }

    pendingJpegRotSteps_ = 0;

    // przeładuj obraz z dysku, żeby currentMat_/originalMat_ były zgodne
    cv::Mat reloaded = loadAnyImage(path); // Twoje imread/imreadAvif
    if (reloaded.empty()) {
        QMessageBox::warning(this,
                             tr("Error"),
                             tr("Failed to reload rotated image."));
        return false;
    }

    originalMat_ = reloaded.clone();
    currentMat_  = reloaded.clone();
    imageWidget_->setImage(currentMat_, true);

    imageDirty_    = false;
    safeOnlyDirty_ = true;

    // histogram na nowo...
    return true;
}

void MainWindow::openSaveAsDialog()
{
    if (!imageWidget_ || originalMat_.empty())
        return;

    if (!saveAsDlg_) {
        saveAsDlg_ = new SaveAsDialog(this);
        connect(saveAsDlg_, &SaveAsDialog::previewRequested,
                this, &MainWindow::onSaveAsPreviewRequested);
        connect(saveAsDlg_, &SaveAsDialog::formatChanged,
        this,        [this]() {
            onSaveAsPreviewRequested(
                saveAsDlg_->selectedFormat(),
                saveAsDlg_->quality(),
                saveAsDlg_->showOriginalChecked()
            );
        });

        connect(saveAsDlg_, &SaveAsDialog::metricSelectionChanged,
                this,        &MainWindow::updateSaveAsMetricsOnly);
        connect(saveAsDlg_, &SaveAsDialog::acceptedSave,
                this, &MainWindow::onSaveAsAccepted);
        if (saveAsDlg_->showOriginalCheck_) {
            connect(saveAsDlg_->showOriginalCheck_, &QCheckBox::toggled,
                    this, &MainWindow::onShowOriginalClicked);
        connect(saveAsDlg_, &SaveAsDialog::dialogClosed,
            this, &MainWindow::closeSaveAsDialog);
        }
        connect(saveAsDlg_, &SaveAsDialog::metricSelectionChanged,
            this,        &MainWindow::updateSaveAsMetricsOnly);
        connect(saveAsDlg_, &SaveAsDialog::formatChanged,
        this, [this]() {
            emit saveAsDlg_->previewRequested(saveAsDlg_->selectedFormat(),
                                              saveAsDlg_->quality(),
                                              saveAsDlg_->showOriginalChecked());
        });
    }

    saveAsDlg_->show();
    saveAsDlg_->raise();
    saveAsDlg_->activateWindow();
}

void MainWindow::onShowOriginalClicked() {
    if (saveAsDlg_->showOriginalCheck_->isChecked()) {
        imageWidget_->setImage(originalMat_, false);
        saveAsDlg_->clearFileSizeInfo();
    } else {
        saveAsDlg_->onShowOriginalToggled(false);
    }
}

void MainWindow::onSaveAsPreviewRequested(ImageFormat fmt, int quality, bool showOriginal)
{
    if (!saveAsDlg_ || originalMat_.empty())
        return;

    SaveAsRequest req{ fmt, quality, showOriginal };

    // 1. Show original → zero kompresji
    if (showOriginal) {
        pendingRequest_.reset();
        if (!previewJobRunning_) {
            saveAsDlg_->clearFileSizeInfo();
            previewMat_.release();
            imageWidget_->setImage(originalMat_, false);
        }
        return;
    }

    // 2. Lossless format (PNG/BMP/TIFF) – there is no point in doing compression previews
    if (!isAlwaysLossyFormat(fmt) &&
        fmt != ImageFormat::Webp &&
        fmt != ImageFormat::Avif &&
        fmt != ImageFormat::Jp2)
    {
        pendingRequest_.reset();
        saveAsDlg_->clearFileSizeInfo();
        previewMat_.release();
        imageWidget_->setImage(originalMat_, false);
        return;
    }

    // 3. If the job is already running → just save it as pending, don't start a new one
    if (previewJobRunning_) {
        pendingRequest_ = req;
        return;
    }

    // 4. Job not working → start a new one
    startPreviewJob(req);
}

void MainWindow::startPreviewJob(const SaveAsRequest& req)
{
    previewJobRunning_ = true;
    pendingRequest_.reset();

    saveAsDlg_->clearFileSizeInfo();

    // dane potrzebne w wątku
    cv::Mat src = originalMat_.clone();
    ImageFormat fmt = req.format;
    int quality = req.quality;

    QString ext = (fmt == ImageFormat::Jpeg) ? "jpg" :
                  (fmt == ImageFormat::Webp) ? "webp" :
                  (fmt == ImageFormat::Jp2) ? "jp2" :
                  (fmt == ImageFormat::Avif) ? "avif" : "jpg";

    previewTmpPath_ = QString("/dev/shm/visu_preview_%1_%2.%3")
            .arg(QCoreApplication::applicationPid())
            .arg(QDateTime::currentMSecsSinceEpoch())
            .arg(ext);

    QString tmpPath = previewTmpPath_; // lokalna kopia do lambdy

    auto future = QtConcurrent::run([src, fmt, quality, tmpPath]() -> bool {
        std::string tmp = tmpPath.toStdString();
        bool ok = false;
        std::vector<int> params;

        if (fmt == ImageFormat::Jpeg) {
            params = { cv::IMWRITE_JPEG_QUALITY, quality };
            ok = cv::imwrite(tmp, src, params);
        } else if (fmt == ImageFormat::Jp2) {
            params = { cv::IMWRITE_JPEG2000_COMPRESSION_X1000, quality };
            ok = cv::imwrite(tmp, src, params);
        } else if (fmt == ImageFormat::Webp) {
            params = { cv::IMWRITE_WEBP_QUALITY, quality };
            ok = cv::imwrite(tmp, src, params);
        } else if (fmt == ImageFormat::Avif) {
            ok = imwriteAvif(tmp, src, quality);
        } else {
            ok = false;
        }

        return ok;
    });

    previewWatcher_->setFuture(future);
}

void MainWindow::onPreviewJobFinished()
{
    previewJobRunning_ = false;
    bool ok = previewWatcher_->result();

    if (!ok) {
        // encoding failed → return to original
        previewMat_.release();
        imageWidget_->setImage(originalMat_, false);
        if (saveAsDlg_) {
            saveAsDlg_->clearFileSizeInfo();
        }
    } else {
        // success – loading tmp and redrawing
        cv::Mat preview = loadAnyImage(previewTmpPath_);
        if (preview.empty()) {
            previewMat_.release();
            imageWidget_->setImage(originalMat_, false);
            if (saveAsDlg_) {
                saveAsDlg_->clearFileSizeInfo();
            }
        } else {
            previewMat_ = preview;
            imageWidget_->setImage(previewMat_, false);

            // calculate size and ratio
            QFileInfo fi(previewTmpPath_);
            qint64 bytes = fi.size();

            int w = originalMat_.cols;
            int h = originalMat_.rows;
            int c = originalMat_.channels();
            double rawSize = double(w) * h * c;
            double ratio = (rawSize > 0.0 && bytes > 0)
                           ? (rawSize / double(bytes))
                           : 0.0;

            if (saveAsDlg_) {
                saveAsDlg_->setFileSizeInfo(bytes, ratio);
                saveAsDlg_->clearMetricInfo();
                MetricType metric = saveAsDlg_->selectedMetric();

                if (metric != MetricType::None) {
                    double value = computeMetric(metric, originalMat_, previewMat_);
                    std::string s = formatMetricResult(metric, value);
                    saveAsDlg_->setMetricInfo(QString::fromStdString(s));
                }
            }
        }
    }

    // delete temporary file
    if (!previewTmpPath_.isEmpty()) {
        QFile::remove(previewTmpPath_);
        previewTmpPath_.clear();
    }

    // jeśli mamy oczekujące żądanie – startujemy nowe
    if (pendingRequest_) {
        SaveAsRequest next = *pendingRequest_;
        pendingRequest_.reset();
        startPreviewJob(next);
    }


}


void MainWindow::onSaveAsAccepted(ImageFormat fmt, int quality, bool showOriginal)
{
    Q_UNUSED(showOriginal);

    // 1. ask for the destination path (QFileDialog::getSaveFileName)
    QString extDefault;
    switch (fmt) {
        case ImageFormat::Png:  extDefault = "png"; break;
        case ImageFormat::Jpeg: extDefault = "jpg"; break;
        case ImageFormat::Gif:  extDefault = "gif"; break;
        case ImageFormat::Webp: extDefault = "webp"; break;
        case ImageFormat::Avif: extDefault = "avif"; break;
        case ImageFormat::Bmp:  extDefault = "bmp"; break;
        case ImageFormat::Tiff: extDefault = "tif"; break;
        default: extDefault = "png"; break;
    }

    QString filter = fileDialogFilterAllImages();
    QString targetPath = QFileDialog::getSaveFileName(this,
                            tr("Save image as"),
                            QString(),
                            filter);
    if (targetPath.isEmpty())
        return;

    // add extension if missing
    if (QFileInfo(targetPath).suffix().isEmpty())
        targetPath += "." + extDefault;

    bool ok = false;
    if (!isAlwaysLossyFormat(fmt) &&
        fmt != ImageFormat::Webp &&
        fmt != ImageFormat::Avif) {
        // lossless saving: PNG/BMP/TIFF (without compression preview)
        ok = saveLossless(targetPath, fmt, originalMat_);
        } else {
            // lossy recording: use the same parameters as in preview (quality)
                ok = saveLossy(targetPath, fmt, originalMat_, quality);
        }

    if (!ok) {
        QMessageBox::warning(this, tr("Save error"),
                             tr("Could not save image to %1").arg(targetPath));
        return;
    }
}

bool MainWindow::saveLossless(const QString& path, ImageFormat fmt, const cv::Mat& mat)
{
    if (mat.empty()) {
        return false;
    }

    std::string fname = path.toStdString();
    std::vector<int> params;
    bool ok = false;

    switch (fmt) {
    case ImageFormat::Png:
        // PNG is lossless – compression level only affects time/size
        // 0 = no compression, 9 = max. compression
        params = { cv::IMWRITE_PNG_COMPRESSION, 3 }; // e.g., 3 as a reasonable compromise
        ok = cv::imwrite(fname, mat, params);
        break;

    case ImageFormat::Bmp:
        // Uncompressed BMP (always lossless)
        ok = cv::imwrite(fname, mat);
        break;

    case ImageFormat::Tiff:
        // TIFF with LZW (lossless). A value of 5 means LZW in OpenCV.
        params = { cv::IMWRITE_TIFF_COMPRESSION, 5 };
        ok = cv::imwrite(fname, mat, params);
        break;

    case ImageFormat::Gif:
        // TODO: real GIF indexing (palette) by your module.
        // For now: no support, we signal this as false.
        ok = false;
        break;

    default:
        // We do not perform lossless saves for other formats.
        ok = false;
        break;
    }

    return ok;
}

bool MainWindow::saveLossy(const QString& path, ImageFormat fmt, const cv::Mat& mat, int quality)
{
    if (mat.empty()) {
        return false;
    }

    if (quality < 0)   quality = 0;
    if (quality > 100) quality = 100;

    std::string fname = path.toStdString();
    std::vector<int> params;
    bool ok = false;

    switch (fmt) {
    case ImageFormat::Jpeg:
        // JPEG – klasyczna kompresja stratna
            params = { cv::IMWRITE_JPEG_QUALITY, quality };
            cv::imwrite(path.toStdString(), currentMat_, params);
            break;

    case ImageFormat::Webp:
        // WebP: OpenCV obsługuje IMWRITE_WEBP_QUALITY (0–100)
        params = { cv::IMWRITE_WEBP_QUALITY, quality };
        ok = cv::imwrite(fname, mat, params);
        break;

    case ImageFormat::Avif:
            imwriteAvif(path.toStdString(), currentMat_, quality);
            break;

    // If you want to add “lossy PNG8/GIF with quantization” in the future, you can add it here:
    // case ImageFormat::Png:
    // case ImageFormat::Gif:
    //     ok = saveIndexedWithQuantization(...);
    //     break;
    default:
        // format not supported in lossy mode
        ok = false;
        break;
    }

    return ok;
}

void MainWindow::closeSaveAsDialog()
{
    pendingRequest_.reset();
    if (previewJobRunning_) {
        // opcjonalnie: można poczekać, ale zazwyczaj po prostu zostawić job,
        // a tylko przestać pokazywać dialog; tmp i tak usuwamy w onPreviewJobFinished.
    }
    if (!previewTmpPath_.isEmpty()) {
        QFile::remove(previewTmpPath_);
        previewTmpPath_.clear();
    }
    if (saveAsDlg_) {
        saveAsDlg_->close();
    }
}

void MainWindow::updateSaveAsMetricsOnly()
{
    if (!saveAsDlg_)
        return;
    if (originalMat_.empty())
        return;

    saveAsDlg_->clearMetricInfo();

    MetricType metric = saveAsDlg_->selectedMetric();
    if (metric == MetricType::None)
        return;

    // Wybór obrazów: referencją jest zawsze oryginał,
    // testem – aktualny preview, jeśli istnieje i NIE pokazujemy oryginału.
    const cv::Mat* ref  = &originalMat_;
    const cv::Mat* test = nullptr;

    bool showOriginal = saveAsDlg_->showOriginalChecked();
    if (!previewMat_.empty() && !showOriginal) {
        test = &previewMat_;     // skompresowana wersja
    } else {
        test = &originalMat_;    // porównanie oryginał do oryginału → ideał
    }

    if (test->empty())
        return;

    double value = computeMetric(metric, *ref, *test);
    std::string s = formatMetricResult(metric, value);
    saveAsDlg_->setMetricInfo(QString::fromStdString(s));
}

void MainWindow::copyRgbHex()
{
    if (lastPixelR_ < 0 || lastPixelG_ < 0 || lastPixelB_ < 0)
        return;

    QString hex = QString("#%1%2%3")
        .arg(lastPixelR_, 2, 16, QLatin1Char('0'))
        .arg(lastPixelG_, 2, 16, QLatin1Char('0'))
        .arg(lastPixelB_, 2, 16, QLatin1Char('0'))
        .toUpper();

    QApplication::clipboard()->setText(hex);
}

void MainWindow::copyRgbDecimal()
{
    if (lastPixelR_ < 0 || lastPixelG_ < 0 || lastPixelB_ < 0)
        return;

    QString text = QString("%1, %2, %3")
        .arg(lastPixelR_)
        .arg(lastPixelG_)
        .arg(lastPixelB_);

    QApplication::clipboard()->setText(text);
}

void MainWindow::copyPosition()
{
    if (lastPixelX_ < 0 || lastPixelY_ < 0)
        return;

    QString text = QString("%1, %2")
        .arg(lastPixelX_)
        .arg(lastPixelY_);

    QApplication::clipboard()->setText(text);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile() && isImageFile(url.toLocalFile())) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (!url.isLocalFile())
            continue;

        QString path = url.toLocalFile();
        if (!isImageFile(path))
            continue;

        if (!maybeSaveCurrentImage())
            return;

        QFileInfo fi(path);
        const QString dir = fi.absolutePath();
        const QString file = fi.absoluteFilePath();

        scanDirectory(dir, file);
        browserWidget_->ensureDirectoryLoaded(dir);

        if (!imageFiles_.isEmpty()) {
            if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
                currentIndex_ = 0;
            loadImageAt(currentIndex_);
            switchToSingleMode();
        }

        event->acceptProposedAction();
        return;
    }

    event->ignore();
}
