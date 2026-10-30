#include "qrcodegen.hpp"
#include <cmath>
QImage QRGenerator::generateQRCodeImage(const QString &text, int size) {
    QImage image(size, size, QImage::Format_Grayscale8);
    image.fill(255);
    qrcodegen::QrCode qr;
    try {
        qr = qrcodegen::QrCode::encodeText(
                 std::string_view(text.toUtf8().constData()),
                 qrcodegen::QrCode::Ecc::MEDIUM);
    } catch (...) { return QImage(); }
    const int n = qr.getSize();
    const double sc = static_cast<double>(size) / n;
    for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) {
        if (!qr.getModule(x, y)) continue;
        int x0=(int)std::llround(x*sc),     x1=(int)std::llround((x+1)*sc);
        int y0=(int)std::llround(y*sc),     y1=(int)std::llround((y+1)*sc);
        for (int yy=y0; yy<y1; ++yy) for (int xx=x0; xx<x1; ++xx) image.setPixel(xx,yy,0);
    }
    return image;
}
