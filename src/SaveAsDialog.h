#ifndef SAVEASDIALOG_H
#define SAVEASDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include "ImageFormats.h"
#include "image_metrics.h"

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

    void onMetricToggled(bool checked);
private:
    QComboBox* formatCombo_;
    QSlider*   qualitySlider_;
    QSpinBox* qualitySpin_;
    QLabel*    sizeInfoLabel_;

    QVector<QCheckBox*> metricChecks_;
    QVector<MetricType> metricTypes_;
    MetricType selectedMetric_ = MetricType::None;

    QLabel*    metricResultLabel_;

    bool isLossyCurrent_ = false;

    ImageFormat formatFromComboIndex(int index) const;
};

#endif // SAVEASDIALOG_H
