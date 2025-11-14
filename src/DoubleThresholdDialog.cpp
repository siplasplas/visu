#include "DoubleThresholdDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QCheckBox>

DoubleThresholdDialog::DoubleThresholdDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Double Threshold"));
    setWindowModality(Qt::NonModal);
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* mainLayout = new QVBoxLayout(this);

    // Wiersz 1: [apply] [spinbox low] [color checkbox]
    {
        auto* row = new QHBoxLayout;

        lowApply_ = new QCheckBox(tr("apply"), this);
        lowApply_->setChecked(true);

        lowSpin_ = new QSpinBox(this);
        lowSpin_->setRange(0, 255);
        lowSpin_->setValue(0);

        lowColorChk_ = new QCheckBox(this);
        lowColorChk_->setToolTip(tr("Highlight low values (yellow) in preview"));
        lowColorChk_->setChecked(false);

        row->addWidget(lowApply_);
        row->addWidget(lowSpin_);
        row->addWidget(lowColorChk_);

        mainLayout->addLayout(row);
    }

    // Wiersz 2: [apply] [spinbox high] [color checkbox]
    {
        auto* row = new QHBoxLayout;

        highApply_ = new QCheckBox(tr("apply"), this);
        highApply_->setChecked(true);

        highSpin_ = new QSpinBox(this);
        highSpin_->setRange(0, 255);
        highSpin_->setValue(255);

        highColorChk_ = new QCheckBox(this);
        highColorChk_->setToolTip(tr("Highlight high values (red) in preview"));
        highColorChk_->setChecked(false);

        row->addWidget(highApply_);
        row->addWidget(highSpin_);
        row->addWidget(highColorChk_);

        mainLayout->addLayout(row);
    }

    auto* buttons = new QDialogButtonBox(this);
    previewBtn_ = buttons->addButton(tr("Preview"), QDialogButtonBox::ActionRole);
    auto* okBtn = buttons->addButton(QDialogButtonBox::Ok);
    auto* cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);

    mainLayout->addWidget(buttons);

    // Przycisk Preview
    connect(previewBtn_, &QPushButton::clicked,
            this, &DoubleThresholdDialog::onPreviewClicked);

    // OK / Cancel jak wcześniej
    connect(okBtn, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);

    // AUTO-PREVIEW:
    // wszystkie 4 checkboxy i oba spinboxy przy zmianie wywołują preview

    if (lowSpin_) {
        connect(lowSpin_, qOverload<int>(&QSpinBox::valueChanged),
                this, &DoubleThresholdDialog::onPreviewClicked);
    }
    if (highSpin_) {
        connect(highSpin_, qOverload<int>(&QSpinBox::valueChanged),
                this, &DoubleThresholdDialog::onPreviewClicked);
    }
    if (lowApply_) {
        connect(lowApply_, &QCheckBox::toggled,
                this, &DoubleThresholdDialog::onPreviewClicked);
    }
    if (highApply_) {
        connect(highApply_, &QCheckBox::toggled,
                this, &DoubleThresholdDialog::onPreviewClicked);
    }
    if (lowColorChk_) {
        connect(lowColorChk_, &QCheckBox::toggled,
                this, &DoubleThresholdDialog::onPreviewClicked);
    }
    if (highColorChk_) {
        connect(highColorChk_, &QCheckBox::toggled,
                this, &DoubleThresholdDialog::onPreviewClicked);
    }
}

int DoubleThresholdDialog::low() const
{
    return lowSpin_ ? lowSpin_->value() : 0;
}

int DoubleThresholdDialog::high() const
{
    return highSpin_ ? highSpin_->value() : 255;
}

void DoubleThresholdDialog::onPreviewClicked()
{
    if (!lowSpin_ || !highSpin_)
        return;

    int lowVal  = lowSpin_->value();
    int highVal = highSpin_->value();

    const bool lowApply    = (lowApply_    && lowApply_->isChecked());
    const bool highApply   = (highApply_   && highApply_->isChecked());
    const bool lowColorize = (lowColorChk_ && lowColorChk_->isChecked());
    const bool highColorize= (highColorChk_&& highColorChk_->isChecked());

    // Logika "apply": gdy odznaczone – w PREVIEW traktujemy jako 0 / 255
    if (!lowApply)
        lowVal = 0;
    if (!highApply)
        highVal = 255;

    emit previewRequested(lowVal, highVal, lowColorize, highColorize);
}

bool DoubleThresholdDialog::lowApplyChecked() const {
    return lowApply_ && lowApply_->isChecked();
}

bool DoubleThresholdDialog::highApplyChecked() const {
    return highApply_ && highApply_->isChecked();
}
