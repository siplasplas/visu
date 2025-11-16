#ifndef VISU_THUMBLABEL_H
#define VISU_THUMBLABEL_H
#include <QLabel>

class ThumbLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ThumbLabel(const QString& path, QWidget* parent = nullptr);
signals:
    void clicked(const QString& path);

protected:
    void mousePressEvent(QMouseEvent* e) override;

private:
    QString path_;
};

#endif //VISU_THUMBLABEL_H
