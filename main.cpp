// main.cpp
#include <QApplication>
#include "QRGenerator.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Установка шрифтов
    QFont defaultFont("Arial", 10);
    app.setFont(defaultFont);
    
    QRGenerator window;
    window.show();
    
    return app.exec();
}
