#pragma once

#include <time.h>
#include "FlyingText.h"

class ShowTime : public FlyingText
{

public:
	ShowTime(Adafruit_GFX &matrix, struct tm &time, int16_t x=0, int16_t y=0, int16_t dx=1, int16_t dy=1, uint16_t color = 0xFFFF, uint8_t font=0, bool full_date=false);
	void render();
	String getFormattedDate();
	String getFormattedTime();

private:
	struct tm &m_time;
	bool m_full_date;
};