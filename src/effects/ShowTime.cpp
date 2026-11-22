#include "ShowTime.h"
#include <cctype>
#include <cstdio>

namespace {

bool parseCountdownTarget(const String &str, time_t &out_epoch)
{
	if (str.length() == 0) return false;

	int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
	const int parsed = sscanf(str.c_str(), "%d-%d-%d %d:%d:%d", &year, &mon, &day, &hour, &min, &sec);
	if (parsed < 3) {
		return false;
	}

	struct tm target{};
	target.tm_year  = year - 1900;
	target.tm_mon   = mon - 1;
	target.tm_mday  = day;
	target.tm_hour  = hour;
	target.tm_min   = min;
	target.tm_sec   = sec;
	target.tm_isdst = -1; // Auto-detect DST in local timezone

	out_epoch = mktime(&target);
	return out_epoch != static_cast<time_t>(-1);
}

bool matchToken(const String &lower, size_t idx, const char *tok)
{
	const size_t len = strlen(tok);
	if (idx + len > lower.length()) return false;
	for (size_t i = 0; i < len; ++i) {
		if (lower[idx + i] != tok[i]) return false;
	}
	return true;
}

void append2(String &out, int val)
{
	if (val < 10) out += '0';
	out += String(val);
}

} // namespace

ShowTime::ShowTime(
	Adafruit_GFX &matrix,
	struct tm &time,
	int16_t x,
	int16_t y,
	int16_t dx,
	int16_t dy,
	uint16_t color,
	uint8_t font,
	const String &format,
	const String &countdown_target,
	const String &expired_text
) :
	FlyingText(
		matrix,
		countdown_target.length() > 0 ? "00d 00:00:00" : format,
		x, y, dx, dy, color, font
	),
	m_time(time),
	m_format(format.length() > 0 ? format : String("hh:mm:ss")),
	m_expired_text(expired_text),
	m_spec_x(x),
	m_spec_y(y),
	m_last_text(m_text)
{
	m_has_countdown = parseCountdownTarget(countdown_target, m_target_epoch);
}

void ShowTime::updateBoundsIfChanged()
{
	if (m_text == m_last_text) return;
	m_last_text = m_text;

	int16_t x1 = 0, y1 = 0;
	setFont(m_font);
	m_matrix.getTextBounds(m_text, 0, 0, &x1, &y1, &m_width, &m_height);
	m_height -= 2;

	if (m_dx == 0 && m_dy == 0) {
		m_x = resolveX(m_spec_x, m_width) - x1;
		m_y = resolveY(m_spec_y, m_height) - y1;
	}
}

void ShowTime::render()
{
	m_text = m_has_countdown ? getFormattedCountdown() : formatDateTime();
	updateBoundsIfChanged();
	FlyingText::render();
}

String ShowTime::formatDateTime()
{
	String lower = m_format;
	lower.toLowerCase();

	String out;
	out.reserve(m_format.length() + 4);

	bool seen_hour = false;
	const size_t n = lower.length();
	for (size_t i = 0; i < n;) {
		if (matchToken(lower, i, "yyyy")) {
			out += String(m_time.tm_year + 1900);
			i += 4;
		} else if (matchToken(lower, i, "yy")) {
			append2(out, (m_time.tm_year + 1900) % 100);
			i += 2;
		} else if (matchToken(lower, i, "dd")) {
			append2(out, m_time.tm_mday);
			i += 2;
		} else if (matchToken(lower, i, "hh")) {
			append2(out, m_time.tm_hour);
			seen_hour = true;
			i += 2;
		} else if (matchToken(lower, i, "mm")) {
			const char prev = (i > 0) ? lower[i - 1] : '\0';
			const char next = (i + 2 < n) ? lower[i + 2] : '\0';
			const bool is_minute = seen_hour || prev == ':' || next == ':';
			append2(out, is_minute ? m_time.tm_min : (m_time.tm_mon + 1));
			i += 2;
		} else if (matchToken(lower, i, "ss")) {
			append2(out, m_time.tm_sec);
			i += 2;
		} else {
			out += m_format[i];
			++i;
		}
	}
	return out;
}

String ShowTime::getFormattedCountdown()
{
	struct tm now_tm = m_time;
	now_tm.tm_isdst = -1;
	const time_t now = mktime(&now_tm);
	const long diff = static_cast<long>(difftime(m_target_epoch, now));

	if (diff <= 0) {
		return (m_expired_text.length() > 0) ? m_expired_text : String("0d 00:00:00");
	}

	const long days  = diff / 86400L;
	long       rem   = diff % 86400L;
	const long hours = rem / 3600L;
	rem %= 3600L;
	const long mins  = rem / 60L;
	const long secs  = rem % 60L;

	char buf[32];
	snprintf(buf, sizeof(buf), "%ldd %02ld:%02ld:%02ld", days, hours, mins, secs);
	return String(buf);
}

String ShowTime::getFormattedDate()
{
	return String(m_time.tm_year + 1900) + "-" +
			(m_time.tm_mon < 9 ? "0" : "") + String(m_time.tm_mon + 1) + "-" +
			(m_time.tm_mday < 10 ? "0" : "") + String(m_time.tm_mday);
}

String ShowTime::getFormattedTime()
{
	return (m_time.tm_hour < 10 ? "0" : "") + String(m_time.tm_hour) + ":" +
			(m_time.tm_min < 10 ? "0" : "") + String(m_time.tm_min) + ":" +
			(m_time.tm_sec < 10 ? "0" : "") + String(m_time.tm_sec);
}
