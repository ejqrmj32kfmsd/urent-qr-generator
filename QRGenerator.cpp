
#include "QRGenerator.h"
#include "qrcodegen.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPageSize>
#include <QPrintDialog>
#include <QPixmap>
#include <QScrollArea>

#include <algorithm>
#include <cmath>
#include <exception>

namespace {

QString escapeRtfText(const QString &text)
{
    QString result;

    for (QChar ch : text) {
        const ushort code = ch.unicode();

        if (ch == '\\' || ch == '{' || ch == '}') {
            result.append('\\');
            result.append(ch);
        }
        else if (code == '\n') {
            result += "\\line ";
        }
        else if (code == '\r') {
            continue;
        }
        else if (code >= 32 && code <= 126) {
            result.append(ch);
        }
        else {
            const qint16 signedCode = static_cast<qint16>(code);
            result += "\\u" + QString::number(signedCode) + "?";
        }
    }

    return result;
}

QByteArray imageToPng(const QImage &image)
{
    QByteArray data;
    QBuffer buffer(&data);

    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return {};

    return data;
}

QString bytesToHex(const QByteArray &data)
{
    static const char digits[] = "0123456789abcdef";

    QByteArray hex;
    hex.reserve(data.size() * 2);

    for (char value : data) {
        const unsigned char byte = static_cast<unsigned char>(value);
        hex.append(digits[byte >> 4]);
        hex.append(digits[byte & 0x0F]);
    }

    return QString::fromLatin1(hex);
}

} // namespace


