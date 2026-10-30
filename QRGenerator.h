#pragma once

#include <QMainWindow>
#include <QImage>
#include <QStringList>

class QLabel;
class QRectF;
class QPlainTextEdit;
class QPushButton;
class QPainter;
class QPrinter;

class QRGenerator final : public QMainWindow
{
    Q_OBJECT

public:
    explicit QRGenerator(QWidget *parent = nullptr);
    ~QRGenerator() override = default;

private slots:
    void updatePreview();
    void printCodes();
    void saveToPDF();
    void saveToWord();
    void saveToPNG();

private:
    static constexpr double StickerSizeMm = 23.0;
    static constexpr double StickerGapMm = 10.0;
    static constexpr int ExportDpi = 600;

    QStringList parseScooterNumbers(const QString &input) const;
    static QString formattedNumber(const QString &compactNumber);
    static QString qrPayload(const QString &compactNumber);

    QImage createStickerImage(const QString &compactNumber, int dpi = ExportDpi) const;
    void drawSticker(QPainter &painter,
                     const QRectF &targetPixels,
                     const QString &compactNumber) const;
    bool drawAllStickers(QPrinter &printer, const QStringList &numbers) const;

    void showExportSuccess(const QString &filePath);

    QPlainTextEdit *inputEdit = nullptr;
    QLabel *countLabel = nullptr;
    QLabel *previewLabel = nullptr;

    QPushButton *printButton = nullptr;
    QPushButton *pdfButton = nullptr;
    QPushButton *wordButton = nullptr;
    QPushButton *pngButton = nullptr;

    QImage currentPreview;
};
