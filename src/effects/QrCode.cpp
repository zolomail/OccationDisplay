#include "QrCode.h"

QrCode::QrCode(
	Adafruit_GFX &matrix,
	const String &text,
	int16_t x,
	int16_t y,
	uint16_t fg_color,
	uint16_t bg_color,
	uint8_t quiet_zone
) :
	Effect(matrix),
	m_x(x),
	m_y(y),
	m_fg_color(fg_color),
	m_bg_color(bg_color),
	m_quiet_zone(quiet_zone)
{
	setText(text);
}

void QrCode::setText(const String &text)
{
	const size_t len = text.length();
	const uint8_t version = (len <= 17) ? 1 : ((len <= 32) ? 2 : 3);
	qrcode_initText(&m_qr, m_qr_data, version, ECC_LOW, text.c_str());
}

void QrCode::render()
{
	const int16_t box_size = m_qr.size + 2 * m_quiet_zone;
	const int16_t start_x  = (m_x < 0) ? ((m_matrix.width()  - box_size) / 2) : m_x;
	const int16_t start_y  = (m_y < 0) ? ((m_matrix.height() - box_size) / 2) : m_y;

	// Draw background + quiet zone
	m_matrix.fillRect(start_x, start_y, box_size, box_size, m_bg_color);

	// Draw dark QR modules
	const int16_t qr_x = start_x + m_quiet_zone;
	const int16_t qr_y = start_y + m_quiet_zone;

	for (uint8_t y = 0; y < m_qr.size; ++y) {
		const int16_t py = qr_y + y;
		if (py < 0 || py >= m_matrix.height()) continue;

		for (uint8_t x = 0; x < m_qr.size; ++x) {
			if (qrcode_getModule(&m_qr, x, y)) {
				const int16_t px = qr_x + x;
				if (px >= 0 && px < m_matrix.width()) {
					m_matrix.drawPixel(px, py, m_fg_color);
				}
			}
		}
	}
}
