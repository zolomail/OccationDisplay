#include "ShowTime.h"

ShowTime::ShowTime(Adafruit_GFX &matrix, struct tm &time, int16_t x, int16_t y, int16_t dx, int16_t dy, uint16_t color, uint8_t font, bool full_date) :
	FlyingText(matrix, full_date ? "0000-00-00 00:00:00" : "00:00:00", x, y, dx, dy, color, font),
	m_time(time),
	m_full_date(full_date)
{
}

void ShowTime::render()
{
	if(m_full_date){
		m_text = getFormattedDate() + " " + getFormattedTime();
	}else{
		m_text = getFormattedTime();
	}
	FlyingText::render();
}

String ShowTime::getFormattedDate()
{
	return String(m_time.tm_year + 1900) + "-" +
   			(m_time.tm_mon < 9 ? "0" : "") + String(m_time.tm_mon+1) + "-" +
   			(m_time.tm_mday < 10 ? "0" : "") + String(m_time.tm_mday);
}

String ShowTime::getFormattedTime()
{
	return (m_time.tm_hour < 10 ? "0" : "") + String(m_time.tm_hour) + ":" +
			(m_time.tm_min < 10 ? "0" : "") + String(m_time.tm_min) + ":" +
			(m_time.tm_sec < 10 ? "0" : "") + String(m_time.tm_sec);
}