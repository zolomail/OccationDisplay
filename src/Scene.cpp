#include "Scene.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {

constexpr size_t SCENE_COUNT = static_cast<size_t>(SceneId::None) + 1;

SemaphoreHandle_t s_mutex = nullptr;
std::vector<std::pair<String, String>> s_overrides[SCENE_COUNT];

SceneId                 s_active_id = SceneId::None;
std::vector<SceneParam> s_active_params;

void ensureMutex()
{
	if (!s_mutex) {
		s_mutex = xSemaphoreCreateMutex();
	}
}

String makeKey(const char* label)
{
	String key;
	key.reserve(strlen(label));
	for (const char* p = label; *p; ++p) {
		const unsigned char c = static_cast<unsigned char>(*p);
		if (c == ' ') {
			key += '_';
		} else {
			key += static_cast<char>(tolower(c));
		}
	}
	return key;
}

bool getOverride(SceneId id, const String& key, String& out_val)
{
	const size_t idx = static_cast<size_t>(id);
	if (idx >= SCENE_COUNT) return false;
	ensureMutex();
	xSemaphoreTake(s_mutex, portMAX_DELAY);
	for (const auto& kv : s_overrides[idx]) {
		if (kv.first == key) {
			out_val = kv.second;
			xSemaphoreGive(s_mutex);
			return true;
		}
	}
	xSemaphoreGive(s_mutex);
	return false;
}

} // namespace

String Scene::text(const char* label, const String& default_val)
{
	const String key = makeKey(label);
	String val = default_val;
	getOverride(m_id, key, val);
	m_params.push_back({ key, label, SceneParamType::Text, val, 0, 0 });
	return val;
}

uint16_t Scene::color(const char* label, uint16_t default_rgb565)
{
	const String key = makeKey(label);
	uint16_t c = default_rgb565;
	String ov;
	if (getOverride(m_id, key, ov)) {
		c = SceneParams::hexToRgb565(ov, default_rgb565);
	}
	m_params.push_back({ key, label, SceneParamType::Color, SceneParams::rgb565ToHex(c), 0, 0 });
	return c;
}

uint8_t Scene::font(const char* label, uint8_t default_font)
{
	const String key = makeKey(label);
	long f = static_cast<long>(default_font);
	String ov;
	if (getOverride(m_id, key, ov)) {
		f = ov.toInt();
	}
	constexpr long max_font = static_cast<long>(PICOPIXEL_FONT);
	if (f < 0L) f = 0L;
	if (f > max_font) f = max_font;
	m_params.push_back({ key, label, SceneParamType::Font, String(f), 0, max_font });
	return static_cast<uint8_t>(f);
}

int32_t Scene::number(const char* label, int32_t default_val, int32_t min_val, int32_t max_val)
{
	const String key = makeKey(label);
	int32_t v = default_val;
	String ov;
	if (getOverride(m_id, key, ov)) {
		v = ov.toInt();
	}
	if (v < min_val) v = min_val;
	if (v > max_val) v = max_val;
	m_params.push_back({ key, label, SceneParamType::Number, String(v), min_val, max_val });
	return v;
}

namespace SceneParams {

void setOverride(SceneId id, const String& key, const String& value)
{
	const size_t idx = static_cast<size_t>(id);
	if (idx >= SCENE_COUNT) return;
	ensureMutex();
	xSemaphoreTake(s_mutex, portMAX_DELAY);
	for (auto& kv : s_overrides[idx]) {
		if (kv.first == key) {
			kv.second = value;
			xSemaphoreGive(s_mutex);
			return;
		}
	}
	s_overrides[idx].emplace_back(key, value);
	xSemaphoreGive(s_mutex);
}

void clearOverrides(SceneId id)
{
	const size_t idx = static_cast<size_t>(id);
	if (idx >= SCENE_COUNT) return;
	ensureMutex();
	xSemaphoreTake(s_mutex, portMAX_DELAY);
	s_overrides[idx].clear();
	xSemaphoreGive(s_mutex);
}

void publishActive(SceneId id, const std::vector<SceneParam>& params)
{
	ensureMutex();
	xSemaphoreTake(s_mutex, portMAX_DELAY);
	s_active_id     = id;
	s_active_params = params;
	xSemaphoreGive(s_mutex);
}

bool getActive(SceneId expected_id, std::vector<SceneParam>& out_params)
{
	ensureMutex();
	xSemaphoreTake(s_mutex, portMAX_DELAY);
	if (s_active_id != expected_id) {
		xSemaphoreGive(s_mutex);
		return false;
	}
	out_params = s_active_params;
	xSemaphoreGive(s_mutex);
	return true;
}

String rgb565ToHex(uint16_t color)
{
	const uint8_t r5 = (color >> 11) & 0x1F;
	const uint8_t g6 = (color >> 5) & 0x3F;
	const uint8_t b5 = color & 0x1F;
	const uint8_t r  = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
	const uint8_t g  = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
	const uint8_t b  = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
	char buf[8];
	snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
	return String(buf);
}

uint16_t hexToRgb565(const String& hex, uint16_t fallback)
{
	const char* s = hex.c_str();
	if (s[0] == '#') ++s;
	if (strlen(s) != 6) return fallback;
	char* end = nullptr;
	const unsigned long val = strtoul(s, &end, 16);
	if (end != s + 6) return fallback;
	const uint8_t r = static_cast<uint8_t>((val >> 16) & 0xFF);
	const uint8_t g = static_cast<uint8_t>((val >> 8) & 0xFF);
	const uint8_t b = static_cast<uint8_t>(val & 0xFF);
	return color565(r, g, b);
}

} // namespace SceneParams
