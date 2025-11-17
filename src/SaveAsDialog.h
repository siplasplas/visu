#ifndef SAVEASDIALOG_H
#define SAVEASDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include "ImageFormats.h"

enum class MetricType {
    None,
    PSNR,
    SSIM,
    MS_SSIM
};

class QSpinBox;

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

    MetricType selectedMetric() const;
    void setMetricInfo(const QString& text);
    void clearMetricInfo();
protected:
    void closeEvent(QCloseEvent *e) override;
signals:
    void previewRequested(ImageFormat format, int quality, bool showOriginal);
    void acceptedSave(ImageFormat format, int quality, bool showOriginal);
    void dialogClosed();
public slots:
    void onFormatChanged(int index);
    void onQualitySliderChanged(int value);
    void onQualitySpinChanged(int value);
    void onShowOriginalToggled(bool);
    void onApplyClicked();
    void onSaveClicked();

    void onMetricPsnrToggled(bool checked);
    void onMetricSsimToggled(bool checked);
    void onMetricMsSsimToggled(bool checked);
private:
    QComboBox* formatCombo_;
    QSlider*   qualitySlider_;
    QSpinBox* qualitySpin_;
    QLabel*    sizeInfoLabel_;

    QCheckBox* metricPsnrCheck_;
    QCheckBox* metricSsimCheck_;
    QCheckBox* metricMsSsimCheck_;
    QLabel*    metricResultLabel_;

    bool isLossyCurrent_ = false;

    ImageFormat formatFromComboIndex(int index) const;
    MetricType selectedMetric_ = MetricType::None;
};

#endif // SAVEASDIALOG_H
