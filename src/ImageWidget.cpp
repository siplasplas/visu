#include "ImageWidget.h"

#include <QPainter>
#include <QMouseEvent>

namespace {

QImage matToQImage(const cv::Mat& src)
{
    if (src.empty())
        return QImage();

    if (src.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(src, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step,
                      QImage::Format_RGB888).copy();
    } else if (src.type() == CV_8UC1) {
        return QImage(src.data, src.cols, src.rows, src.step,
                      QImage::Format_Grayscale8).copy();
    } else {
        // inne formaty można obsłużyć później
        cv::Mat tmp;
        src.convertTo(tmp, CV_8U);
        return QImage(tmp.data, tmp.cols, tmp.rows, tmp.step,
                      QImage::Format_Grayscale8).copy();
    }
}

} // namespace

ImageWidget::ImageWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
}

void ImageWidget::setImage(const cv::Mat& mat)
{
    imageMat_ = mat.clone();
    qimage_ = matToQImage(imageMat_);

    zoomMode_    = ZoomMode::AutoFit;
    scaleFactor_ = 1.0;

    update();
}


void ImageWidget::resetZoom()
{
    zoomMode_    = ZoomMode::AutoFit;
    scaleFactor_ = 1.0;
    update();
}

void ImageWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (qimage_.isNull())
        return;

    const QSize widgetSize  = size();
    const QSize imgSizeOrig = qimage_.size();

    double scale = 1.0;

    if (zoomMode_ == ZoomMode::AutoFit) {
        // Mode 1: auto-fit, only reduces, never enlarges
        scale = computeFitScale(widgetSize, imgSizeOrig);
    } else {
        // Mode 2: fixed scale, image may be larger than the widget (cropped)
        scale = scaleFactor_;
    }

    QSize imgSize(
        int(imgSizeOrig.width()  * scale),
        int(imgSizeOrig.height() * scale)
    );

    targetRect_ = QRect(QPoint(0, 0), imgSize);
    targetRect_.moveCenter(rect().center());

    // SELECTION OF “AREA vs NEAREST”:
    // - scale < 1 → downscale → better quality (like AREA)
    // - scale > 1 → upscale  → simplest filtering (like NEAREST)
    if (scale <= 1.0) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    } else {
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    }

    p.drawImage(targetRect_, qimage_);
}

void ImageWidget::mouseMoveEvent(QMouseEvent* event)
{
    updatePixelInfoAt(event->pos());
}

void ImageWidget::updatePixelInfoAt(const QPoint& pos)
{
    if (imageMat_.empty() || qimage_.isNull()) {
        emit pixelInfoChanged(-1, -1, -1, -1, -1);
        return;
    }

    if (!targetRect_.contains(pos)) {
        emit pixelInfoChanged(-1, -1, -1, -1, -1);
        return;
    }

    double scaleX = double(qimage_.width())  / double(targetRect_.width());
    double scaleY = double(qimage_.height()) / double(targetRect_.height());

    int imgX = int((pos.x() - targetRect_.left()) * scaleX);
    int imgY = int((pos.y() - targetRect_.top())  * scaleY);

    if (imgX < 0 || imgY < 0 ||
        imgX >= imageMat_.cols || imgY >= imageMat_.rows) {
        emit pixelInfoChanged(-1, -1, -1, -1, -1);
        return;
        }

    int r = -1, g = -1, b = -1;

    if (imageMat_.channels() == 3) {
        cv::Vec3b bgr = imageMat_.at<cv::Vec3b>(imgY, imgX);
        b = bgr[0];
        g = bgr[1];
        r = bgr[2];
    } else if (imageMat_.channels() == 1) {
        uchar v = imageMat_.at<uchar>(imgY, imgX);
        r = g = b = int(v);
    }

    emit pixelInfoChanged(imgX, imgY, r, g, b);
}

double ImageWidget::computeFitScale(const QSize& widgetSize,
                                    const QSize& imageSize) const
{
    if (imageSize.isEmpty())
        return 1.0;

    // domyślnie brak skalowania (1:1)
    double scale = 1.0;

    // tylko jeśli obraz jest większy niż widget – zmniejszamy
    if (imageSize.width()  > widgetSize.width() ||
        imageSize.height() > widgetSize.height()) {

        const double sx = static_cast<double>(widgetSize.width())  /
                          static_cast<double>(imageSize.width());
        const double sy = static_cast<double>(widgetSize.height()) /
                          static_cast<double>(imageSize.height());

        scale = std::min(sx, sy);
        }

    return scale;
}

void ImageWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // In Mode 1, changing the size of the widget changes the fit
    if (zoomMode_ == ZoomMode::AutoFit) {
        update();
    }
    // In Mode 2, nothing special – fixed scale, image may be cropped
}

void ImageWidget::applyZoom(double factor)
{
    if (qimage_.isNull())
        return;

    // first zoom – transition from AutoFit to Fixed
    if (zoomMode_ == ZoomMode::AutoFit) {
        double fitScale = computeFitScale(size(), qimage_.size());
        if (fitScale <= 0.0)
            fitScale = 1.0;
        zoomMode_    = ZoomMode::Fixed;
        scaleFactor_ = fitScale;
    }

    scaleFactor_ *= factor;

    const double minScale = 0.01;
    const double maxScale = 80.0;
    if (scaleFactor_ < minScale) scaleFactor_ = minScale;
    if (scaleFactor_ > maxScale) scaleFactor_ = maxScale;

    update();
}

void ImageWidget::zoomIn()
{
    // np. +25% na klik
    applyZoom(1.25);
}

void ImageWidget::zoomOut()
{
    applyZoom(1.0 / 1.25);
}

#include <QWheelEvent>

void ImageWidget::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0) {
        zoomIn();
    } else if (event->angleDelta().y() < 0) {
        zoomOut();
    }
    event->accept();
}
