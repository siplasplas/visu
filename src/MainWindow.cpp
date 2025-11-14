#include "MainWindow.h"
#include "ImageWidget.h"

#include <QStatusBar>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QMenuBar>
#include <QAction>
#include <QKeyEvent>

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

    scanDirectory(dirPath, normalizedStartFile);

    if (!imageFiles_.isEmpty()) {
        if (currentIndex_ < 0 || currentIndex_ >= imageFiles_.size())
            currentIndex_ = 0;
        loadImageAt(currentIndex_);
    } else {
        setWindowTitle("visu - (brak obrazów)");
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

    // Możesz dodać menu / akcje później
    auto fileMenu = menuBar()->addMenu(tr("&File"));
    auto quitAct = new QAction(tr("Quit"), this);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAct);
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
        // plik nieczytelny – można dodać komunikat, na razie pomijamy
        return;
    }

    currentIndex_ = index;
    imageWidget_->setImage(img);

    QPoint globalPos = QCursor::pos();
    QPoint widgetPos = imageWidget_->mapFromGlobal(globalPos);
    imageWidget_->updatePixelInfoAt(widgetPos);

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

void MainWindow::goToIndex(int index)
{
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
