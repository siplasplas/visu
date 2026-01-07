#include "ColorMapDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QRegularExpression>

ColorMapDialog::ColorMapDialog(bool isGrayscale, QWidget* parent)
    : QDialog(parent)
    , isGrayscale_(isGrayscale)
{
    setWindowTitle(tr("Color Map"));
    setWindowModality(Qt::NonModal);
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* mainLayout = new QVBoxLayout(this);

    QString placeholder = isGrayscale_ ? tr("0-255") : tr("R, G, B");
    QString hint = isGrayscale_
        ? tr("Enter grayscale value (0-255)")
        : tr("Enter RGB values separated by commas (e.g., 200, 123, 162)");

    // Hint label
    auto* hintLabel = new QLabel(hint, this);
    hintLabel->setStyleSheet("color: gray; font-size: 10px;");
    mainLayout->addWidget(hintLabel);

    // Source color row
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(tr("Source:"), this);
        label->setMinimumWidth(60);

        sourceEdit_ = new QLineEdit(this);
        sourceEdit_->setPlaceholderText(placeholder);
        sourceEdit_->setMinimumWidth(120);

        sourcePreview_ = new QLabel(this);
        sourcePreview_->setFixedSize(24, 24);
        sourcePreview_->setStyleSheet("border: 1px solid black; background-color: white;");

        row->addWidget(label);
        row->addWidget(sourceEdit_);
        row->addWidget(sourcePreview_);
        row->addStretch();

        mainLayout->addLayout(row);
    }

    // Destination color row
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(tr("Destination:"), this);
        label->setMinimumWidth(60);

        destEdit_ = new QLineEdit(this);
        destEdit_->setPlaceholderText(placeholder);
        destEdit_->setMinimumWidth(120);

        destPreview_ = new QLabel(this);
        destPreview_->setFixedSize(24, 24);
        destPreview_->setStyleSheet("border: 1px solid black; background-color: white;");

        row->addWidget(label);
        row->addWidget(destEdit_);
        row->addWidget(destPreview_);
        row->addStretch();

        mainLayout->addLayout(row);
    }

    // Buttons
    auto* buttons = new QDialogButtonBox(this);
    previewBtn_ = buttons->addButton(tr("Preview"), QDialogButtonBox::ActionRole);
    auto* okBtn = buttons->addButton(QDialogButtonBox::Ok);
    auto* cancelBtn = buttons->addButton(QDialogButtonBox::Cancel);

    mainLayout->addWidget(buttons);

    // Connections
    connect(previewBtn_, &QPushButton::clicked,
            this, &ColorMapDialog::onPreviewClicked);

    connect(okBtn, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);

    // Update color preview when text changes
    connect(sourceEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        cv::Vec3b color;
        if (parseColor(text, color)) {
            updateColorPreview(sourcePreview_, color);
        }
    });

    connect(destEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        cv::Vec3b color;
        if (parseColor(text, color)) {
            updateColorPreview(destPreview_, color);
        }
    });
}

QString ColorMapDialog::sourceText() const
{
    return sourceEdit_ ? sourceEdit_->text() : QString();
}

QString ColorMapDialog::destText() const
{
    return destEdit_ ? destEdit_->text() : QString();
}

bool ColorMapDialog::parseColor(const QString& text, cv::Vec3b& color) const
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    if (isGrayscale_) {
        bool ok = false;
        int val = trimmed.toInt(&ok);
        if (ok && val >= 0 && val <= 255) {
            color = cv::Vec3b(val, val, val);
            return true;
        }
        return false;
    }

    // Color: expect "R, G, B" or "R,G,B"
    static QRegularExpression re(R"(^\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*$)");
    auto match = re.match(trimmed);
    if (!match.hasMatch())
        return false;

    bool okR, okG, okB;
    int r = match.captured(1).toInt(&okR);
    int g = match.captured(2).toInt(&okG);
    int b = match.captured(3).toInt(&okB);

    if (!okR || !okG || !okB)
        return false;
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
        return false;

    // OpenCV uses BGR order
    color = cv::Vec3b(b, g, r);
    return true;
}

void ColorMapDialog::updateColorPreview(QLabel* label, const cv::Vec3b& color)
{
    // color is in BGR order
    QString style = QString("border: 1px solid black; background-color: rgb(%1, %2, %3);")
        .arg(color[2])  // R
        .arg(color[1])  // G
        .arg(color[0]); // B
    label->setStyleSheet(style);
}

void ColorMapDialog::onPreviewClicked()
{
    cv::Vec3b srcColor, dstColor;
    if (!parseColor(sourceText(), srcColor))
        return;
    if (!parseColor(destText(), dstColor))
        return;

    emit previewRequested(srcColor, dstColor);
}