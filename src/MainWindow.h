#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QString>

#include <opencv2/opencv.hpp>

class QLabel;
class ImageWidget;
class DoubleThresholdDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString& startFilePath = QString(),
                        QWidget* parent = nullptr);

private slots:
    void onPixelInfoChanged(int x, int y, int r, int g, int b);

protected:
    void keyPressEvent(QKeyEvent* event) override;

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


    ImageWidget* imageWidget_ = nullptr;
    QLabel* coordLabel_ = nullptr;
    QLabel* rgbLabel_   = nullptr;
    QLabel* indexLabel_ = nullptr;

    QVector<QString> imageFiles_;
    int currentIndex_ = -1;

    QString baseDirectory_;
    QVector<QString> recentMoveDirs_;

    // NEW: obrazy w pamięci
    cv::Mat originalMat_;
    cv::Mat currentMat_;

    void openDirectory();
    void openFile();

};

#endif // MAINWINDOW_H
