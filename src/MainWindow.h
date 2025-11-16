#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <array>
#include <cmath>
#include <QMainWindow>
#include <QVector>
#include <QString>
#include <QStackedWidget>
#include <QDateTime>
#include <QCloseEvent>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <optional>

#include <opencv2/opencv.hpp>

#include "SaveAsDialog.h"

class QLabel;
class ImageWidget;
class DoubleThresholdDialog;
class ShadowCompressionDialog;
class BrowserWidget;

struct SaveAsRequest {
    ImageFormat format;
    int quality;
    bool showOriginal;
};

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
    void onShowOriginalClicked();
    void onPreviewJobFinished();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void initUi();

    void updateActionsForMode();

    void scanDirectory(const QString& directory, const QString& startFilePath);

    cv::Mat loadAnyImage(const QString &path);

    bool isImageFile(const QString& filePath) const;
    void loadImageAt(int index);
    void updateIndexLabel();
    void goToIndex(int index);

    void moveCurrentImage();
    double computePsnr(const cv::Mat& ref, const cv::Mat& test) const;
    double computeSsim(const cv::Mat& ref, const cv::Mat& test) const;

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

    QAction* moveAct_;
    QAction* doubleThAct_;
    QAction* shadowCompAct_;

    void openDirectory();
    void openFile();

    void openShadowCompressionDialog();
    cv::Mat applyShadowCompression(const cv::Mat& src, int T, double gamma, int maxOut);

    void computeHistogram(const cv::Mat& gray);
    int estimateTFromHistogram() const;

    std::array<uint64_t, 256> histogram_{};   // current histogram 0..255

    bool imageDirty_ = false;          // czy bieżący obraz zmodyfikowany
    bool safeOnlyDirty_ = true;
    bool isCurrentFileJpeg_   = false;
    int  pendingJpegRotSteps_ = 0;   // 0..3 kroki po 90°
    void rotateCurrentImageLeft();
    void rotateCurrentImageRight();

    void restoreOriginalTimestamp(const QString & path);

    bool applyPendingJpegRotationOnDisk(bool restoreTimestamp);

    void openSaveAsDialog();

    void onSaveAsPreviewRequested(ImageFormat fmt, int quality, bool showOriginal);
    void startPreviewJob(const SaveAsRequest& req);
    void onSaveAsAccepted(ImageFormat fmt, int quality, bool showOriginal);

    QDateTime originalFileTime_;       // czas pliku przy wczytaniu
    bool hasOriginalFileTime_ = false; // czy mamy ważny timestamp

    bool maybeSaveCurrentImage();      // popup Save / Discard / Cancel
    bool saveCurrentImage(bool restoreTimestamp);
    void revertCurrentImage();

    void deleteCurrentImageToTrash();
    bool canDoLosslessJpegRotation() const;
    bool isInTrash(const QString& path) const;
    SaveAsDialog* saveAsDlg_ = nullptr;
    cv::Mat previewMat_;           // ostatni skompresowany preview
    QString previewTempPath_;
    bool saveLossless(const QString& path, ImageFormat fmt, const cv::Mat& mat);
    bool saveLossy(const QString& path, ImageFormat fmt, const cv::Mat& mat, int quality);

    void closeSaveAsDialog();

    QFutureWatcher<bool>* previewWatcher_ = nullptr;
    bool previewJobRunning_ = false;
    std::optional<SaveAsRequest> pendingRequest_;
    QString previewTmpPath_;
};

#endif // MAINWINDOW_H
