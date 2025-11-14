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

    bool lowApplyChecked() const;
    bool highApplyChecked() const;

    signals:
            // PREVIEW: przekazujemy skuteczne low/high oraz info o kolorowaniu
        void previewRequested(int low, int high,
                              bool lowColorize, bool highColorize);

private slots:
    void onPreviewClicked();

private:
    QSpinBox*   lowSpin_      = nullptr;
    QSpinBox*   highSpin_     = nullptr;

    QCheckBox*  lowApply_     = nullptr;  // "apply" dla low
    QCheckBox*  highApply_    = nullptr;  // "apply" dla high

    QCheckBox*  lowColorChk_  = nullptr;  // kolorowanie low
    QCheckBox*  highColorChk_ = nullptr;  // kolorowanie high

    QPushButton* previewBtn_  = nullptr;
};

#endif // DOUBLETHRESHOLDDIALOG_H
