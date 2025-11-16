#ifndef BROWSERWIDGET_H
#define BROWSERWIDGET_H

#include <QWidget>
#include <QFutureWatcher>
#include <QPointer>

class QFileSystemModel;
class QTreeView;
class QScrollArea;
class QGridLayout;

class BrowserWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BrowserWidget(QWidget* parent = nullptr);
    void setRootDirectory(const QString &path);
    void ensureDirectoryLoaded(const QString& dirPath);

signals:
    void thumbnailActivated(const QString& filePath); // click thumb

private slots:
    void onDirectorySelected(const QModelIndex& index);
    void onThumbnailsFinished();

private:
    void initUi();
    void startLoadingThumbnails(const QString& dirPath);
    void cancelThumbnailLoading();
    void addThumbnail(const QString& filePath, const QImage& image);

    QFileSystemModel* fsModel_     = nullptr;
    QTreeView*        tree_        = nullptr;
    QScrollArea*      scrollArea_  = nullptr;
    QWidget*          thumbsWidget_ = nullptr;
    QGridLayout*      gridLayout_  = nullptr;

    QFutureWatcher<void> watcher_;
    std::atomic<bool> cancelFlag_{false};

    QString currentDir_;
};

#endif // BROWSERWIDGET_H
