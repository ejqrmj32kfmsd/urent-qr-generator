#include "QRGenerator.h"
#include "qrcodegen.hpp"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPrintDialog>
#include <QPixmap>
#include <QRectF>
#include <QPrinter>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <exception>

namespace {

constexpr double MmPerInch = 25.4;
constexpr int RtfStickerCellTwips = 1860; // 32.8 mm: sticker 23 mm + ~10 mm gap
constexpr int RtfStickerGoalTwips = 1304; // 23 mm
constexpr int RtfColumns = 6;
constexpr int RtfRowsPerPage = 8;

int mmToPixels(double mm, int dpi)
{
    return qRound(mm * static_cast<double>(dpi) / MmPerInch);
}

QString ensureExtension(QString path, const QString &extension)
{
    if (QFileInfo(path).suffix().isEmpty())
        path += extension;
    return path;
}

QByteArray imageAsPng(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return {};
    return bytes;
}

QByteArray bytesAsRtfHex(const QByteArray &bytes)
{
    static constexpr char hex[] = "0123456789abcdef";
    QByteArray result;
    result.reserve(bytes.size() * 2);
    for (unsigned char value : bytes) {
        result.append(hex[(value >> 4) & 0x0F]);
        result.append(hex[value & 0x0F]);
    }
    return result;
}

void configureA4Printer(QPrinter &printer)
{
    printer.setResolution(600);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(5.0, 5.0, 5.0, 5.0), QPageLayout::Millimeter);
}

} // namespace

