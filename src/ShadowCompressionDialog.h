#ifndef SHADOWCOMPRESSIONDIALOG_H
#define SHADOWCOMPRESSIONDIALOG_H

#include <QDialog>

class QSpinBox;
class QDoubleSpinBox;
class QPushButton;

class ShadowCompressionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ShadowCompressionDialog(QWidget* parent = nullptr);

    int tValue() const;
    double gammaValue() const;
    int maxOutValue() const;

    void setT(int t);      // dla przycisku "T from histogram"

    signals:
        void previewRequested(int T, double gamma, int maxOut);
    void requestTFromHistogram();          // kliknięcie przycisku "T from histogram"

public slots:
    void onPreviewClicked();

private:
    QSpinBox*       tSpin_      = nullptr;    // T (0..254)
    QDoubleSpinBox* gammaSpin_  = nullptr;    // gamma
    QSpinBox*       maxOutSpin_ = nullptr;    // maxOut
    QPushButton*    previewBtn_ = nullptr;
    QPushButton*    fromHistBtn_= nullptr;
};

#endif // SHADOWCOMPRESSIONDIALOG_H
