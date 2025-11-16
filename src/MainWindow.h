#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <array>
#include <QMainWindow>
#include <QVector>
#include <QString>
#include <QStackedWidget>
#include <QDateTime>
#include <QCloseEvent>

#include <opencv2/opencv.hpp>

class QLabel;
class ImageWidget;
class DoubleThresholdDialog;
class ShadowCompressionDialog;
class BrowserWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString& startFilePath = QString(),
                        QWidget* parent = nullptr);

private slots:
    void onPixelInfoChanged(int x, int y, int r, int g, int b);
    void switchToBrowserMode();
    void switchToSingleMode();
    void onThumbnailActivated(const QString& filePath);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void initUi();
    void scanDirectory(const QString& directory, const QString& startFilePath);
    bool isImageFile(const QString& filePath) const;
    void loadImageAt(int index);
    void updateIndexLabel();
    void goToIndex(int index);

    void moveCurrentImage();

    void openDoubleThresholdDialog();
    cv::Mat applyDoubleThresholdBW(const cv::Mat& src, int low, int high);
    cv::Mat applyDoubleThresholdPreview(const cv::Mat& src, int low, int high,
                                        bool lowColorize, bool highColorize);


    QStackedWidget* stacked_ = nullptr;
    BrowserWidget* browserWidget_ = nullptr;
    ImageWidget* imageWidget_ = nullptr;
    QLabel* coordLabel_ = nullptr;
    QLabel* rgbLabel_   = nullptr;
    QLabel* indexLabel_ = nullptr;

    QVector<QString> imageFiles_;
    int currentIndex_ = -1;

    QString baseDirectory_;
    QVector<QString> recentMoveDirs_;
    QString currentImageDir_;

    cv::Mat originalMat_;
    cv::Mat currentMat_;

    void openDirectory();
    void openFile();

    void openShadowCompressionDialog();
    cv::Mat applyShadowCompression(const cv::Mat& src, int T, double gamma, int maxOut);

    void computeHistogram(const cv::Mat& gray);
    int estimateTFromHistogram() const;

    std::array<uint64_t, 256> histogram_{};   // current histogram 0..255

    bool imageDirty_ = false;          // czy bieżący obraz zmodyfikowany
    QDateTime originalFileTime_;       // czas pliku przy wczytaniu
    bool hasOriginalFileTime_ = false; // czy mamy ważny timestamp

    bool maybeSaveCurrentImage();      // popup Save / Discard / Cancel
    bool saveCurrentImage(bool restoreTimestamp);
    void revertCurrentImage();
};

#endif // MAINWINDOW_H
