#ifndef IMAGEWIDGET_H
#define IMAGEWIDGET_H

#include <QWidget>
#include <QImage>
#include <QRect>
#include <opencv2/opencv.hpp>

class ImageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ImageWidget(QWidget* parent = nullptr);

    void resetViewToFit();

    // resetView = true → behavior as before (autofit, etc.)
    // resetView = false → retains current zoom/pan if image size is the same
    void setImage(const cv::Mat& mat, bool resetView);

    const cv::Mat& imageMat() const { return imageMat_; }

    void resetZoom();
    void zoomIn();
    void zoomOut();
    void zoomResetTo100();  // key '0'
    void zoomFit();         // key 'f'

    void updatePixelInfoAt(const QPoint& widgetPos);
    void clearImage();
signals:
    void pixelInfoChanged(int x, int y, int r, int g, int b);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;    // TAK
    void resizeEvent(QResizeEvent* event) override;  //

private:
    cv::Mat imageMat_;
    QImage qimage_;
    QRect targetRect_;

    enum class ZoomMode { AutoFit, Fixed };
    ZoomMode zoomMode_ = ZoomMode::AutoFit;
    double   scaleFactor_ = 1.0;  //for mode Fixed

    // PAN
    QPoint panOffset_{0, 0};   // offset in screen pixels
    bool   panning_ = false;
    QPoint lastMousePos_;

    double computeFitScale(const QSize& widgetSize, const QSize& imageSize) const;
    void   applyZoom(double factor);
    void applyZoomAt(const QPoint& anchorWidget, double factor);
};

#endif // IMAGEWIDGET_H
