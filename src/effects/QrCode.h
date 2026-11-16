#pragma once

#include <qrcode.h>
#include "Effect.h"

class QrCode : public Effect
{
public:
	// text:       String/URL to encode (auto-selects QR Version 1 (21x21), 2 (25x25), or 3 (29x29))
	// x, y:       Top-left position of the QR box including quiet zone (-1 = center on that axis)
	// fg_color:   Module (dark) color (default black 0x0000)
	// bg_color:   Background / quiet-zone color (default soft white so LEDs don't glare into camera)
	// quiet_zone: Border thickness in pixels around the QR modules (default 2)
	QrCode(
		Adafruit_GFX &matrix,
		const String &text,
		int16_t x = -1,
		int16_t y = -1,
		uint16_t fg_color = 0x0000,
		uint16_t bg_color = color565(200, 200, 200),
		uint8_t quiet_zone = 2
	);

	void setText(const String &text);
	void render() override;

private:
	int16_t  m_x;
	int16_t  m_y;
	uint16_t m_fg_color;
	uint16_t m_bg_color;
	uint8_t  m_quiet_zone;
	QRCode   m_qr{};
	uint8_t  m_qr_data[128]{}; // Sufficient up to QR Version 3 (29x29 = 106 bytes)
};
