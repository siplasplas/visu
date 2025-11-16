#ifndef THUMBWIDGET_H
#define THUMBWIDGET_H
#include <QWidget>

class QLabel;

class ThumbWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ThumbWidget(const QString& filePath, const QImage& img, QWidget* parent = nullptr);

    [[nodiscard]] QString filePath() const { return filePath_; }

signals:
    void activated(const QString& path);

protected:
    void mousePressEvent(QMouseEvent* e) override;

private:
    QString filePath_;
    QLabel* imageLabel_ = nullptr;
    QLabel* textLabel_  = nullptr;
};

#endif //THUMBWIDGET_H