QRGenerator::QRGenerator(QWidget *parent)
    : QMainWindow(parent),
      centralWidget(nullptr),
      mainLayout(nullptr),
      leftPanel(nullptr),
      leftLayout(nullptr),
      inputLabel(nullptr),
      inputText(nullptr),
      buttonPanel(nullptr),
      buttonLayout(nullptr),
      printButton(nullptr),
      savePDFButton(nullptr),
      saveWordButton(nullptr),
      savePNGButton(nullptr),
      rightPanel(nullptr),
      rightLayout(nullptr),
      previewLabel(nullptr),
      previewTitle(nullptr)
{
    setWindowTitle("Urent QR Generator");
    setMinimumSize(850, 600);
    resize(1100, 750);

    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    QHBoxLayout *contentLayout = new QHBoxLayout();

    // Левая панель
    leftPanel = new QWidget(centralWidget);
    leftPanel->setMinimumWidth(260);
    leftPanel->setMaximumWidth(360);

    leftLayout = new QVBoxLayout(leftPanel);

    inputLabel = new QLabel("Номера самокатов:", leftPanel);

    inputText = new QTextEdit(leftPanel);
    inputText->setAcceptRichText(false);
    inputText->setPlaceholderText(
        "Введи номера самокатов.\n"
        "Каждый номер с новой строки.\n\n"
        "Например:\n"
        "UR-10001\n"
        "UR-10002\n"
        "UR-10003"
    );

    leftLayout->addWidget(inputLabel);
    leftLayout->addWidget(inputText, 1);

    // Кнопки
    buttonPanel = new QWidget(leftPanel);
    buttonLayout = new QVBoxLayout(buttonPanel);

    printButton = new QPushButton("Печать", buttonPanel);
    savePDFButton = new QPushButton("Сохранить в PDF", buttonPanel);
    saveWordButton = new QPushButton("Сохранить для Word", buttonPanel);
    savePNGButton = new QPushButton("Сохранить в PNG", buttonPanel);

    buttonLayout->addWidget(printButton);
    buttonLayout->addWidget(savePDFButton);
    buttonLayout->addWidget(saveWordButton);
    buttonLayout->addWidget(savePNGButton);

    leftLayout->addWidget(buttonPanel);

    // Правая панель
    rightPanel = new QWidget(centralWidget);
    rightLayout = new QVBoxLayout(rightPanel);

    previewTitle = new QLabel("Предпросмотр", rightPanel);
    previewTitle->setAlignment(Qt::AlignCenter);

    previewLabel = new QLabel(rightPanel);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumSize(300, 350);
    previewLabel->setText("Здесь появятся QR-коды.");

    QScrollArea *scrollArea = new QScrollArea(rightPanel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(previewLabel);

    rightLayout->addWidget(previewTitle);
    rightLayout->addWidget(scrollArea, 1);

    contentLayout->addWidget(leftPanel, 1);
    contentLayout->addWidget(rightPanel, 2);

    mainLayout->addLayout(contentLayout, 1);

    // Сигналы кнопок
    connect(inputText, &QTextEdit::textChanged,
            this, &QRGenerator::updatePreview);

    connect(printButton, &QPushButton::clicked,
            this, &QRGenerator::printCodes);

    connect(savePDFButton, &QPushButton::clicked,
            this, &QRGenerator::saveToPDF);

    connect(saveWordButton, &QPushButton::clicked,
            this, &QRGenerator::saveToWord);

    connect(savePNGButton, &QPushButton::clicked,
            this, &QRGenerator::saveToPNG);

    updatePreview();
}


QRGenerator::~QRGenerator() = default;


// Генерация QR-кода
QImage QRGenerator::generateQRCodeImage(const QString &text, int size)
{
    if (text.isEmpty() || size < 32)
        return QImage();

    QImage image(size, size, QImage::Format_Grayscale8);
    image.fill(255);

    try {
        const QByteArray utf8 = text.toUtf8();

        const qrcodegen::QrCode qr =
            qrcodegen::QrCode::encodeText(
                utf8.constData(),
                qrcodegen::QrCode::Ecc::MEDIUM
            );

        const int moduleCount = qr.getSize();
        const int quietZone = 4;
        const int totalModules = moduleCount + quietZone * 2;

        auto pixelPosition = [size, totalModules](int position) {
            return static_cast<int>(
                std::llround(
                    static_cast<double>(position) * size / totalModules
                )
            );
        };

        for (int y = 0; y < moduleCount; ++y) {
            for (int x = 0; x < moduleCount; ++x) {

                if (!qr.getModule(x, y))
                    continue;

                const int x0 = pixelPosition(x + quietZone);
                const int x1 = pixelPosition(x + quietZone + 1);

                const int y0 = pixelPosition(y + quietZone);
                const int y1 = pixelPosition(y + quietZone + 1);

                for (int yy = y0; yy < y1; ++yy) {
                    uchar *line = image.scanLine(yy);

                    for (int xx = x0; xx < x1; ++xx)
                        line[xx] = 0;
                }
            }
        }
    }
    catch (const std::exception &) {
        return QImage();
    }

    return image;
}


// Генерация изображения одного самоката
QImage QRGenerator::generateScooterImage(const QString &scooterNumber)
{
    const int width = 300;
    const int height = 350;
    const int qrSize = 220;

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::black);

    QFont titleFont("Arial");
    titleFont.setPixelSize(17);
    titleFont.setBold(true);

    painter.setFont(titleFont);
    painter.drawText(
        QRect(5, 5, width - 10, 35),
        Qt::AlignCenter,
        "QR-КОД САМОКАТА"
    );

    const QImage qr = generateQRCodeImage(scooterNumber, qrSize);

    painter.drawRect(QRect(37, 47, qrSize + 6, qrSize + 6));
    painter.drawImage(QPoint(40, 50), qr);

    QFont numberFont("Arial");
    numberFont.setPixelSize(19);
    numberFont.setBold(true);

    painter.setFont(numberFont);

    painter.drawText(
        QRect(5, 285, width - 10, 50),
        Qt::AlignCenter,
        scooterNumber
    );

    painter.end();

    return image;
}


// Нормализация номера
QString QRGenerator::normalizeScooterNumber(const QString &number)
{
    return number.trimmed().toUpper();
}


// Проверка номера
bool QRGenerator::isValidScooterNumber(const QString &number)
{
    static const QRegularExpression pattern(
        "^[\\p{L}\\p{N}][\\p{L}\\p{N}_-]{0,63}$"
    );

    return pattern.match(number).hasMatch();
}


// Парсинг номеров самокатов
std::vector<QString> QRGenerator::parseScooterNumbers(const QString &input)
{
    std::vector<QString> result;

    const QStringList parts = input.split(
        QRegularExpression("[\\s,;]+"),
        Qt::SkipEmptyParts
    );

    for (const QString &part : parts) {
        const QString number = normalizeScooterNumber(part);

        if (!isValidScooterNumber(number))
            continue;

        if (std::find(result.begin(), result.end(), number) == result.end())
            result.push_back(number);
    }

    return result;
}