QRGenerator::QRGenerator(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Юрент · QR Studio"));
    setMinimumSize(900, 650);
    resize(1080, 730);

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(22, 22, 22, 22);
    root->setSpacing(18);

    // Branded top panel.
    auto *hero = new QFrame(central);
    hero->setObjectName("hero");
    auto *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(24, 18, 24, 18);

    auto *heroText = new QVBoxLayout();
    auto *brand = new QLabel(QStringLiteral("ЮРЕНТ  /  QR STUDIO"), hero);
    brand->setObjectName("heroTitle");
    auto *tagline = new QLabel(
        QStringLiteral("Сервисная печать QR-наклеек для самокатов · формат 23 × 23 мм"), hero);
    tagline->setObjectName("heroSubtitle");
    heroText->addWidget(brand);
    heroText->addWidget(tagline);
    heroLayout->addLayout(heroText);
    heroLayout->addStretch();

    auto *badge = new QLabel(QStringLiteral("SERVICE TOOL"), hero);
    badge->setObjectName("heroBadge");
    badge->setAlignment(Qt::AlignCenter);
    heroLayout->addWidget(badge, 0, Qt::AlignVCenter);
    root->addWidget(hero);

    auto *content = new QHBoxLayout();
    content->setSpacing(18);
    root->addLayout(content, 1);

    // Left card: input and actions.
    auto *inputCard = new QFrame(central);
    inputCard->setObjectName("card");
    inputCard->setMinimumWidth(340);
    inputCard->setMaximumWidth(410);
    auto *inputLayout = new QVBoxLayout(inputCard);
    inputLayout->setContentsMargins(22, 22, 22, 22);
    inputLayout->setSpacing(12);

    auto *inputTitle = new QLabel(QStringLiteral("СПИСОК САМОКАТОВ"), inputCard);
    inputTitle->setObjectName("sectionTitle");
    auto *inputHint = new QLabel(
        QStringLiteral("Вставь номера — по одному на строку или через запятую."), inputCard);
    inputHint->setObjectName("mutedText");
    inputHint->setWordWrap(true);

    inputEdit = new QPlainTextEdit(inputCard);
    inputEdit->setObjectName("inputEdit");
    inputEdit->setPlaceholderText(
        QStringLiteral("137-539\n137540\n000-123"));
    inputEdit->setTabChangesFocus(true);

    countLabel = new QLabel(QStringLiteral("Найдено номеров: 0"), inputCard);
    countLabel->setObjectName("countLabel");

    auto *formatHint = new QLabel(
        QStringLiteral("Форматы: 000-000 или 000000.\nПовторы будут объединены."), inputCard);
    formatHint->setObjectName("mutedText");
    formatHint->setWordWrap(true);

    auto *actions = new QGridLayout();
    actions->setHorizontalSpacing(10);
    actions->setVerticalSpacing(10);

    printButton = new QPushButton(QStringLiteral("Печать на A4"), inputCard);
    printButton->setObjectName("primaryButton");
    pdfButton = new QPushButton(QStringLiteral("Сохранить в PDF"), inputCard);
    wordButton = new QPushButton(QStringLiteral("Сохранить для Word"), inputCard);
    pngButton = new QPushButton(QStringLiteral("Сохранить в PNG"), inputCard);

    actions->addWidget(printButton, 0, 0, 1, 2);
    actions->addWidget(pdfButton, 1, 0);
    actions->addWidget(wordButton, 1, 1);
    actions->addWidget(pngButton, 2, 0, 1, 2);

    inputLayout->addWidget(inputTitle);
    inputLayout->addWidget(inputHint);
    inputLayout->addWidget(inputEdit, 1);
    inputLayout->addWidget(countLabel);
    inputLayout->addWidget(formatHint);
    inputLayout->addSpacing(4);
    inputLayout->addLayout(actions);
    content->addWidget(inputCard, 0);

    // Right card: first-item-only preview.
    auto *previewCard = new QFrame(central);
    previewCard->setObjectName("card");
    auto *previewLayout = new QVBoxLayout(previewCard);
    previewLayout->setContentsMargins(22, 22, 22, 22);
    previewLayout->setSpacing(12);

    auto *previewTitle = new QLabel(QStringLiteral("ПРЕДПРОСМОТР НАКЛЕЙКИ"), previewCard);
    previewTitle->setObjectName("sectionTitle");
    auto *previewDescription = new QLabel(
        QStringLiteral("Показывается первая корректная запись из списка. Размер при экспорте — ровно 23 × 23 мм."),
        previewCard);
    previewDescription->setObjectName("mutedText");
    previewDescription->setWordWrap(true);

    auto *previewSurface = new QFrame(previewCard);
    previewSurface->setObjectName("previewSurface");
    auto *surfaceLayout = new QVBoxLayout(previewSurface);
    surfaceLayout->setContentsMargins(16, 16, 16, 16);

    previewLabel = new QLabel(previewSurface);
    previewLabel->setObjectName("previewLabel");
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumSize(330, 330);
    previewLabel->setText(QStringLiteral("Добавь номер самоката\nдля предпросмотра"));
    surfaceLayout->addWidget(previewLabel, 1, Qt::AlignCenter);

    previewMetaLabel = new QLabel(QStringLiteral("QR: https://ure.su/a/…"), previewCard);
    previewMetaLabel->setObjectName("mutedText");
    previewMetaLabel->setAlignment(Qt::AlignCenter);

    previewLayout->addWidget(previewTitle);
    previewLayout->addWidget(previewDescription);
    previewLayout->addStretch();
    previewLayout->addWidget(previewSurface, 0, Qt::AlignCenter);
    previewLayout->addWidget(previewMetaLabel);
    previewLayout->addStretch();
    content->addWidget(previewCard, 1);

    setStyleSheet(R"QSS(
        QMainWindow, QWidget {
            background: #F6F3FA;
            color: #241D2E;
            font-family: "Segoe UI", "Arial";
            font-size: 10pt;
        }
        #hero {
            background: #7027C8;
            border: none;
            border-radius: 20px;
        }
        #heroTitle {
            color: #FFFFFF;
            font-size: 19pt;
            font-weight: 800;
            letter-spacing: 1px;
        }
        #heroSubtitle {
            color: #E9DDF8;
            font-size: 9pt;
        }
        #heroBadge {
            color: #FFFFFF;
            background: rgba(255,255,255,0.15);
            border: 1px solid rgba(255,255,255,0.35);
            border-radius: 11px;
            padding: 8px 13px;
            font-size: 8pt;
            font-weight: 700;
        }
        #card {
            background: #FFFFFF;
            border: 1px solid #E9E2F0;
            border-radius: 18px;
        }
        #sectionTitle {
            color: #32233F;
            font-size: 10pt;
            font-weight: 800;
            letter-spacing: 0.7px;
        }
        #mutedText {
            color: #81778B;
            font-size: 9pt;
        }
        #inputEdit {
            background: #FBFAFD;
            border: 1px solid #DED5E8;
            border-radius: 12px;
            padding: 12px;
            selection-background-color: #7027C8;
            font-family: "Consolas", "Courier New";
            font-size: 11pt;
        }
        #inputEdit:focus {
            border: 2px solid #9255D6;
        }
        #countLabel {
            color: #7027C8;
            font-weight: 700;
        }
        QPushButton {
            background: #F3EEF8;
            color: #4A2D62;
            border: 1px solid #E6DDF0;
            border-radius: 11px;
            padding: 12px 14px;
            font-weight: 700;
        }
        QPushButton:hover {
            background: #EAE0F5;
            border-color: #C9B3E0;
        }
        QPushButton:pressed { background: #DED0ED; }
        QPushButton:disabled {
            color: #AFA6B7;
            background: #F4F1F6;
            border-color: #EBE6EF;
        }
        #primaryButton {
            color: #FFFFFF;
            background: #7027C8;
            border: none;
            padding: 14px;
        }
        #primaryButton:hover { background: #5D1FA9; }
        #primaryButton:pressed { background: #4D188F; }
        #previewSurface {
            background: #F8F6FA;
            border: 1px dashed #D8CDE4;
            border-radius: 16px;
        }
        #previewLabel {
            background: #FFFFFF;
            border: 1px solid #ECE8F0;
            border-radius: 4px;
            color: #8A8094;
        }
    )QSS");

    connect(inputEdit, &QPlainTextEdit::textChanged,
            this, &QRGenerator::updatePreview);
    connect(printButton, &QPushButton::clicked,
            this, &QRGenerator::printCodes);
    connect(pdfButton, &QPushButton::clicked,
            this, &QRGenerator::saveToPDF);
    connect(wordButton, &QPushButton::clicked,
            this, &QRGenerator::saveToWord);
    connect(pngButton, &QPushButton::clicked,
            this, &QRGenerator::saveToPNG);

    updatePreview();
}

