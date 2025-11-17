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
    void metricSelectionChanged();
    void formatChanged();
public slots:
    void onFormatChanged(int index);

    void onQualityChanged(int q);

    void onQualitySliderChanged(int value);
    void onQualitySpinChanged(int value);
    void onShowOriginalToggled(bool);
    void onApplyClicked();
    void onSaveClicked();

    void onMetricToggled(bool checked);

    void onQualityEditChanged(const QString &text);

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
    ImageFormat selectedFormat_;   // current format (e.g., Jpeg, Png, Avif)
    int         currentQuality_;   // current quality (0–100 or according to your scale)

    ImageFormat formatFromComboIndex(int index) const;
};

inline ImageFormat SaveAsDialog::selectedFormat() const
{
    return selectedFormat_;
}

#endif // SAVEASDIALOG_H
