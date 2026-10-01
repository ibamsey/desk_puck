#ifndef DIARY_DIARY_QR_H
#define DIARY_DIARY_QR_H

#include "LGFX_config.h"

/** Draw QR for URL centred at cx,cy with module pixel size. */
void diary_qr_draw(lgfx::LGFX_Device& gfx, const char* url, int cx, int cy, int pixel_size);

#endif // DIARY_DIARY_QR_H
