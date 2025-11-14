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

    void setImage(const cv::Mat& mat);
    const cv::Mat& imageMat() const { return imageMat_; }

    void updatePixelInfoAt(const QPoint& widgetPos);

    signals:
        void pixelInfoChanged(int x, int y, int r, int g, int b);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    cv::Mat imageMat_;
    QImage qimage_;
    QRect targetRect_; // obszar, w którym rysujemy obraz
};

#endif // IMAGEWIDGET_H