// Обновление предпросмотра
void QRGenerator::updatePreview()
{
    const std::vector<QString> numbers =
        parseScooterNumbers(inputText->toPlainText());

    const bool hasCodes = !numbers.empty();

    printButton->setEnabled(hasCodes);
    savePDFButton->setEnabled(hasCodes);
    saveWordButton->setEnabled(hasCodes);
    savePNGButton->setEnabled(hasCodes);

    if (!hasCodes) {
        currentPreview = QImage();

        if (inputText->toPlainText().trimmed().isEmpty()) {
            previewLabel->setPixmap(QPixmap());
            previewLabel->setText("Введи номера самокатов.");
        }
        else {
            previewLabel->setPixmap(QPixmap());
            previewLabel->setText("Не найдено корректных номеров.");
        }

        previewTitle->setText("Предпросмотр");
        return;
    }

    const int cellWidth = 300;
    const int cellHeight = 350;
    const int gap = 12;

    const int columns = std::min(
        3,
        static_cast<int>(numbers.size())
    );

    const int rows = static_cast<int>(
        (numbers.size() + columns - 1) / columns
    );

    const int imageWidth = gap + columns * (cellWidth + gap);
    const int imageHeight = gap + rows * (cellHeight + gap);

    currentPreview = QImage(
        imageWidth,
        imageHeight,
        QImage::Format_ARGB32_Premultiplied
    );

    currentPreview.fill(Qt::white);

    QPainter painter(&currentPreview);
    painter.setRenderHint(QPainter::Antialiasing);

    for (size_t i = 0; i < numbers.size(); ++i) {
        const int column = static_cast<int>(i % columns);
        const int row = static_cast<int>(i / columns);

        const int x = gap + column * (cellWidth + gap);
        const int y = gap + row * (cellHeight + gap);

        const QImage card = generateScooterImage(numbers[i]);

        painter.drawImage(
            QRect(x, y, cellWidth, cellHeight),
            card
        );
    }

    painter.end();

    previewTitle->setText(
        QString("Предпросмотр — QR-кодов: %1").arg(numbers.size())
    );

    previewLabel->setText("");
    previewLabel->setPixmap(
        QPixmap::fromImage(currentPreview).scaled(
            1000,
            800,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        )
    );
}


// Рисование одного QR-кода на принтере
void QRGenerator::drawScooterOnPainter(
    QPainter &painter,
    const QString &scooterNumber,
    int x,
    int y,
    int sizeMm)
{
    const int dpiX = painter.device()
        ? painter.device()->logicalDpiX()
        : DPI;

    const int dpiY = painter.device()
        ? painter.device()->logicalDpiY()
        : DPI;

    const int qrPx = qRound(sizeMm * dpiX / 25.4);
    const int margin = qRound(2.0 * dpiX / 25.4);
    const int labelHeight = qRound(6.0 * dpiY / 25.4);

    const int cardWidth = qrPx + margin * 2;
    const int cardHeight = qrPx + margin * 2 + labelHeight;

    const QRect card(x, y, cardWidth, cardHeight);

    painter.save();

    painter.fillRect(card, Qt::white);
    painter.setPen(Qt::black);
    painter.drawRect(card.adjusted(0, 0, -1, -1));

    const QImage qr = generateQRCodeImage(scooterNumber, qrPx);

    painter.drawImage(
        QRect(x + margin, y + margin, qrPx, qrPx),
        qr
    );

    QFont font("Arial");
    font.setBold(true);
    font.setPixelSize(
        qMax(10, qRound(3.0 * dpiY / 25.4))
    );

    painter.setFont(font);

    painter.drawText(
        QRect(
            x + 2,
            y + margin + qrPx,
            cardWidth - 4,
            labelHeight
        ),
        Qt::AlignCenter,
        scooterNumber
    );

    painter.restore();
}


// Размещение QR-кодов на страницах принтера
bool QRGenerator::drawCodesOnPrinter(
    QPrinter &printer,
    const std::vector<QString> &numbers)
{
    QPainter painter;

    if (!painter.begin(&printer))
        return false;

    const int dpiX = painter.device()->logicalDpiX();
    const int dpiY = painter.device()->logicalDpiY();

    const int qrPx = qRound(CODE_SIZE_MM * dpiX / 25.4);
    const int margin = qRound(2.0 * dpiX / 25.4);
    const int labelHeight = qRound(6.0 * dpiY / 25.4);

    const int cardWidth = qrPx + margin * 2;
    const int cardHeight = qrPx + margin * 2 + labelHeight;

    const int gap = qRound(SPACING_MM * dpiX / 25.4);

    QRect page = painter.viewport();

    const int left = page.left() + gap;
    const int bottom = page.top() + page.height() - gap;
    const int right = page.left() + page.width() - gap;

    int x = left;
    int y = page.top() + gap;

    for (const QString &number : numbers) {

        if (x + cardWidth > right) {
            x = left;
            y += cardHeight + gap;
        }

        if (y + cardHeight > bottom) {
            if (!printer.newPage()) {
                painter.end();
                return false;
            }

            page = painter.viewport();
            x = page.left() + gap;
            y = page.top() + gap;
        }

        drawScooterOnPainter(
            painter,
            number,
            x,
            y,
            static_cast<int>(CODE_SIZE_MM)
        );

        x += cardWidth + gap;
    }

    painter.end();

    return true;
}


