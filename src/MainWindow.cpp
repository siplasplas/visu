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

    // NEW: Move...
    auto moveAct = new QAction(tr("Move..."), this);
    moveAct->setShortcut(Qt::Key_M);       // zwykła litera 'm'
    connect(moveAct, &QAction::triggered,
            this, &MainWindow::moveCurrentImage);
    fileMenu->addAction(moveAct);

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
    imageWidget_->setImage(img);

    // od razu przeliczamy piksel pod kursorem
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

    // Dialog: edytowalny combo z historią
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Move image"));

    QVBoxLayout* layout = new QVBoxLayout(&dlg);

    QLabel* label = new QLabel(tr("Move current image to directory:"), &dlg);
    layout->addWidget(label);

    QComboBox* combo = new QComboBox(&dlg);
    combo->setEditable(true);

    // wypełniamy historią katalogów
    for (const QString& dir : recentMoveDirs_) {
        combo->addItem(dir);
    }

    // domyślny tekst – np. ".." jako przykład względnej ścieżki
    if (!recentMoveDirs_.isEmpty()) {
        combo->setEditText(recentMoveDirs_.front());
    } else {
        combo->setEditText("..");
    }

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

    // Rozszerzenie ~ na katalog domowy
    if (text == "~") {
        text = QDir::homePath();
    } else if (text.startsWith("~/")) {
        text.replace(0, 1, QDir::homePath());
    }

    QString targetDirPath;
    if (QDir::isAbsolutePath(text)) {
        targetDirPath = QDir::cleanPath(text);
    } else {
        // względnie do bieżącego katalogu (skanowanego)
        QDir baseDir(baseDirectory_.isEmpty() ? currentDir : baseDirectory_);
        targetDirPath = QDir::cleanPath(baseDir.absoluteFilePath(text));
    }

    // Sprawdzenie, czy to nie jest bieżący katalog
    QString normalizedCurrentDir = QDir::cleanPath(currentDir);
    QString normalizedTargetDir  = QDir::cleanPath(targetDirPath);

    if (normalizedTargetDir == normalizedCurrentDir) {
        QMessageBox::warning(this, tr("Move image"),
                             tr("Target directory is the current directory.\nNothing to do."));
        return;
    }

    QDir targetDir(normalizedTargetDir);
    if (!targetDir.exists()) {
        QMessageBox::warning(this, tr("Move image"),
                             tr("Target directory does not exist:\n%1").arg(normalizedTargetDir));
        return;
    }

    const QString fileName = curInfo.fileName();
    const QString targetFilePath = targetDir.filePath(fileName);

    if (QFile::exists(targetFilePath)) {
        QMessageBox::warning(this, tr("Move image"),
                             tr("File already exists in target directory:\n%1").arg(targetFilePath));
        return;
    }

    if (!QFile::rename(currentFile, targetFilePath)) {
        QMessageBox::warning(this, tr("Move image"),
                             tr("Failed to move file:\n%1\n→\n%2")
                                 .arg(currentFile, targetFilePath));
        return;
    }

    // Aktualizacja historii katalogów (unikalne, ostatnio użyty na początku)
    int idx = recentMoveDirs_.indexOf(normalizedTargetDir);
    if (idx >= 0)
        recentMoveDirs_.removeAt(idx);
    recentMoveDirs_.prepend(normalizedTargetDir);
    if (recentMoveDirs_.size() > 10) {
        recentMoveDirs_.removeLast();
    }

    // Usuwamy plik z listy i przechodzimy do następnego
    imageFiles_.removeAt(currentIndex_);

    if (imageFiles_.isEmpty()) {
        currentIndex_ = -1;
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
