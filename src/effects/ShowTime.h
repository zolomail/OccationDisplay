#pragma once

#include <time.h>
#include "FlyingText.h"

class ShowTime : public FlyingText
{
public:
	ShowTime(
		Adafruit_GFX &matrix,
		struct tm &time,
		int16_t x = 0,
		int16_t y = 0,
		int16_t dx = 1,
		int16_t dy = 1,
		uint16_t color = 0xFFFF,
		uint8_t font = 0,
		const String &format = "hh:mm:ss",
		const String &countdown_target = "",
		const String &expired_text = ""
	);
	void render() override;
	String getFormattedDate();
	String getFormattedTime();

private:
	String formatDateTime();
	String getFormattedCountdown();
	void updateBoundsIfChanged();

	struct tm &m_time;
	String     m_format;
	bool       m_has_countdown = false;
	time_t     m_target_epoch  = 0;
	String     m_expired_text;
	int16_t    m_spec_x;
	int16_t    m_spec_y;
	String     m_last_text;
};
