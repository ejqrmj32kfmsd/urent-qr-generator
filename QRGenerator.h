// QRGenerator.h
#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QPainter>
#include <QImage>
#include <QPrinter>
#include <QFileDialog>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QRegularExpression>
#include <vector>

class QRGenerator : public QMainWindow {
    Q_OBJECT

public:
    QRGenerator(QWidget *parent = nullptr);
    ~QRGenerator();

private slots:
    void updatePreview();
    void printCodes();
    void saveToPDF();
    void saveToWord();
    void saveToPNG();

private:
    // Генерация QR кода
    QImage generateQRCodeImage(const QString &text, int size = 200);
    
    // Генерация полного изображения самоката
    QImage generateScooterImage(const QString &scooterNumber);
    
    // Парсинг номеров самокатов
    std::vector<QString> parseScooterNumbers(const QString &input);
    
    // Валидация формата
    bool isValidScooterNumber(const QString &number);
    QString normalizeScooterNumber(const QString &number);
    
    // Рисование на принтере/документе
    void drawScooterOnPainter(QPainter &painter, const QString &scooterNumber, 
                              int x, int y, int sizeMm = 23);

    bool drawCodesOnPrinter(QPrinter &printer,
                            const std::vector<QString> &numbers);
    
    // UI элементы
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    
    // Левая панель
    QWidget *leftPanel;
    QVBoxLayout *leftLayout;
    QLabel *inputLabel;
    QTextEdit *inputText;
    
    // Кнопки
    QWidget *buttonPanel;
    QVBoxLayout *buttonLayout;
    QPushButton *printButton;
    QPushButton *savePDFButton;
    QPushButton *saveWordButton;
    QPushButton *savePNGButton;
    
    // Правая панель с предпросмотром
    QWidget *rightPanel;
    QVBoxLayout *rightLayout;
    QLabel *previewLabel;
    QLabel *previewTitle;
    
    // Текущий предпросмотр
    QImage currentPreview;
    
    // Константы
    static constexpr double CODE_SIZE_MM = 23.0; // 2.3 см
    static constexpr double SPACING_MM = 10.0;   // 1 см между кодами
    static constexpr int DPI = 300;              // Разрешение для печати
};
