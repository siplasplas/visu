#include "ThumbWidget.h"
#include <QFileInfo>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>

ThumbWidget::ThumbWidget(const QString &filePath, const QImage &img, QWidget *parent): QWidget(parent)
    , filePath_(filePath) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    imageLabel_ = new QLabel(this);
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setPixmap(QPixmap::fromImage(img));
    imageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    textLabel_ = new QLabel(this);
    textLabel_->setAlignment(Qt::AlignCenter);
    textLabel_->setWordWrap(false);
    textLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QFileInfo fi(filePath);
    QString baseName = fi.fileName();

    // simple eliding
    QFontMetrics fm(textLabel_->font());
    QString elided = fm.elidedText(baseName, Qt::ElideMiddle, 140); // width in px
    textLabel_->setText(elided);

    layout->addWidget(imageLabel_);
    layout->addWidget(textLabel_);

    setLayout(layout);

    setCursor(Qt::PointingHandCursor);
}

void ThumbWidget::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton)
        emit activated(filePath_);
    QWidget::mousePressEvent(e);
}
