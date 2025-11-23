#include "BrowserWidget.h"
#include "DirOnlyModel.h"

#include <QTreeView>
#include <QScrollArea>
#include <QGridLayout>
#include <QDir>
#include <QDirIterator>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QtConcurrent/QtConcurrent>

#include "ThumbWidget.h"

BrowserWidget::BrowserWidget(QWidget* parent)
    : QWidget(parent)
{
    initUi();

    connect(&watcher_, &QFutureWatcher<void>::finished,
            this, &BrowserWidget::onThumbnailsFinished);
}

void BrowserWidget::onDirectorySelected(const QModelIndex& index)
{
    const QString dirPath = fsModel_->filePath(index);
    ensureDirectoryLoaded(dirPath);
}

void BrowserWidget::onThumbnailsFinished()
{
    //available for future use
}

void BrowserWidget::initUi()
{
    auto* mainLayout = new QHBoxLayout(this);

    fsModel_ = new DirOnlyModel(this);
    fsModel_->setRootPath(QDir::rootPath());

    tree_ = new QTreeView(this);
    tree_->setModel(fsModel_);

    // tylko kolumna Name
    for (int col = 1; col < fsModel_->columnCount(); ++col)
        tree_->hideColumn(col);

    tree_->setHeaderHidden(true);
    tree_->setRootIndex(fsModel_->index(QDir::homePath()));

    connect(tree_, &QTreeView::clicked,
            this, &BrowserWidget::onDirectorySelected);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);

    thumbsWidget_ = new QWidget(scrollArea_);
    gridLayout_ = new QGridLayout(thumbsWidget_);
    gridLayout_->setSpacing(4);
    gridLayout_->setContentsMargins(4,4,4,4);
    thumbsWidget_->setLayout(gridLayout_);
    scrollArea_->setWidget(thumbsWidget_);

    mainLayout->addWidget(tree_, 1);
    mainLayout->addWidget(scrollArea_, 3);
    setLayout(mainLayout);
}

void BrowserWidget::startLoadingThumbnails(const QString& dirPath)
{
    cancelFlag_ = false;

    auto future = QtConcurrent::run([this, dirPath]() {
        QDir dir(dirPath);
        QStringList nameFilters;
        nameFilters << "*.png" << "*.jpg" << "*.jpeg" << ".jp2" << "*.bmp"
                    << "*.gif" << "*.webp" << "*.tif" << "*.tiff" << "*.avif";

        QStringList files = dir.entryList(nameFilters, QDir::Files, QDir::Name);
        files.sort(Qt::CaseInsensitive);

        const int maxThumbW = 160;
        const int maxThumbH = 160;

        for (const QString& fn : files) {
            if (cancelFlag_.load())
                break;

            const QString filePath = dir.absoluteFilePath(fn);
            QImage img(filePath);
            if (img.isNull())
                continue;

            QImage thumb = img;
            if (img.width() > maxThumbW || img.height() > maxThumbH) {
                thumb = img.scaled(maxThumbW, maxThumbH,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
            }

            QMetaObject::invokeMethod(
                this,
                [this, filePath, thumb, dirPath]() {
                    // if we have moved to another directory in the meantime,
                    // we ignore this thumbnail as expired
                    if (dirPath != currentDir_)
                        return;

                    addThumbnail(filePath, thumb);
                },
                Qt::QueuedConnection
            );
        }
    });

    watcher_.setFuture(future);
}

void BrowserWidget::cancelThumbnailLoading()
{
    if (watcher_.isRunning()) {
        cancelFlag_.store(true);
        watcher_.waitForFinished();
    }
}

void BrowserWidget::addThumbnail(const QString& filePath, const QImage& image)
{
    int index = gridLayout_->count();
    int cols  = 4;
    int row   = index / cols;
    int col   = index % cols;

    auto* thumb = new ThumbWidget(filePath, image, thumbsWidget_);
    connect(thumb, &ThumbWidget::activated,
            this, &BrowserWidget::thumbnailActivated);

    gridLayout_->addWidget(thumb, row, col);
}

void BrowserWidget::setRootDirectory(const QString& path)
{
    QModelIndex idx = fsModel_->index(path);
    if (idx.isValid())
        tree_->setRootIndex(idx);
}

void BrowserWidget::ensureDirectoryLoaded(const QString& dirPath)
{
    if (dirPath.isEmpty())
        return;

    if (currentDir_ == dirPath && gridLayout_->count() > 0)
        return;

    cancelThumbnailLoading();
    currentDir_ = dirPath;

    QModelIndex idx = fsModel_->index(dirPath);
    if (idx.isValid()) {
        tree_->setCurrentIndex(idx);
        tree_->scrollTo(idx);
        tree_->expand(idx);
    }

    QLayoutItem* item;
    while ((item = gridLayout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    startLoadingThumbnails(dirPath);
}
