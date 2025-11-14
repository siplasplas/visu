#include "MainWindow.h"
#include "ImageWidget.h"
#include "DoubleThresholdDialog.h"

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
#include <QDialogButtonBox>
#include <QMessageBox>

#include <opencv2/opencv.hpp>

MainWindow::MainWindow(const QString& startFilePath, QWidget* parent)
    : QMainWindow(parent)
{
    initUi();

    QString dirPath;
    QString normalizedStartFile;

    if (!startFilePath.isEmpty()) {
        QFileInfo fi(startFilePath);
        if (fi.exists() && fi.isFile()) {
            dirPath = fi.absolutePath();
            normalizedStartFile = fi.absoluteFilePath();
        }
    }

    if (dirPath.isEmpty()) {
        dirPath = QDir::currentPath();
    }

    baseDirectory_ = dirPath;              // NEW: zapamiętujemy katalog bazowy
    scanDirectory(dirPath, normalizedStartFile);

    if (!imageFiles_.isEmpty()) {
        if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
            currentIndex_ = 0;
        loadImageAt(currentIndex_);
    } else {
        setWindowTitle("visu - (no images)");
    }

    resize(1024, 768);
}

void MainWindow::initUi()
{
    imageWidget_ = new ImageWidget(this);
    setCentralWidget(imageWidget_);

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

    auto moveAct = new QAction(tr("Move..."), this);
    moveAct->setShortcut(Qt::Key_M);
    connect(moveAct, &QAction::triggered,
            this, &MainWindow::moveCurrentImage);
    fileMenu->addAction(moveAct);

    auto quitAct = new QAction(tr("Quit"), this);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAct);

    auto imageMenu = menuBar()->addMenu(tr("&Image"));

    auto doubleThAct = new QAction(tr("Double Threshold..."), this);
    doubleThAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(doubleThAct, &QAction::triggered,
            this, &MainWindow::openDoubleThresholdDialog);
    imageMenu->addAction(doubleThAct);
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

    // jeśli nie znaleziono pliku startowego, currentIndex_ zostanie ustawiony później
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

    // przelicz piksel pod kursorem
    {
        QPoint globalPos = QCursor::pos();
        QPoint widgetPos = imageWidget_->mapFromGlobal(globalPos);
        imageWidget_->updatePixelInfoAt(widgetPos);
    }

    QFileInfo fi(path);
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

    // Preview
    connect(dlg, &DoubleThresholdDialog::previewRequested,
            this, [this](int low, int high) {
                cv::Mat preview = applyDoubleThreshold(originalMat_, low, high);
                currentMat_ = preview.clone();
                imageWidget_->setImage(currentMat_);
            });

    // OK – zastosuj progowanie (jeszcze raz, na oryginale, na wypadek braku Preview)
    connect(dlg, &QDialog::accepted,
            this, [this, dlg]() {
                int low  = dlg->low();
                int high = dlg->high();
                cv::Mat result = applyDoubleThreshold(originalMat_, low, high);
                currentMat_ = result.clone();
                imageWidget_->setImage(currentMat_);
                dlg->close();
            });

    // Cancel – wróć do oryginału
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

cv::Mat MainWindow::applyDoubleThreshold(const cv::Mat& src, int low, int high)
{
    int L = std::clamp(low, 0, 255);
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
            // pomiędzy L i H pozostaje bez zmian
        }
    }

    return dst;
}
