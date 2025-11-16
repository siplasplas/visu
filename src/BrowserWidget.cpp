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
    QString dirPath = fsModel_->filePath(index);
    startLoadingThumbnails(dirPath);
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
    cancelThumbnailLoading();

    currentDir_ = dirPath;
    cancelFlag_ = false;

    auto future = QtConcurrent::run([this, dirPath]() {
        QDir dir(dirPath);
        QStringList nameFilters;
        nameFilters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp"
                    << "*.gif" << "*.webp" << "*.tif" << "*.tiff";

        QStringList files = dir.entryList(nameFilters, QDir::Files, QDir::Name);
        files.sort(Qt::CaseInsensitive);

        for (const QString& fn : files) {
            if (cancelFlag_.load())
                break;

            const QString filePath = dir.absoluteFilePath(fn);
            QImage img(filePath);
            if (img.isNull())
                continue;

            QImage thumb = img.scaled(160, 160, Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation);

            QMetaObject::invokeMethod(
                this,
                [this, filePath, thumb]() {
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
    int cols = 4;
    int row = index / cols;
    int col = index % cols;

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

    // different directory than before → full switch
    if (currentDir_ != dirPath) {
        cancelThumbnailLoading();

        currentDir_ = dirPath;

        // setting root in the tree (lazy directories)
        QModelIndex idx = fsModel_->index(dirPath);
        if (idx.isValid())
            tree_->setRootIndex(idx);

        // clear old thumbnails
        QLayoutItem* item;
        while ((item = gridLayout_->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }

        startLoadingThumbnails(dirPath);
        return;
    }

    // same directory:
    // - if it is still loading → do nothing (continues in the background)
    // - if it is finished and thumbnails are present → also do nothing
    // (if necessary, you can add checking for changes on the disk later)
}
