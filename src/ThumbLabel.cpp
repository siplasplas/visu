#include "ThumbLabel.h"
#include <QMouseEvent>

ThumbLabel::ThumbLabel(const QString &path, QWidget *parent): QLabel(parent), path_(path) {
    setCursor(Qt::PointingHandCursor);
}

void ThumbLabel::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton)
        emit clicked(path_);
    QLabel::mousePressEvent(e);
}
