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
    update();
}

void ImageWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (qimage_.isNull())
        return;

    QSize widgetSize   = size();
    QSize imgSizeOrig  = qimage_.size();
    QSize imgSize      = imgSizeOrig;

    if (imgSize.width() > widgetSize.width() ||
        imgSize.height() > widgetSize.height()) {
        imgSize.scale(widgetSize, Qt::KeepAspectRatio);
        } else {
            imgSize = imgSizeOrig; // 1:1
        }

    targetRect_ = QRect(QPoint(0, 0), imgSize);
    targetRect_.moveCenter(rect().center());

    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
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
