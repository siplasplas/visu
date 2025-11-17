#include "SaveAsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QWindow>

#include "image_metrics.h"

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

    qualitySpin_ = new QSpinBox(this);
    qualitySpin_->setRange(0, 100);
    qualitySpin_->setValue(90);

    auto* qualityLayout = new QHBoxLayout;
    qualityLayout->addWidget(new QLabel(tr("Quality:"), this));
    qualityLayout->addWidget(qualitySlider_);
    qualityLayout->addWidget(qualitySpin_);
    mainLayout->addLayout(qualityLayout);

    connect(qualitySlider_, &QSlider::valueChanged,
            this, &SaveAsDialog::onQualitySliderChanged);
    connect(qualitySpin_, &QSpinBox::valueChanged,
            this, &SaveAsDialog::onQualitySpinChanged);

    showOriginalCheck_ = new QCheckBox(tr("Show original"), this);
    connect(showOriginalCheck_, &QCheckBox::toggled,
            this, &SaveAsDialog::onShowOriginalToggled);
    mainLayout->addWidget(showOriginalCheck_);

    sizeInfoLabel_ = new QLabel(tr("Size: -"), this);
    mainLayout->addWidget(sizeInfoLabel_);

    auto* metricsLayout = new QHBoxLayout;
    metricsLayout->addWidget(new QLabel(tr("Metrics:"), this));

    const auto& defs = getAvailableMetrics();
    for (const auto& def : defs) {
        auto* chk = new QCheckBox(QString::fromLatin1(def.label), this);
        metricsLayout->addWidget(chk);

        metricChecks_.push_back(chk);
        metricTypes_.push_back(def.type);

        connect(chk, &QCheckBox::toggled,
                this, &SaveAsDialog::onMetricToggled);
    }

    metricsLayout->addStretch();
    mainLayout->addLayout(metricsLayout);

    metricResultLabel_ = new QLabel(this);
    metricResultLabel_->setVisible(false);
    mainLayout->addWidget(metricResultLabel_);

    metricsLayout->addStretch();
    mainLayout->addLayout(metricsLayout);

    metricResultLabel_ = new QLabel(this);
    metricResultLabel_->setVisible(false);
    mainLayout->addWidget(metricResultLabel_);

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
    qualitySpin_->setEnabled(lossy);
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
    // You can add your logic here: WebP/AVIF – treat as "lossy compression
    // in this Save As mode."
    setLossy(lossy);

    clearFileSizeInfo();
    clearMetricInfo();

    // After changing the format, it is worth forcing a refresh of the preview:
    emit previewRequested(selectedFormat(), quality(), showOriginalChecked());
}

void SaveAsDialog::onQualitySliderChanged(int value)
{
    if (qualitySpin_->value() != value)
        qualitySpin_->setValue(value);
    if (isLossyCurrent_)
        emit previewRequested(selectedFormat(), value, showOriginalChecked());
}

void SaveAsDialog::onQualitySpinChanged(int value)
{
    int v = value;
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

void SaveAsDialog::closeEvent(QCloseEvent* e)
{
    emit dialogClosed();
    QDialog::closeEvent(e);
}

MetricType SaveAsDialog::selectedMetric() const
{
    return selectedMetric_;
}

void SaveAsDialog::setMetricInfo(const QString& text)
{
    metricResultLabel_->setText(text);
    metricResultLabel_->setVisible(!text.isEmpty());
}

void SaveAsDialog::clearMetricInfo()
{
    metricResultLabel_->clear();
    metricResultLabel_->setVisible(false);
}


void SaveAsDialog::onMetricToggled(bool checked)
{
    // Determine which checkbox triggered the slot
    auto* senderChk = qobject_cast<QCheckBox*>(sender());
    if (!senderChk)
        return;

    // find index
    int idx = -1;
    for (int i = 0; i < metricChecks_.size(); ++i) {
        if (metricChecks_[i] == senderChk) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return;

    if (checked) {
        // one metric enabled → disable all others
        for (int i = 0; i < metricChecks_.size(); ++i) {
            if (i == idx) continue;
            metricChecks_[i]->blockSignals(true);
            metricChecks_[i]->setChecked(false);
            metricChecks_[i]->blockSignals(false);
        }
        selectedMetric_ = metricTypes_[idx];
    } else {
        // this checkbox unchecked → if no other is checked, we have None
        bool anyChecked = false;
        for (auto* chk : metricChecks_) {
            if (chk->isChecked()) {
                anyChecked = true;
                break;
            }
        }
        if (!anyChecked)
            selectedMetric_ = MetricType::None;
    }

    clearMetricInfo();

    // immediately refresh the preview with the new metric
    emit previewRequested(selectedFormat(), quality(), showOriginalChecked());
}
