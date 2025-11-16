#include "ShadowCompressionDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>

ShadowCompressionDialog::ShadowCompressionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Shadow Compression"));
    setWindowModality(Qt::NonModal);
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* mainLayout = new QVBoxLayout(this);

    // 1) T (edge threshold)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(tr("T:"), this);

        tSpin_ = new QSpinBox(this);
        tSpin_->setRange(1, 254);
        tSpin_->setValue(242); // domyślne, można zmienić

        fromHistBtn_ = new QPushButton(tr("T from histogram"), this);

        row->addWidget(label);
        row->addWidget(tSpin_);
        row->addWidget(fromHistBtn_);

        mainLayout->addLayout(row);
    }

    // 2) Gamma
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(tr("Gamma:"), this);

        gammaSpin_ = new QDoubleSpinBox(this);
        gammaSpin_->setDecimals(2);
        gammaSpin_->setRange(0.10, 5.0);
        gammaSpin_->setSingleStep(0.10);
        gammaSpin_->setValue(2.0);   // sensowny start

        row->addWidget(label);
        row->addWidget(gammaSpin_);

        mainLayout->addLayout(row);
    }

    // 3) MaxOut (max jasność obrysu po przekształceniu)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(tr("Max outline brightness:"), this);

        maxOutSpin_ = new QSpinBox(this);
        maxOutSpin_->setRange(1, 254);
        maxOutSpin_->setValue(200);  // obrys ciemniejszy niż tło, ale wyraźny

        row->addWidget(label);
        row->addWidget(maxOutSpin_);

        mainLayout->addLayout(row);
    }

    auto* buttons = new QDialogButtonBox(this);
    previewBtn_ = buttons->addButton(tr("Preview"), QDialogButtonBox::ActionRole);
    auto* okBtn = buttons->addButton(QDialogButtonBox::Ok);
    auto* cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);

    mainLayout->addWidget(buttons);

    // Preview button
    connect(previewBtn_, &QPushButton::clicked,
            this, &ShadowCompressionDialog::onPreviewClicked);

    // OK / Cancel
    connect(okBtn, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);

    // T from histogram
    connect(fromHistBtn_, &QPushButton::clicked,
            this, [this]() {
                emit requestTFromHistogram();
            });

    // Auto-preview przy zmianach spinów
    connect(tSpin_, qOverload<int>(&QSpinBox::valueChanged),
            this, &ShadowCompressionDialog::onPreviewClicked);
    connect(maxOutSpin_, qOverload<int>(&QSpinBox::valueChanged),
            this, &ShadowCompressionDialog::onPreviewClicked);
    connect(gammaSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &ShadowCompressionDialog::onPreviewClicked);
}

int ShadowCompressionDialog::tValue() const
{
    return tSpin_ ? tSpin_->value() : 242;
}

double ShadowCompressionDialog::gammaValue() const
{
    return gammaSpin_ ? gammaSpin_->value() : 2.0;
}

int ShadowCompressionDialog::maxOutValue() const
{
    return maxOutSpin_ ? maxOutSpin_->value() : 200;
}

void ShadowCompressionDialog::setT(int t)
{
    if (!tSpin_)
        return;
    tSpin_->setValue(t);
}

void ShadowCompressionDialog::onPreviewClicked()
{
    emit previewRequested(tValue(), gammaValue(), maxOutValue());
}
