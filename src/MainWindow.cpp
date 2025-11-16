#include "MainWindow.h"
#include "BrowserWidget.h"
#include "ImageWidget.h"
#include "DoubleThresholdDialog.h"
#include "ShadowCompressionDialog.h"
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

#include <opencv2/opencv.hpp>

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
        "png","jpg","jpeg","bmp","gif","webp","tif","tiff"
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
    if (!imageFiles_.isEmpty()) {
        loadImageAt(currentIndex_);
        switchToSingleMode();
    } else {
        switchToBrowserMode();
    }
}

void MainWindow::initUi()
{
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
    indexLabel_ = new QLabel(this);

    statusBar()->addPermanentWidget(indexLabel_);
    statusBar()->addPermanentWidget(coordLabel_);
    statusBar()->addPermanentWidget(rgbLabel_);

    coordLabel_->setText("X: -, Y: -");
    rgbLabel_->setText("R: -, G: -, B: -");
    indexLabel_->setText("0/0");

    connect(imageWidget_, &ImageWidget::pixelInfoChanged,
            this, &MainWindow::onPixelInfoChanged);

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

    // Move...
    auto moveAct = new QAction(tr("Move..."), this);
    moveAct->setShortcut(Qt::Key_M);
    connect(moveAct, &QAction::triggered,
            this, &MainWindow::moveCurrentImage);
    fileMenu->addAction(moveAct);

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
    auto doubleThAct = new QAction(tr("Double Threshold..."), this);
    doubleThAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(doubleThAct, &QAction::triggered,
            this, &MainWindow::openDoubleThresholdDialog);
    imageMenu->addAction(doubleThAct);

    auto softThAct = new QAction(tr("Shadow Compression..."), this);
    softThAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(softThAct, &QAction::triggered,
            this, &MainWindow::openShadowCompressionDialog);
    imageMenu->addAction(softThAct);
}

bool MainWindow::isImageFile(const QString& filePath) const
{
    static const QStringList exts = {
        "jpg", "jpeg", "png", "bmp", "gif", "tif", "tiff", "webp"
    };

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    return exts.contains(ext);
}

void MainWindow::scanDirectory(const QString& directory, const QString& startFilePath)
{
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

void MainWindow::loadImageAt(int index)
{
    if (index < 0 || index >= imageFiles_.size())
        return;

    const QString& path = imageFiles_[index];
    cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
    if (img.empty()) {
        return;
    }

    currentIndex_ = index;

    originalMat_ = img.clone();
    currentMat_  = img.clone();
    imageWidget_->setImage(currentMat_);

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
    } else {
        rgbLabel_->setText(
            QString("R: %1, G: %2, B: %3")
                .arg(r)
                .arg(g)
                .arg(b)
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

    switch (event->key()) {
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

    if (!handled) {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::moveCurrentImage()
{
    if (imageFiles_.isEmpty() || currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
        return;

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
        imageWidget_->setImage(cv::Mat());
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

    // PREVIEW – używa kolorów zależnie od checkboxów
    connect(dlg, &DoubleThresholdDialog::previewRequested,
            this, [this](int low, int high, bool lowColor, bool highColor) {
                cv::Mat preview = applyDoubleThresholdPreview(originalMat_, low, high,
                                                              lowColor, highColor);
                currentMat_ = preview.clone();
                imageWidget_->setImage(currentMat_);
            });

    connect(dlg, &QDialog::accepted,
        this, [this, dlg]() {

            // 1) Wczytujemy spinboxy
            int low  = dlg->low();
            int high = dlg->high();

            if (!dlg->lowApplyChecked())  low  = 0;
            if (!dlg->highApplyChecked()) high = 255;

            // 3) Kolorów NIE bierzemy pod uwagę → BW
            cv::Mat result = applyDoubleThresholdBW(originalMat_, low, high);

            originalMat_ = result.clone();
            currentMat_  = originalMat_.clone();
            imageWidget_->setImage(currentMat_);

            imageDirty_ = true;

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
                    imageWidget_->setImage(currentMat_);
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

    cv::Mat gray;
    if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        gray = src.clone();

    cv::Mat dst = gray.clone();

    for (int y = 0; y < dst.rows; ++y) {
        uchar* row = dst.ptr<uchar>(y);
        for (int x = 0; x < dst.cols; ++x) {
            uchar v = row[x];
            if (v <= L)
                row[x] = 0;
            else if (v >= H)
                row[x] = 255;
            // środek: bez zmian
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
    cv::Mat gray;
    if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        gray = src.clone();

    cv::Mat dst(gray.size(), CV_8UC3);

    for (int y = 0; y < gray.rows; ++y) {
        const uchar* srcRow = gray.ptr<uchar>(y);
        cv::Vec3b* dstRow   = dst.ptr<cv::Vec3b>(y);

        for (int x = 0; x < gray.cols; ++x) {
            uchar v = srcRow[x];

            // domyślnie – szarość
            cv::Vec3b color(v, v, v); // B=G=R=v

            if (v <= L) {
                if (lowColorize) {
                    // żółty: BGR = (0,255,255)
                    color = cv::Vec3b(0, 255, 255);
                } else {
                    // klasyczny czarny
                    color = cv::Vec3b(0, 0, 0);
                }
            } else if (v >= H) {
                if (highColorize) {
                    // czerwony: BGR = (0,0,255)
                    color = cv::Vec3b(0, 0, 255);
                } else {
                    // klasyczna biel
                    color = cv::Vec3b(255, 255, 255);
                }
            }

            dstRow[x] = color;
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
        imageWidget_->setImage(cv::Mat());
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
        tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp);;All files (*.*)")
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
        imageWidget_->setImage(cv::Mat());
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

    // konwersja do gray
    cv::Mat gray;
    if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        gray = src.clone();

    CV_Assert(gray.type() == CV_8UC1);

    // LUT
    cv::Mat lut(1, 256, CV_8UC1);
    uchar* p = lut.ptr<uchar>();

    for (int v = 0; v < 256; ++v) {
        uchar out = 0;

        if (v == 255) {
            // tło zostaje białe
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

    cv::Mat out;
    cv::LUT(gray, lut, out);
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
                imageWidget_->setImage(currentMat_);
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
            imageWidget_->setImage(currentMat_);

            imageDirty_ = true;

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
                    imageWidget_->setImage(currentMat_);
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
}

void MainWindow::switchToSingleMode()
{
    stacked_->setCurrentWidget(imageWidget_);
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
    tsCheck->setChecked(false); // domyślnie WYŁĄCZONY
    msg.setCheckBox(tsCheck);

    msg.exec();

    QAbstractButton* clicked = msg.clickedButton();
    if (clicked == cancelBtn) {
        // przerwać akcję (PageUp/PageDown, wyjście, przejście do Browser itp.)
        return false;
    } else if (clicked == saveBtn) {
        bool ok = saveCurrentImage(tsCheck->isChecked());
        return ok; // jeśli zapis się nie uda, zostawiamy obraz i nie wychodzimy
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

    cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
    if (img.empty()) {
        QMessageBox::warning(this, tr("Reload failed"),
                             tr("Could not reload picture from \"%1\".").arg(path));
        return;
    }

    originalMat_ = img.clone();
    currentMat_  = img.clone();
    imageWidget_->setImage(currentMat_);

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