QStringList QRGenerator::parseScooterNumbers(const QString &input) const
{
    QStringList result;
    const QStringList tokens = input.split(
        QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
    const QRegularExpression numberPattern(QStringLiteral("^(\\d{3})-?(\\d{3})$"));

    for (const QString &token : tokens) {
        const auto match = numberPattern.match(token.trimmed());
        if (!match.hasMatch())
            continue;

        const QString compact = match.captured(1) + match.captured(2);
        if (!result.contains(compact))
            result.append(compact);
    }
    return result;
}

QString QRGenerator::formattedNumber(const QString &compactNumber)
{
    if (compactNumber.size() != 6)
        return compactNumber;
    return compactNumber.left(3) + QLatin1Char('-') + compactNumber.mid(3, 3);
}

QString QRGenerator::qrPayload(const QString &compactNumber)
{
    // The supplied reference sticker decodes to this exact URL pattern.
    return QStringLiteral("https://ure.su/a/") + compactNumber;
}

QImage QRGenerator::createStickerImage(const QString &compactNumber, int dpi) const
{
    const int side = mmToPixels(StickerSizeMm, dpi);
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    const int dotsPerMeter = qRound(static_cast<double>(dpi) / 0.0254);
    image.setDotsPerMeterX(dotsPerMeter);
    image.setDotsPerMeterY(dotsPerMeter);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    drawSticker(painter, QRectF(0, 0, side, side), compactNumber);
    painter.end();
    return image;
}

void QRGenerator::drawSticker(QPainter &painter,
                              const QRectF &targetPixels,
                              const QString &compactNumber) const
{
    if (targetPixels.width() <= 0 || targetPixels.height() <= 0 || compactNumber.size() != 6)
        return;

    const qreal sx = targetPixels.width() / StickerSizeMm;
    const qreal sy = targetPixels.height() / StickerSizeMm;
    const auto x = [&targetPixels, sx](qreal mm) { return targetPixels.left() + mm * sx; };
    const auto y = [&targetPixels, sy](qreal mm) { return targetPixels.top() + mm * sy; };
    const auto w = [sx](qreal mm) { return mm * sx; };
    const auto h = [sy](qreal mm) { return mm * sy; };

    painter.save();
    painter.fillRect(targetPixels, Qt::white);

    // Top wordmark, kept black like the physical reference.
    QFont brandFont(QStringLiteral("Arial"));
    brandFont.setWeight(QFont::Black);
    brandFont.setPixelSize(qMax(1, qRound(h(2.55))));
    brandFont.setStretch(150);
    brandFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.25 * sy);
    painter.setFont(brandFont);
    painter.setPen(Qt::black);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.drawText(QRectF(x(0.5), y(0.05), w(22.0), h(2.8)),
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     QStringLiteral("ЮРЕНТ"));

    // QR content: https://ure.su/a/ + six-digit scooter ID.
    const QByteArray payload = qrPayload(compactNumber).toUtf8();
    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(
        payload.constData(), qrcodegen::QrCode::Ecc::MEDIUM);

    // Pixel-aligned modules avoid blurred edges in print and PNG exports.
    constexpr qreal qrSizeMm = 16.2;
    constexpr qreal qrLeftMm = (StickerSizeMm - qrSizeMm) / 2.0;
    constexpr qreal qrTopMm = 3.0;
    const int modules = qr.getSize();
    const int leftPx = qRound(x(qrLeftMm));
    const int topPx = qRound(y(qrTopMm));
    const int qrSidePx = qRound(w(qrSizeMm));

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (int row = 0; row < modules; ++row) {
        const int y0 = topPx + (row * qrSidePx) / modules;
        const int y1 = topPx + ((row + 1) * qrSidePx) / modules;
        for (int col = 0; col < modules; ++col) {
            if (!qr.getModule(col, row))
                continue;
            const int x0 = leftPx + (col * qrSidePx) / modules;
            const int x1 = leftPx + ((col + 1) * qrSidePx) / modules;
            painter.fillRect(QRect(x0, y0, x1 - x0, y1 - y0), Qt::black);
        }
    }

    // Bottom scooter number, always displayed in NNN-NNN format.
    QFont numberFont(QStringLiteral("Arial"));
    numberFont.setWeight(QFont::Black);
    numberFont.setPixelSize(qMax(1, qRound(h(3.15))));
    numberFont.setStretch(115);
    painter.setFont(numberFont);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.drawText(QRectF(x(0.3), y(19.25), w(22.4), h(3.45)),
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     formattedNumber(compactNumber));

    painter.restore();
}

