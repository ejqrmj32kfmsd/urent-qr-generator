// QRGenerator.cpp
#include "QRGenerator.h"
#include <qrencode.h>
#include <QApplication>
#include <QDesktopWidget>
#include <QTextDocument>
#include <QTextStream>

QRGenerator::QRGenerator(QWidget *parent)
    : QMainWindow(parent) {
    
    setWindowTitle("ЮРЕНТ Генератор QR кодов");
    setMinimumSize(1200, 700);
    
    // Фирменные цвета Юрента (фиолетовый)
    setStyleSheet(R"(
        QMainWindow {
            background-color: #B866FF;
        }
        QWidget {
            background-color: #B866FF;
        }
        QLabel {
            color: #2D1B4E;
            font-family: 'Arial', sans-serif;
        }
        QTextEdit {
            background-color: white;
            border: 2px solid #2D1B4E;
            border-radius: 5px;
            font-size: 14px;
            padding: 5px;
        }
        QPushButton {
            background-color: white;
            color: #2D1B4E;
            border: 2px solid #2D1B4E;
            border-radius: 5px;
            padding: 10px;
            font-size: 14px;
            font-weight: bold;
            min-width: 150px;
        }
        QPushButton:hover {
            background-color: #2D1B4E;
            color: white;
        }
        QPushButton:pressed {
            background-color: #1a0f33;
        }
    )");
    
    centralWidget = new QWidget();
    setCentralWidget(centralWidget);
    
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);
    
    // Заголовок
    QLabel *titleLabel = new QLabel("ЮРЕНТ ГЕНЕРАТОР QR КОДОВ");
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #2D1B4E;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // Горизонтальный布局
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(30);
    
    // === ЛЕВАЯ ПАНЕЛЬ ===
    leftPanel = new QWidget();
    leftPanel->setMaximumWidth(400);
    leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    inputLabel = new QLabel("Введите список самокатов\nв формате 000-000");
    inputLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    leftLayout->addWidget(inputLabel);
    
    inputText = new QTextEdit();
    inputText->setPlaceholderText("Пример:\n137-539\n138-001\n138-002");
    inputText->setMinimumHeight(400);
    connect(inputText, &QTextEdit::textChanged, this, &QRGenerator::updatePreview);
    leftLayout->addWidget(inputText);
    
    // Панель кнопок
    buttonPanel = new QWidget();
    buttonLayout = new QVBoxLayout(buttonPanel);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(10);
    
    printButton = new QPushButton("ПЕЧАТЬ");
    connect(printButton, &QPushButton::clicked, this, &QRGenerator::printCodes);
    buttonLayout->addWidget(printButton);
    
    savePDFButton = new QPushButton("СОХРАНИТЬ В PDF");
    connect(savePDFButton, &QPushButton::clicked, this, &QRGenerator::saveToPDF);
    buttonLayout->addWidget(savePDFButton);
    
    saveWordButton = new QPushButton("СОХРАНИТЬ В WORD");
    connect(saveWordButton, &QPushButton::clicked, this, &QRGenerator::saveToWord);
    buttonLayout->addWidget(saveWordButton);
    
    savePNGButton = new QPushButton("СОХРАНИТЬ В PNG");
    connect(savePNGButton, &QPushButton::clicked, this, &QRGenerator::saveToPNG);
    buttonLayout->addWidget(savePNGButton);
    
    leftLayout->addWidget(buttonPanel);
    contentLayout->addWidget(leftPanel);
    
    // === ПРАВАЯ ПАНЕЛЬ (ПРЕДПРОСМОТР) ===
    rightPanel = new QWidget();
    rightPanel->setStyleSheet("background-color: rgba(255, 255, 255, 0.3); border-radius: 10px;");
    rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(30, 30, 30, 30);
    
    previewTitle = new QLabel("ПРЕДПРОСМОТР");
    previewTitle->setStyleSheet("font-size: 20px; font-weight: bold; color: #2D1B4E;");
    previewTitle->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(previewTitle);
    
    previewLabel = new QLabel();
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumSize(300, 300);
    previewLabel->setStyleSheet("background-color: white; border: 2px solid #2D1B4E; border-radius: 5px;");
    rightLayout->addWidget(previewLabel);
    
    contentLayout->addWidget(rightPanel);
    mainLayout->addLayout(contentLayout);
    
    // Инициализация пустым предпросмотром
    updatePreview();
}

