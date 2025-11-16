#include "BrowserWidget.h"

#include <QFileSystemModel>
#include <QTreeView>
#include <QScrollArea>
#include <QGridLayout>
#include <QDir>
#include <QDirIterator>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QtConcurrent/QtConcurrent>

#include "ThumbLabel.h"

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

    fsModel_ = new QFileSystemModel(this);
    fsModel_->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    fsModel_->setRootPath(QDir::rootPath());

    tree_ = new QTreeView(this);
    tree_->setModel(fsModel_);
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

    // clear previous thumbs
    QLayoutItem* item;
    while ((item = gridLayout_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    cancelFlag_ = false;

    auto future = QtConcurrent::run([this, dirPath]() {
        QDir dir(dirPath);
        QStringList nameFilters;
        nameFilters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.gif" << "*.webp" << "*.tif" << "*.tiff";

        QDirIterator it(dirPath, nameFilters, QDir::Files);
        int row = 0, col = 0;

        while (it.hasNext()) {
            if (cancelFlag_.load())
                break;

            const QString filePath = it.next();

            QImage img(filePath);
            if (img.isNull())
                continue;

            QImage thumb = img.scaled(160, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            // przekazujemy do GUI
            QMetaObject::invokeMethod(
                this,
                [this, filePath, thumb, row, col]() {
                    addThumbnail(filePath, thumb);
                },
                Qt::QueuedConnection
            );

            // prosty układ gridowy 4 kolumny
            ++col;
            if (col >= 4) {
                col = 0;
                ++row;
            }
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

    auto* label = new ThumbLabel(filePath, thumbsWidget_);
    label->setPixmap(QPixmap::fromImage(image));
    label->setScaledContents(false);

    connect(label, &ThumbLabel::clicked,
            this, &BrowserWidget::thumbnailActivated);

    gridLayout_->addWidget(label, row, col);
}
