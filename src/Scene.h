#pragma once

#include <memory>
#include <utility>
#include <vector>
#include "effects/Effect.h"

enum class SceneId : uint8_t {
	Welcome = 0,	//Must be first
	Connecting,
	SetupAp,
	WifiUnknown,
	WifiStopped,
	Test,
	NewYearsMidnight,
	JanuaryDays,
	Aquatic,
	Halloween,
	JustSnow,
	ChristmasEve,
	Birthday,
	NewYearsDay,
	ChristmasDays,
	NormalDays,
	FlamingHot,
	WaterEffect,
	BumpMapEffect,
	ValentinesDay,
	CubeEffect,
	WebUiQr,
	Test1,
	Test2,
	Test3,
	Test4,
	Test5,
	FullscreenTest,
	None	// Must be last
};

enum class SceneParamType : uint8_t {
	Text,
	Color,
	Font,
	Number
};

struct SceneParam {
	String         key;
	String         label;
	SceneParamType type;
	String         value;
	int32_t        min_val = 0;
	int32_t        max_val = 0;
};

class Scene
{
public:
	Scene() = default;
	Scene(SceneId id, Adafruit_GFX& display) : m_id(id), m_display(&display) {}

	template<typename T, typename... Args>
	void add_effect(Args&&... args)
	{
		if (m_display) {
			m_effects.push_back(std::make_shared<T>(*m_display, std::forward<Args>(args)...));
		}
	}

	void render()
	{
		for (auto& effect : m_effects) {
			effect->render();
			yield();
		}
	}

	void clear()
	{
		m_effects.clear();
		m_params.clear();
	}

	SceneId id() const { return m_id; }
	const std::vector<SceneParam>& params() const { return m_params; }

	String   text(const char* label, const String& default_val);
	uint16_t color(const char* label, uint16_t default_rgb565);
	uint8_t  font(const char* label, uint8_t default_font);
	int32_t  number(const char* label, int32_t default_val, int32_t min_val, int32_t max_val);

private:
	SceneId                              m_id      = SceneId::None;
	Adafruit_GFX*                        m_display = nullptr;
	std::vector<std::shared_ptr<Effect>> m_effects;
	std::vector<SceneParam>              m_params;
};

namespace SceneParams {

void setOverride(SceneId id, const String& key, const String& value);
void clearOverrides(SceneId id);
void publishActive(SceneId id, const std::vector<SceneParam>& params);
bool getActive(SceneId expected_id, std::vector<SceneParam>& out_params);

String   rgb565ToHex(uint16_t color);
uint16_t hexToRgb565(const String& hex, uint16_t fallback);

} // namespace SceneParams
