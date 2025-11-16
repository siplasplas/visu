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

    signals:
        void thumbnailActivated(const QString& filePath); // klik w miniaturę

private slots:
    void onDirectorySelected(const QModelIndex& index);
    void onThumbnailsFinished(); // koniec batcha

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
};

#endif // BROWSERWIDGET_H
