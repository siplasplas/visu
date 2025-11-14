#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString startFilePath;
    if (argc > 1) {
        startFilePath = QString::fromLocal8Bit(argv[1]);
    }

    MainWindow w(startFilePath);
    w.show();

    return app.exec();
}
