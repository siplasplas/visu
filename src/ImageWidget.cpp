#include "ImageWidget.h"

#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>

namespace {

const double minScale = 0.01;
const double maxScale = 100.0;

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


void ImageWidget::resetViewToFit()
{
    zoomMode_    = ZoomMode::AutoFit;
    scaleFactor_ = 1.0;
    panOffset_   = QPoint(0, 0);
    panning_     = false;
}

void ImageWidget::setImage(const cv::Mat& mat, bool resetView)
{
    // remember mat – shallow copy, no pixel copying
    imageMat_ = mat;

    // we create a QImage based on this matrix
    QImage newImg = matToQImage(imageMat_);
    qimage_ = std::move(newImg);

    if (qimage_.isNull()) {
        // no image - clear state
        imageMat_.release();
        hasImage_ = false;
        update();
        emit pixelInfoChanged(-1, -1, -1, -1, -1);
        return;
    }

    hasImage_ = true;

    if (resetView) {
        scaleFactor_ = 1.0;
        panOffset_ = QPoint(0, 0);
        zoomMode_    = ZoomMode::AutoFit;
    }

    update();
    emit pixelInfoChanged(-1, -1, -1, -1, -1);
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
    if (!hasImage_) {
        return;
    }

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

    const int scaledW = int(imgSizeOrig.width()  * scale);
    const int scaledH = int(imgSizeOrig.height() * scale);
    QSize imgSize(scaledW, scaledH);

    // base-centered rectangle (without borders)
    QRect baseRect(QPoint(0, 0), imgSize);
    baseRect.moveCenter(rect().center());

    QRect r = baseRect;

    // PAN only in Mode 2 and only when the image is larger than the widget
    if (zoomMode_ == ZoomMode::Fixed &&
        (scaledW > widgetSize.width() || scaledH > widgetSize.height())) {

        // apply current offset
        r.translate(panOffset_);

        // apply current offset
        // X
        if (scaledW <= widgetSize.width()) {
            // no horizontal control – centering
            int cx = (widgetSize.width() - scaledW) / 2;
            r.moveLeft(cx);
            panOffset_.setX(0);
        } else {
            int minX = widgetSize.width() - scaledW; // farthest to the left
            int maxX = 0;                            // farthest to the right

            if (r.left() > maxX) {
                int diff = r.left() - maxX;
                r.moveLeft(maxX);
                panOffset_.setX(panOffset_.x() - diff);
            } else if (r.left() < minX) {
                int diff = r.left() - minX;
                r.moveLeft(minX);
                panOffset_.setX(panOffset_.x() - diff);
            }
        }

        // Y
        if (scaledH <= widgetSize.height()) {
            int cy = (widgetSize.height() - scaledH) / 2;
            r.moveTop(cy);
            panOffset_.setY(0);
        } else {
            int minY = widgetSize.height() - scaledH;
            int maxY = 0;

            if (r.top() > maxY) {
                int diff = r.top() - maxY;
                r.moveTop(maxY);
                panOffset_.setY(panOffset_.y() - diff);
            } else if (r.top() < minY) {
                int diff = r.top() - minY;
                r.moveTop(minY);
                panOffset_.setY(panOffset_.y() - diff);
            }
        }
    } else {
        // no control – reset offset, just in case
        panOffset_ = QPoint(0, 0);
    }

    targetRect_ = r;

    // AREA mode vs NEAREST mode according to scale
    if (scale <= 1.0) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);  // downscale (AREA-like)
    } else {
        p.setRenderHint(QPainter::SmoothPixmapTransform, false); // upscale (NEAREST-like)
    }

    p.drawImage(targetRect_, qimage_);
}


void ImageWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (panning_) {
        QPoint delta = event->pos() - lastMousePos_;
        lastMousePos_ = event->pos();
        panOffset_ += delta;
        update();
        event->accept();
        return;
    }
    updatePixelInfoAt(event->pos());
    QWidget::mouseMoveEvent(event);
}

void ImageWidget::updatePixelInfoAt(const QPoint& pos)
{
    if (!hasImage_) {
        emit pixelInfoChanged(-1,-1,-1,-1,-1);
        return;
    }

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

    // no scaling by default (1:1)
    double scale = 1.0;

    // only if the image is larger than the widget – we reduce it
    if (imageSize.width()  > widgetSize.width() ||
        imageSize.height() > widgetSize.height()) {

        const double sx = double(widgetSize.width())  / imageSize.width();
        const double sy = double(widgetSize.height()) / imageSize.height();
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
        panOffset_   = QPoint(0, 0);
    }

    scaleFactor_ *= factor;

    if (scaleFactor_ < minScale) scaleFactor_ = minScale;
    if (scaleFactor_ > maxScale) scaleFactor_ = maxScale;

    update();
}

