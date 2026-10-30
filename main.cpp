
#include <QApplication>
#include <QFont>

#include "QRGenerator.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("Urent QR Generator");
    app.setApplicationVersion("1.0");

    QFont defaultFont("Arial", 10);
    app.setFont(defaultFont);

    QRGenerator window;
    window.show();

    return app.exec();
}