void QRGenerator::updatePreview()
{
    const QStringList numbers = parseScooterNumbers(inputEdit->toPlainText());
    countLabel->setText(QStringLiteral("Найдено уникальных номеров: %1").arg(numbers.size()));

    const bool enabled = !numbers.isEmpty();
    printButton->setEnabled(enabled);
    pdfButton->setEnabled(enabled);
    wordButton->setEnabled(enabled);
    pngButton->setEnabled(enabled);

    if (!enabled) {
        currentPreview = QImage();
        previewLabel->setPixmap(QPixmap());
        previewLabel->setText(QStringLiteral("Добавь номер самоката\nдля предпросмотра"));
        previewMetaLabel->setText(QStringLiteral("QR: https://ure.su/a/…"));
        return;
    }

    // The brief explicitly asks to preview only the first valid scooter.
    currentPreview = createStickerImage(numbers.first(), ExportDpi);
    previewLabel->setText(QString());
    previewLabel->setPixmap(QPixmap::fromImage(currentPreview).scaled(
        350, 350, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    previewMetaLabel->setText(
        QStringLiteral("%1  →  %2")
            .arg(formattedNumber(numbers.first()), qrPayload(numbers.first())));
}

bool QRGenerator::drawAllStickers(QPrinter &printer, const QStringList &numbers) const
{
    if (numbers.isEmpty())
        return false;

    QPainter painter;
    if (!painter.begin(&printer))
        return false;

    const int dpiX = qMax(1, printer.logicalDpiX());
    const int dpiY = qMax(1, printer.logicalDpiY());
    QRect viewport = painter.viewport();

    const qreal pageWidthMm = viewport.width() * MmPerInch / dpiX;
    const qreal pageHeightMm = viewport.height() * MmPerInch / dpiY;
    const int columns = qMax(1, static_cast<int>(std::floor(
        (pageWidthMm + StickerGapMm) / (StickerSizeMm + StickerGapMm))));
    const int rows = qMax(1, static_cast<int>(std::floor(
        (pageHeightMm + StickerGapMm) / (StickerSizeMm + StickerGapMm))));
    const int perPage = columns * rows;

    const qreal gridWidthMm = columns * StickerSizeMm + (columns - 1) * StickerGapMm;
    const qreal startXmm = (pageWidthMm - gridWidthMm) / 2.0;

    for (int i = 0; i < numbers.size(); ++i) {
        const int indexOnPage = i % perPage;
        if (i > 0 && indexOnPage == 0) {
            if (!printer.newPage()) {
                painter.end();
                return false;
            }
            viewport = painter.viewport();
        }

        const int column = indexOnPage % columns;
        const int row = indexOnPage / columns;
        const qreal leftMm = startXmm + column * (StickerSizeMm + StickerGapMm);
        const qreal topMm = row * (StickerSizeMm + StickerGapMm);

        const qreal leftPx = viewport.left() + leftMm * dpiX / MmPerInch;
        const qreal topPx = viewport.top() + topMm * dpiY / MmPerInch;
        const qreal widthPx = StickerSizeMm * dpiX / MmPerInch;
        const qreal heightPx = StickerSizeMm * dpiY / MmPerInch;

        drawSticker(painter,
                    QRectF(leftPx, topPx, widthPx, heightPx),
                    numbers.at(i));
    }

    painter.end();
    return true;
}

void QRGenerator::printCodes()
{
    const QStringList numbers = parseScooterNumbers(inputEdit->toPlainText());
    if (numbers.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Нет номеров"),
                             QStringLiteral("Введи номера в формате 000-000 или 000000."));
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    configureA4Printer(printer);

    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(QStringLiteral("Печать QR-наклеек Urent"));
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (!drawAllStickers(printer, numbers)) {
        QMessageBox::critical(this, QStringLiteral("Ошибка печати"),
                              QStringLiteral("Не удалось сформировать страницы для печати."));
    }
}

void QRGenerator::saveToPDF()
{
    const QStringList numbers = parseScooterNumbers(inputEdit->toPlainText());
    if (numbers.isEmpty())
        return;

    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Сохранить наклейки в PDF"),
        QStringLiteral("Urent_QR_Stickers.pdf"),
        QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return;
    path = ensureExtension(path, QStringLiteral(".pdf"));

    QPrinter printer(QPrinter::HighResolution);
    configureA4Printer(printer);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);

    if (!drawAllStickers(printer, numbers)) {
        QFile::remove(path);
        QMessageBox::critical(this, QStringLiteral("Ошибка PDF"),
                              QStringLiteral("Не удалось создать PDF-файл."));
        return;
    }

    showExportSuccess(path);
}

void QRGenerator::saveToWord()
{
    const QStringList numbers = parseScooterNumbers(inputEdit->toPlainText());
    if (numbers.isEmpty())
        return;

    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Сохранить документ для Microsoft Word"),
        QStringLiteral("Urent_QR_Stickers.rtf"),
        QStringLiteral("Документ Word (RTF) (*.rtf)"));
    if (path.isEmpty())
        return;
    path = ensureExtension(path, QStringLiteral(".rtf"));

    // RTF is used because Qt Widgets has no built-in DOCX writer; Word opens RTF natively.
    QString rtf = QStringLiteral(
        "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Arial;}}"
        "\\paperw11906\\paperh16838\\margl340\\margr340\\margt340\\margb340"
        "\\viewkind4\\uc1\n");

    for (int rowStart = 0; rowStart < numbers.size(); rowStart += RtfColumns * RtfRowsPerPage) {
        const int rowsOnPage = qMin(RtfColumns * RtfRowsPerPage,
                                    static_cast<int>(numbers.size()) - rowStart);
        const int rowCount = (rowsOnPage + RtfColumns - 1) / RtfColumns;

        for (int row = 0; row < rowCount; ++row) {
            rtf += QStringLiteral("{\\trowd\\trgaph0\\trleft0\\trrh-%1")
                       .arg(RtfStickerCellTwips);
            for (int col = 1; col <= RtfColumns; ++col)
                rtf += QStringLiteral("\\clvertalt\\cellx%1")
                           .arg(col * RtfStickerCellTwips);
            rtf += QLatin1Char('\n');

            for (int col = 0; col < RtfColumns; ++col) {
                const int index = rowStart + row * RtfColumns + col;
                rtf += QStringLiteral("\\pard\\intbl\\ql\\sb0\\sa0 ");

                if (index < numbers.size()) {
                    const QImage sticker = createStickerImage(numbers.at(index), 300);
                    const QByteArray png = imageAsPng(sticker);
                    if (png.isEmpty()) {
                        QMessageBox::critical(this, QStringLiteral("Ошибка Word"),
                                              QStringLiteral("Не удалось создать изображение QR-кода."));
                        return;
                    }
                    rtf += QStringLiteral("{\\pict\\pngblip\\picw%1\\pich%1\\picwgoal%2\\pichgoal%2\n")
                               .arg(sticker.width())
                               .arg(RtfStickerGoalTwips);
                    rtf += QString::fromLatin1(bytesAsRtfHex(png));
                    rtf += QStringLiteral("}\n");
                }
                rtf += QStringLiteral("\\cell ");
            }
            rtf += QStringLiteral("\\row}\n");
        }

        if (rowStart + rowsOnPage < numbers.size())
            rtf += QStringLiteral("\\page\n");
    }
    rtf += QLatin1Char('}');

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, QStringLiteral("Ошибка Word"),
                              QStringLiteral("Не удалось записать файл: %1").arg(file.errorString()));
        return;
    }
    const QByteArray data = rtf.toLatin1();
    if (file.write(data) != data.size()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка Word"),
                              QStringLiteral("Не удалось полностью записать документ."));
        return;
    }
    file.close();
    showExportSuccess(path);
}