void ImageWidget::applyZoomAt(const QPoint& anchorWidget, double factor)
{
    if (qimage_.isNull())
        return;

    const QSize widgetSize  = size();
    const QSize imgSizeOrig = qimage_.size();

    // przejście AutoFit -> Fixed przy pierwszym zoomie
    if (zoomMode_ == ZoomMode::AutoFit) {
        double fitScale = computeFitScale(widgetSize, imgSizeOrig);
        if (fitScale <= 0.0)
            fitScale = 1.0;
        zoomMode_    = ZoomMode::Fixed;
        scaleFactor_ = fitScale;
        panOffset_   = QPoint(0, 0);
    }

    double oldScale = scaleFactor_;
    double newScale = scaleFactor_ * factor;

    if (newScale < minScale) newScale = minScale;
    if (newScale > maxScale) newScale = maxScale;

    // jeśli realnie się nie zmienia, nic nie rób
    if (std::abs(newScale - oldScale) < 1e-6)
        return;

    // prostokąt przed zoomem
    int oldW = int(imgSizeOrig.width()  * oldScale);
    int oldH = int(imgSizeOrig.height() * oldScale);
    QRect baseOld(QPoint(0, 0), QSize(oldW, oldH));
    baseOld.moveCenter(rect().center());
    QRect rOld = baseOld.translated(panOffset_);

    // współrzędne punktu obrazu pod kursorem przed zoomem
    QPointF anchor = anchorWidget;
    QPointF imgCoord(
        (anchor.x() - rOld.left()) / oldScale,
        (anchor.y() - rOld.top())  / oldScale
    );

    // prostokąt bazowy po zoomie (bez panOffset)
    int newW = int(imgSizeOrig.width()  * newScale);
    int newH = int(imgSizeOrig.height() * newScale);
    QRect baseNew(QPoint(0, 0), QSize(newW, newH));
    baseNew.moveCenter(rect().center());

    // chcemy: anchorWidget = topLeftNew + imgCoord * newScale
    QPointF topLeftNewF(
        anchor.x() - imgCoord.x() * newScale,
        anchor.y() - imgCoord.y() * newScale
    );
    QPoint topLeftNew = topLeftNewF.toPoint();

    // z tego wyliczamy nowy panOffset_
    panOffset_ = topLeftNew - baseNew.topLeft();

    scaleFactor_ = newScale;
    update();
}


void ImageWidget::zoomIn()
{
    QPoint center = rect().center();
    applyZoomAt(center, 1.25);        // +25%
}

void ImageWidget::zoomOut()
{
    QPoint center = rect().center();
    applyZoomAt(center, 1.0 / 1.25); // -20%
}

#include <QWheelEvent>

void ImageWidget::wheelEvent(QWheelEvent* event)
{
    if (qimage_.isNull()) {
        QWidget::wheelEvent(event);
        return;
    }

    QPoint anchor = event->position().toPoint();

    if (event->angleDelta().y() > 0) {
        applyZoomAt(anchor, 1.25);
    } else if (event->angleDelta().y() < 0) {
        applyZoomAt(anchor, 1.0 / 1.25);
    }

    event->accept();
}

void ImageWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && hasImage_) {
        updatePixelInfoAt(event->pos());
        emit contextMenuRequested(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton &&
        zoomMode_ == ZoomMode::Fixed &&
        !qimage_.isNull()) {

        const QSize widgetSize  = size();
        const QSize imgSizeOrig = qimage_.size();
        const int scaledW = int(imgSizeOrig.width()  * scaleFactor_);
        const int scaledH = int(imgSizeOrig.height() * scaleFactor_);

        // panowanie tylko jeśli obraz większy niż widget
        if (scaledW > widgetSize.width() || scaledH > widgetSize.height()) {
            panning_ = true;
            lastMousePos_ = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        }

    QWidget::mousePressEvent(event);
}

void ImageWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && panning_) {
        panning_ = false;
        unsetCursor();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void ImageWidget::zoomResetTo100()
{
    if (qimage_.isNull())
        return;

    zoomMode_    = ZoomMode::Fixed;
    scaleFactor_ = 1.0;
    panOffset_   = QPoint(0, 0);
    update();
}

void ImageWidget::zoomFit()
{
    if (qimage_.isNull())
        return;

    zoomMode_    = ZoomMode::AutoFit;
    scaleFactor_ = 1.0;
    panOffset_   = QPoint(0, 0);
    update();
}

void ImageWidget::clearImage()
{
    hasImage_ = false;
    qimage_ = QImage();
    targetRect_ = QRect();
    panOffset_ = QPoint(0, 0);
    zoomMode_ = ZoomMode::AutoFit;
    scaleFactor_ = 1.0;
    panning_ = false;
    update();
}
