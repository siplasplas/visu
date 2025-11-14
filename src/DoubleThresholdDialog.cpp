#include "DoubleThresholdDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
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

    // Wiersz 1: Low + checkbox (żółty)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(tr("Low threshold:"), this);
        lowSpin_ = new QSpinBox(this);
        lowSpin_->setRange(0, 255);
        lowSpin_->setValue(0);

        lowCheck_ = new QCheckBox(tr("Highlight low (yellow)"), this);
        lowCheck_->setChecked(false);

        row->addWidget(label);
        row->addWidget(lowSpin_);
        row->addWidget(lowCheck_);
        mainLayout->addLayout(row);
    }

    // Wiersz 2: High + checkbox (czerwony)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(tr("High threshold:"), this);
        highSpin_ = new QSpinBox(this);
        highSpin_->setRange(0, 255);
        highSpin_->setValue(255);

        highCheck_ = new QCheckBox(tr("Highlight high (red)"), this);
        highCheck_->setChecked(false);

        row->addWidget(label);
        row->addWidget(highSpin_);
        row->addWidget(highCheck_);
        mainLayout->addLayout(row);
    }

    auto* buttons = new QDialogButtonBox(this);
    previewBtn_ = buttons->addButton(tr("Preview"), QDialogButtonBox::ActionRole);
    auto* okBtn = buttons->addButton(QDialogButtonBox::Ok);
    auto* cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);

    mainLayout->addWidget(buttons);

    connect(previewBtn_, &QPushButton::clicked,
            this, &DoubleThresholdDialog::onPreviewClicked);

    connect(okBtn, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);
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
    const bool lowColorize  = lowCheck_  && lowCheck_->isChecked();
    const bool highColorize = highCheck_ && highCheck_->isChecked();
    emit previewRequested(low(), high(), lowColorize, highColorize);
}
