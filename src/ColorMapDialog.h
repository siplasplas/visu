#ifndef COLORMAPDIALOG_H
#define COLORMAPDIALOG_H

#include <QDialog>
#include <opencv2/core.hpp>

class QLineEdit;
class QPushButton;
class QLabel;

class ColorMapDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ColorMapDialog(bool isGrayscale, QWidget* parent = nullptr);

    QString sourceText() const;
    QString destText() const;

signals:
    void previewRequested(const cv::Vec3b& srcColor, const cv::Vec3b& dstColor);

public slots:
    void onPreviewClicked();

private:
    bool parseColor(const QString& text, cv::Vec3b& color) const;
    void updateColorPreview(QLabel* label, const cv::Vec3b& color);

    bool isGrayscale_ = false;
    QLineEdit* sourceEdit_ = nullptr;
    QLineEdit* destEdit_ = nullptr;
    QLabel* sourcePreview_ = nullptr;
    QLabel* destPreview_ = nullptr;
    QPushButton* previewBtn_ = nullptr;
};

#endif // COLORMAPDIALOG_H