void QRGenerator::saveToPNG()
{
    const QStringList numbers = parseScooterNumbers(inputEdit->toPlainText());
    if (numbers.isEmpty())
        return;

    if (numbers.size() == 1) {
        QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Сохранить QR-наклейку в PNG"),
            formattedNumber(numbers.first()) + QStringLiteral(".png"),
            QStringLiteral("PNG (*.png)"));
        if (path.isEmpty())
            return;
        path = ensureExtension(path, QStringLiteral(".png"));

        if (!createStickerImage(numbers.first(), ExportDpi).save(path, "PNG")) {
            QMessageBox::critical(this, QStringLiteral("Ошибка PNG"),
                                  QStringLiteral("Не удалось сохранить изображение."));
            return;
        }
        showExportSuccess(path);
        return;
    }

    const QString baseFolder = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Выбери папку для набора PNG-наклеек"));
    if (baseFolder.isEmpty())
        return;

    const QString folderName = QStringLiteral("Urent_QR_%1")
                                   .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString outputFolder = QDir(baseFolder).filePath(folderName);
    if (!QDir().mkpath(outputFolder)) {
        QMessageBox::critical(this, QStringLiteral("Ошибка PNG"),
                              QStringLiteral("Не удалось создать папку: %1").arg(outputFolder));
        return;
    }

    for (const QString &number : numbers) {
        const QString filePath = QDir(outputFolder).filePath(
            formattedNumber(number) + QStringLiteral(".png"));
        if (!createStickerImage(number, ExportDpi).save(filePath, "PNG")) {
            QMessageBox::critical(this, QStringLiteral("Ошибка PNG"),
                                  QStringLiteral("Не удалось сохранить файл: %1").arg(filePath));
            return;
        }
    }

    showExportSuccess(outputFolder);
}

void QRGenerator::showExportSuccess(const QString &filePath)
{
    QMessageBox::information(
        this, QStringLiteral("Готово"),
        QStringLiteral("Файл успешно сохранён:\n%1").arg(QDir::toNativeSeparators(filePath)));
}
