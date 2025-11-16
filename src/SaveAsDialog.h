#ifndef SAVEASDIALOG_H
#define SAVEASDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include "ImageFormats.h"

class SaveAsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SaveAsDialog(QWidget* parent = nullptr);

    ImageFormat selectedFormat() const;
    int quality() const;           // dla stratnych
    bool showOriginalChecked() const;

    void setLossy(bool lossy);
    void setFileSizeInfo(qint64 bytes, double ratio);
    void clearFileSizeInfo();
    int formatToComboIndex(ImageFormat fmt) const;
    QCheckBox* showOriginalCheck_;
signals:
    void previewRequested(ImageFormat format, int quality, bool showOriginal);
    void acceptedSave(ImageFormat format, int quality, bool showOriginal);
public slots:
    void onFormatChanged(int index);
    void onQualitySliderChanged(int value);
    void onQualityEditChanged(const QString& text);
    void onShowOriginalToggled(bool);
    void onApplyClicked();
    void onSaveClicked();

private:
    QComboBox* formatCombo_;
    QSlider*   qualitySlider_;
    QLineEdit* qualityEdit_;
    QLabel*    sizeInfoLabel_;

    bool isLossyCurrent_ = false;

    ImageFormat formatFromComboIndex(int index) const;
};

#endif // SAVEASDIALOG_H