// Печать
void QRGenerator::printCodes()
{
    const std::vector<QString> numbers =
        parseScooterNumbers(inputText->toPlainText());

    if (numbers.empty()) {
        QMessageBox::warning(this, "Ошибка", "Нет корректных номеров.");
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setResolution(DPI);
    printer.setPageSize(QPageSize(QPageSize::A4));

    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle("Печать QR-кодов");

    if (dialog.exec() != QDialog::Accepted)
        return;

    if (!drawCodesOnPrinter(printer, numbers)) {
        QMessageBox::critical(
            this,
            "Ошибка печати",
            "Не удалось подготовить документ для печати."
        );
    }
}


// Экспорт PDF
void QRGenerator::saveToPDF()
{
    const std::vector<QString> numbers =
        parseScooterNumbers(inputText->toPlainText());

    if (numbers.empty())
        return;

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить PDF",
        "Urent_QR_Codes.pdf",
        "PDF документы (*.pdf)"
    );

    if (fileName.isEmpty())
        return;

    if (QFileInfo(fileName).suffix().isEmpty())
        fileName += ".pdf";

    QPrinter printer(QPrinter::HighResolution);
    printer.setResolution(DPI);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);

    if (!drawCodesOnPrinter(printer, numbers)) {
        QMessageBox::critical(
            this,
            "Ошибка",
            "Не удалось создать PDF."
        );
        return;
    }

    QMessageBox::information(
        this,
        "Готово",
        "PDF успешно сохранён."
    );
}


// Экспорт PNG
void QRGenerator::saveToPNG()
{
    if (currentPreview.isNull())
        return;

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить PNG",
        "Urent_QR_Codes.png",
        "PNG изображения (*.png)"
    );

    if (fileName.isEmpty())
        return;

    if (QFileInfo(fileName).suffix().isEmpty())
        fileName += ".png";

    if (!currentPreview.save(fileName, "PNG")) {
        QMessageBox::critical(
            this,
            "Ошибка",
            "Не удалось сохранить PNG."
        );
        return;
    }

    QMessageBox::information(
        this,
        "Готово",
        "PNG успешно сохранён."
    );
}


// Экспорт в Word (RTF)
void QRGenerator::saveToWord()
{
    const std::vector<QString> numbers =
        parseScooterNumbers(inputText->toPlainText());

    if (numbers.empty())
        return;

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Сохранить документ для Word",
        "Urent_QR_Codes.rtf",
        "Документы RTF (*.rtf)"
    );

    if (fileName.isEmpty())
        return;

    if (QFileInfo(fileName).suffix().isEmpty())
        fileName += ".rtf";

    QString document =
        "{\\rtf1\\ansi\\deff0"
        "{\\fonttbl{\\f0 Arial;}}"
        "\\viewkind4\\uc1"
        "\\pard\\qc\\b\\fs32 Urent QR Generator\\b0\\fs20\\par"
        "\\par\n";

    for (size_t i = 0; i < numbers.size(); ++i) {
        const QImage image = generateScooterImage(numbers[i]);
        const QByteArray png = imageToPng(image);

        if (png.isEmpty()) {
            QMessageBox::critical(
                this,
                "Ошибка",
                "Не удалось сформировать изображение QR-кода."
            );
            return;
        }

        document += "\\pard\\qc\\b ";
        document += escapeRtfText(numbers[i]);
        document += "\\b0\\par\n";

        document +=
            "{\\pict\\pngblip"
            "\\picw300\\pich350"
            "\\picwgoal2600\\pichgoal3000\n";

        document += bytesToHex(png);
        document += "}\n\\par\n";

        if ((i + 1) % 3 == 0 && i + 1 < numbers.size())
            document += "\\page\n";
    }

    document += "}";

    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(
            this,
            "Ошибка",
            "Не удалось открыть файл для записи."
        );
        return;
    }

    const QByteArray data = document.toUtf8();

    if (file.write(data) != data.size()) {
        QMessageBox::critical(
            this,
            "Ошибка",
            "Не удалось полностью записать документ."
        );
        return;
    }

    file.close();

    QMessageBox::information(
        this,
        "Готово",
        "Документ RTF успешно сохранён. Его можно открыть в Microsoft Word."
    );
}
