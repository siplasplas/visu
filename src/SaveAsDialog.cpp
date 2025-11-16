#include "SaveAsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QWindow>

SaveAsDialog::SaveAsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Save As"));
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setModal(false);

    auto* mainLayout = new QVBoxLayout(this);

    formatCombo_ = new QComboBox(this);
    // jeden wspólny zestaw formatów out – możesz uszeregować jak chcesz:
    // png, jpg, gif, webp, avif, bmp, tiff
    formatCombo_->addItem("PNG (*.png)");
    formatCombo_->addItem("JPG (*.jpg)");
    formatCombo_->addItem("GIF (*.gif)");
    formatCombo_->addItem("WEBP (*.webp)");
    formatCombo_->addItem("AVIF (*.avif)");
    formatCombo_->addItem("BMP (*.bmp)");
    formatCombo_->addItem("TIFF (*.tif)");

    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SaveAsDialog::onFormatChanged);

    auto* formatLayout = new QHBoxLayout;
    formatLayout->addWidget(new QLabel(tr("Format:"), this));
    formatLayout->addWidget(formatCombo_);
    mainLayout->addLayout(formatLayout);

    // quality controls (dla stratnych)
    qualitySlider_ = new QSlider(Qt::Horizontal, this);
    qualitySlider_->setRange(1, 100);
    qualitySlider_->setValue(90);

    qualityEdit_ = new QLineEdit(QString::number(90), this);
    qualityEdit_->setFixedWidth(50);

    auto* qualityLayout = new QHBoxLayout;
    qualityLayout->addWidget(new QLabel(tr("Quality:"), this));
    qualityLayout->addWidget(qualitySlider_);
    qualityLayout->addWidget(qualityEdit_);
    mainLayout->addLayout(qualityLayout);

    connect(qualitySlider_, &QSlider::valueChanged,
            this, &SaveAsDialog::onQualitySliderChanged);
    connect(qualityEdit_, &QLineEdit::textChanged,
            this, &SaveAsDialog::onQualityEditChanged);

    showOriginalCheck_ = new QCheckBox(tr("Show original"), this);
    connect(showOriginalCheck_, &QCheckBox::toggled,
            this, &SaveAsDialog::onShowOriginalToggled);
    mainLayout->addWidget(showOriginalCheck_);

    sizeInfoLabel_ = new QLabel(tr("Size: -"), this);
    mainLayout->addWidget(sizeInfoLabel_);

    auto* btnLayout = new QHBoxLayout;
    auto* applyBtn = new QPushButton(tr("Preview"), this);
    auto* saveBtn  = new QPushButton(tr("Save"), this);
    auto* closeBtn = new QPushButton(tr("Close"), this);

    btnLayout->addWidget(applyBtn);
    btnLayout->addWidget(saveBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    connect(applyBtn, &QPushButton::clicked,
            this, &SaveAsDialog::onApplyClicked);
    connect(saveBtn, &QPushButton::clicked,
            this, &SaveAsDialog::onSaveClicked);
    connect(closeBtn, &QPushButton::clicked,
            this, &QDialog::close);

    onFormatChanged(formatCombo_->currentIndex());
}

ImageFormat SaveAsDialog::formatFromComboIndex(int index) const
{
    switch (index) {
    case 0: return ImageFormat::Png;
    case 1: return ImageFormat::Jpeg;  // tylko "jpg" w UI
    case 2: return ImageFormat::Gif;
    case 3: return ImageFormat::Webp;
    case 4: return ImageFormat::Avif;
    case 5: return ImageFormat::Bmp;
    case 6: return ImageFormat::Tiff;
    default: return ImageFormat::Unknown;
    }
}

int SaveAsDialog::formatToComboIndex(ImageFormat fmt) const
{
    switch (fmt) {
    case ImageFormat::Png:  return 0;
    case ImageFormat::Jpeg: return 1;
    case ImageFormat::Gif:  return 2;
    case ImageFormat::Webp: return 3;
    case ImageFormat::Avif: return 4;
    case ImageFormat::Bmp:  return 5;
    case ImageFormat::Tiff: return 6;
    default: return 0;
    }
}

ImageFormat SaveAsDialog::selectedFormat() const
{
    return formatFromComboIndex(formatCombo_->currentIndex());
}

int SaveAsDialog::quality() const
{
    return qualitySlider_->value();
}

bool SaveAsDialog::showOriginalChecked() const
{
    return showOriginalCheck_->isChecked();
}

void SaveAsDialog::setLossy(bool lossy)
{
    isLossyCurrent_ = lossy;
    qualitySlider_->setEnabled(lossy);
    qualityEdit_->setEnabled(lossy);
    // Dla bezstratnych formatów slider/edit wyłączone, ale dialog może służyć tylko do wyboru formatu i Save.
}

void SaveAsDialog::setFileSizeInfo(qint64 bytes, double ratio)
{
    // ratio = compressed_size / (w*h*channels)
    QString text = tr("Size: %1 bytes, %2x smaller than raw RGB")
                       .arg(bytes)
                       .arg(ratio, 0, 'f', 2);
    sizeInfoLabel_->setText(text);
    sizeInfoLabel_->repaint();
}

void SaveAsDialog::clearFileSizeInfo()
{
    sizeInfoLabel_->clear();
    sizeInfoLabel_->repaint();
}

void SaveAsDialog::onFormatChanged(int index)
{
    ImageFormat fmt = formatFromComboIndex(index);
    bool lossy = isAlwaysLossyFormat(fmt)
                 || (fmt == ImageFormat::Webp)
                 || (fmt == ImageFormat::Avif);
    // Możesz tu dodać swoją logikę: WebP/AVIF – traktujesz jako „kompresja stratna
    // w tym trybie Save As”.
    setLossy(lossy);

    // Po zmianie formatu warto wymusić odświeżenie preview:
    emit previewRequested(selectedFormat(), quality(), showOriginalChecked());
}

void SaveAsDialog::onQualitySliderChanged(int value)
{
    if (qualityEdit_->text().toInt() != value)
        qualityEdit_->setText(QString::number(value));
    if (isLossyCurrent_)
        emit previewRequested(selectedFormat(), value, showOriginalChecked());
}

void SaveAsDialog::onQualityEditChanged(const QString& text)
{
    bool ok = false;
    int v = text.toInt(&ok);
    if (!ok)
        return;
    if (v < 1) v = 1;
    if (v > 100) v = 100;
    if (qualitySlider_->value() != v)
        qualitySlider_->setValue(v);
    if (isLossyCurrent_)
        emit previewRequested(selectedFormat(), v, showOriginalChecked());
}

void SaveAsDialog::onShowOriginalToggled(bool)
{
    emit previewRequested(selectedFormat(), quality(), showOriginalChecked());
}

void SaveAsDialog::onApplyClicked()
{
    emit previewRequested(selectedFormat(), quality(), showOriginalChecked());
}

void SaveAsDialog::onSaveClicked()
{
    emit acceptedSave(selectedFormat(), quality(), showOriginalChecked());
}
