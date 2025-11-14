#ifndef DOUBLETHRESHOLDDIALOG_H
#define DOUBLETHRESHOLDDIALOG_H

#include <QDialog>

class QSpinBox;
class QPushButton;
class QCheckBox;

class DoubleThresholdDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DoubleThresholdDialog(QWidget* parent = nullptr);

    int low() const;
    int high() const;

    signals:
        void previewRequested(int low, int high, bool lowColorize, bool highColorize);

private slots:
    void onPreviewClicked();

private:
    QSpinBox* lowSpin_ = nullptr;
    QSpinBox* highSpin_ = nullptr;
    QPushButton* previewBtn_ = nullptr;
    QCheckBox*  lowCheck_  = nullptr;
    QCheckBox*  highCheck_ = nullptr;
};

#endif // DOUBLETHRESHOLDDIALOG_H