QRGenerator::~QRGenerator() {}

QImage QRGenerator::generateQRCodeImage(const QString &text, int size) {
    QRcode *qrcode = QRcode_encodeString(text.toUtf8().constData(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    
    if (!qrcode) {
        return QImage();
    }
    
    int width = qrcode->width > 0 ? qrcode->width : 1;
    double scale = size / static_cast<double>(width);
    
    QImage image(size, size, QImage::Format_Mono);
    image.fill(1);
    
    QPainter painter(&image);
    painter.setPen(Qt::black);
    
    for (int y = 0; y < width; y++) {
        for (int x = 0; x < width; x++) {
            if (qrcode->data[y * width + x] & 0x01) {
                painter.fillRect(x * scale, y * scale, scale, scale, Qt::black);
            }
        }
    }
    
    QRcode_free(qrcode);
    return image;
}

QImage QRGenerator::generateScooterImage(const QString &scooterNumber) {
    // Размер 2.3 см x 2.3 см при 300 DPI
    int sizePx = static_cast<int>(CODE_SIZE_MM * DPI / 25.4);
    
    QImage image(sizePx, sizePx, QImage::Format_ARGB32);
    image.fill(Qt::white);
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int topMargin = sizePx * 0.05;
    int qrSize = sizePx * 0.55;
    int bottomMargin = sizePx * 0.1;
    
    // Надпись "ЮРЕНТ"
    QFont urentFont("Arial", sizePx / 8, QFont::Bold);
    painter.setFont(urentFont);
    painter.setPen(QColor("#2D1B4E"));
    QRectF textRect(0, topMargin, sizePx, sizePx * 0.15);
    painter.drawText(textRect, Qt::AlignCenter, "ЮРЕНТ");
    
    // QR код
    QImage qrImage = generateQRCodeImage(scooterNumber, qrSize);
    int qrX = (sizePx - qrSize) / 2;
    int qrY = topMargin + sizePx * 0.15 + (sizePx * 0.55 - qrSize) / 2;
    painter.drawImage(qrX, qrY, qrImage);
    
    // Номер самоката
    QFont numberFont("Arial", sizePx / 10, QFont::Bold);
    painter.setFont(numberFont);
    QRectF numberRect(0, sizePx - bottomMargin - sizePx * 0.15, sizePx, sizePx * 0.15);
    painter.drawText(numberRect, Qt::AlignCenter, scooterNumber);
    
    return image;
}

std::vector<QString> QRGenerator::parseScooterNumbers(const QString &input) {
    std::vector<QString> numbers;
    QStringList lines = input.split(QRegularExpression("[\n,;]+"), Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        
        // Удаляем все нецифровые символы кроме дефиса
        QString cleaned;
        for (QChar c : trimmed) {
            if (c.isDigit() || c == '-') {
                cleaned.append(c);
            }
        }
        
        if (isValidScooterNumber(cleaned)) {
            numbers.push_back(normalizeScooterNumber(cleaned));
        }
    }
    
    return numbers;
}

bool QRGenerator::isValidScooterNumber(const QString &number) {
    // Проверяем формат XXX-XXX или XXXXXX
    QRegularExpression re1("^\\d{3}-\\d{3}$");
    QRegularExpression re2("^\\d{6}$");
    
    return re1.match(number).hasMatch() || re2.match(number).hasMatch();
}

QString QRGenerator::normalizeScooterNumber(const QString &number) {
    if (number.length() == 6) {
        return number.left(3) + "-" + number.right(3);
    }
    return number;
}

void QRGenerator::updatePreview() {
    QString input = inputText->toPlainText();
    auto numbers = parseScooterNumbers(input);
    
    if (!numbers.empty()) {
        currentPreview = generateScooterImage(numbers[0]);
        QPixmap pixmap = QPixmap::fromImage(currentPreview.scaled(250, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        previewLabel->setPixmap(pixmap);
    } else {
        currentPreview = QImage();
        previewLabel->clear();
        previewLabel->setText("Нет данных для отображения");
    }
}

void QRGenerator::drawScooterOnPainter(QPainter &painter, const QString &scooterNumber, 
                                       int x, int y, int sizeMm) {
    int sizePx = static_cast<int>(sizeMm * DPI / 25.4);
    QImage image = generateScooterImage(scooterNumber);
    
    painter.drawImage(x, y, image.scaled(sizePx, sizePx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void QRGenerator::printCodes() {
    QString input = inputText->toPlainText();
    auto numbers = parseScooterNumbers(input);
    
    if (numbers.empty()) {
        QMessageBox::warning(this, "Ошибка", "Введите хотя бы один номер самоката");
        return;
    }
    
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize::A4);
    printer.setOrientation(QPageLayout::Portrait);
    printer.setFullPage(true);
    printer.setColorMode(QPrinter::Color);
    
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    QPainter painter(&printer);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int pageWidth = printer.pageRect(QPageLayout::Millimeter).width();
    int pageHeight = printer.pageRect(QPageLayout::Millimeter).height();
    
    int codeSizeMm = static_cast<int>(CODE_SIZE_MM);
    int spacingMm = static_cast<int>(SPACING_MM);
    int totalWidth = codeSizeMm + spacingMm;
    int totalHeight = codeSizeMm + spacingMm;
    
    int codesPerRow = pageWidth / totalWidth;
    int codesPerCol = pageHeight / totalHeight;
    
    if (codesPerRow < 1) codesPerRow = 1;
    if (codesPerCol < 1) codesPerCol = 1;
    
    int index = 0;
    for (const auto &number : numbers) {
        int row = index / codesPerRow;
        int col = index % codesPerRow;
        
        if (row >= codesPerCol) {
            printer.newPage();
            row = 0;
            index = 0;
            col = index % codesPerRow;
        }
        
        int xMm = col * totalWidth + spacingMm / 2;
        int yMm = row * totalHeight + spacingMm / 2;
        
        int xPx = printer.resolution() * xMm / 25.4;
        int yPx = printer.resolution() * yMm / 25.4;
        
        drawScooterOnPainter(painter, number, xPx, yPx, codeSizeMm);
        
        index++;
    }
    
    painter.end();
}

void QRGenerator::saveToPDF() {
    QString input = inputText->toPlainText();
    auto numbers = parseScooterNumbers(input);
    
    if (numbers.empty()) {
        QMessageBox::warning(this, "Ошибка", "Введите хотя бы один номер самоката");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить PDF", 
                                                     "scooters.pdf", "PDF Files (*.pdf)");
    if (fileName.isEmpty()) return;
    
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize::A4);
    printer.setOrientation(QPageLayout::Portrait);
    printer.setFullPage(true);
    
    QPainter painter(&printer);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int pageWidth = printer.pageRect(QPageLayout::Millimeter).width();
    int pageHeight = printer.pageRect(QPageLayout::Millimeter).height();
    
    int codeSizeMm = static_cast<int>(CODE_SIZE_MM);
    int spacingMm = static_cast<int>(SPACING_MM);
    int totalWidth = codeSizeMm + spacingMm;
    int totalHeight = codeSizeMm + spacingMm;
    
    int codesPerRow = pageWidth / totalWidth;
    int codesPerCol = pageHeight / totalHeight;
    
    if (codesPerRow < 1) codesPerRow = 1;
    if (codesPerCol < 1) codesPerCol = 1;
    
    int index = 0;
    for (const auto &number : numbers) {
        int row = index / codesPerRow;
        int col = index % codesPerRow;
        
        if (row >= codesPerCol) {
            printer.newPage();
            row = 0;
            index = 0;
            col = index % codesPerRow;
        }
        
        int xMm = col * totalWidth + spacingMm / 2;
        int yMm = row * totalHeight + spacingMm / 2;
        
        int xPx = printer.resolution() * xMm / 25.4;
        int yPx = printer.resolution() * yMm / 25.4;
        
        drawScooterOnPainter(painter, number, xPx, yPx, codeSizeMm);
        
        index++;
    }
    
    painter.end();
    QMessageBox::information(this, "Успех", "PDF файл успешно сохранен");
}

void QRGenerator::saveToWord() {
    QString input = inputText->toPlainText();
    auto numbers = parseScooterNumbers(input);
    
    if (numbers.empty()) {
        QMessageBox::warning(this, "Ошибка", "Введите хотя бы один номер самоката");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить Word", 
                                                     "scooters.doc", "Word Files (*.doc)");
    if (fileName.isEmpty()) return;
    
    // Создаем RTF документ (совместим с Word)
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось создать файл");
        return;
    }
    
    QTextStream out(&file);
    out.setCodec("UTF-8");
    
    // RTF заголовок
    out << "{\\rtf1\\ansi\\deff0\n";
    out << "{\\fonttbl{\\f0 Arial;}}\n";
    out << "\\paperw12240\\paperh15840\n"; // A4 размеры в twips
    out << "\\margl1440\\margr1440\\margt1440\\margb1440\n";
    
    int codeSizeMm = static_cast<int>(CODE_SIZE_MM);
    int spacingMm = static_cast<int>(SPACING_MM);
    
    // Генерируем изображения и вставляем в документ
    int col = 0;
    for (const auto &number : numbers) {
        if (col > 0) {
            out << "\\tab ";
        }
        
        QImage image = generateScooterImage(number);
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        
        QString base64 = byteArray.toBase64();
        
        // Вставляем изображение как RTF
        out << "{\\pict\\pngblip\\picw" << image.width() 
            << "\\pich" << image.height() << " ";
        out << base64 << "}\n";
        
        col++;
        if (col >= 3) { // 3 колонки
            col = 0;
            out << "\\par\\par ";
        }
    }
    
    out << "}";
    file.close();
    
    QMessageBox::information(this, "Успех", "Word документ успешно сохранен");
}

void QRGenerator::saveToPNG() {
    QString input = inputText->toPlainText();
    auto numbers = parseScooterNumbers(input);
    
    if (numbers.empty()) {
        QMessageBox::warning(this, "Ошибка", "Введите хотя бы один номер самоката");
        return;
    }
    
    if (numbers.size() == 1) {
        // Один файл
        QString fileName = QFileDialog::getSaveFileName(this, "Сохранить PNG", 
                                                         numbers[0] + ".png", "PNG Files (*.png)");
        if (fileName.isEmpty()) return;
        
        QImage image = generateScooterImage(numbers[0]);
        if (image.save(fileName, "PNG", 100)) {
            QMessageBox::information(this, "Успех", "Изображение успешно сохранено");
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось сохранить файл");
        }
    } else {
        // Множественные файлы - создаем папку
        QString dirName = QFileDialog::getSaveFileName(this, "Выберите папку для сохранения", 
                                                        "scooters_qr", "");
        if (dirName.isEmpty()) return;
        
        QDir dir(dirName);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        int savedCount = 0;
        for (size_t i = 0; i < numbers.size(); i++) {
            QString fileName = dir.filePath(numbers[i] + ".png");
            QImage image = generateScooterImage(numbers[i]);
            
            if (image.save(fileName, "PNG", 100)) {
                savedCount++;
            }
        }
        
        QMessageBox::information(this, "Успех", 
                                 QString("Сохранено %1 из %2 файлов в папку:\n%3")
                                     .arg(savedCount).arg(numbers.size()).arg(dirName));
    }
}
