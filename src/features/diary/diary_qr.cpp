#include "diary/diary_qr.h"

#include <qrcode.h>

void diary_qr_draw(lgfx::LGFX_Device& gfx, const char* url, int cx, int cy, int pixel_size) {
    if (!url || url[0] == '\0') {
        return;
    }
    const int version = 6;
    uint8_t qrcodeData[qrcode_getBufferSize(version)];
    QRCode qrcode;
    if (qrcode_initText(&qrcode, qrcodeData, version, ECC_LOW, url) != 0) {
        gfx.setTextColor(TFT_RED, TFT_BLACK);
        gfx.drawString("QR too long", cx, cy);
        return;
    }
    const int dim = qrcode.size;
    const int total = dim * pixel_size;
    const int x0 = cx - total / 2;
    const int y0 = cy - total / 2;
    gfx.fillRect(x0 - 2, y0 - 2, total + 4, total + 4, TFT_WHITE);
    for (int y = 0; y < dim; ++y) {
        for (int x = 0; x < dim; ++x) {
            if (qrcode_getModule(&qrcode, x, y)) {
                gfx.fillRect(x0 + x * pixel_size, y0 + y * pixel_size, pixel_size, pixel_size,
                             TFT_BLACK);
            }
        }
    }
}
