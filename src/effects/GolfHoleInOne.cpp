#include "GolfHoleInOne.h"
#include "GolferFrames.h"
#include "FastHub75.h"
#include <cmath>

namespace {

constexpr uint16_t BALL_WHITE = color565(255, 255, 255);
constexpr uint16_t BALL_TRAIL = color565(180, 220, 255);
constexpr uint16_t SPARK_GOLD = color565(255, 220, 40);
constexpr uint16_t SPARK_RED  = color565(255, 60, 60);

} // namespace

GolfHoleInOne::GolfHoleInOne(Adafruit_GFX& matrix) :
	Effect(matrix)
{
	m_bg.reset(new FlyingPng(matrix, "/fullscreen/golfer.png", 0, 0, 0, 0, false, false));
}

void GolfHoleInOne::render()
{
	if (m_bg) {
		m_bg->render();
	}

	// Choose golfer pose frame based on animation tick
	size_t pose = 0;
	if (m_tick < 15)      pose = 0; // Address
	else if (m_tick < 20) pose = 1; // Half-backswing
	else if (m_tick < 27) pose = 2; // Top of backswing
	else if (m_tick < 30) pose = 3; // Downswing / impact
	else if (m_tick < 35) pose = 4; // Follow-through
	else                  pose = 5; // Watching the shot

	auto* hub75 = static_cast<MatrixPanel_I2S_DMA*>(&m_matrix);
	FastHub75::drawRGBBitmap(
		*hub75,
		GOLFER_OVERLAY_X,
		GOLFER_OVERLAY_Y,
		GOLFER_FRAMES[pose],
		nullptr,
		GOLFER_OVERLAY_W,
		GOLFER_OVERLAY_H,
		false
	);

	// Render golf ball / hole-in-one pixel effects
	if (m_tick < 29) {
		// Ball sitting on tee at (168, 29)
		m_matrix.drawPixel(168, 29, BALL_WHITE);
	} else if (m_tick <= 78) {
		// Parabolic flight from (168, 29) to green landing at (16, 27)
		const float t = static_cast<float>(m_tick - 29) / static_cast<float>(78 - 29);
		const float x = 168.0f + (16.0f - 168.0f) * t;
		const float base_y = 29.0f + (27.0f - 29.0f) * t;
		const float arc = 24.0f * 4.0f * t * (1.0f - t);
		const int16_t bx = static_cast<int16_t>(lroundf(x));
		const int16_t by = static_cast<int16_t>(lroundf(base_y - arc));

		// Subtle 1-pixel trail behind the ball
		if (m_tick > 30) {
			const float tp = static_cast<float>(m_tick - 30) / static_cast<float>(78 - 29);
			const int16_t tx = static_cast<int16_t>(lroundf(168.0f + (16.0f - 168.0f) * tp));
			const int16_t ty = static_cast<int16_t>(lroundf((29.0f - 2.0f * tp) - 24.0f * 4.0f * tp * (1.0f - tp)));
			if (tx != bx || ty != by) {
				m_matrix.drawPixel(tx, ty, BALL_TRAIL);
			}
		}
		m_matrix.drawPixel(bx, by, BALL_WHITE);
	} else if (m_tick <= 88) {
		// Small bounce on the green from (16, 27) to (10, 28)
		const float t = static_cast<float>(m_tick - 78) / 10.0f;
		const int16_t bx = static_cast<int16_t>(lroundf(16.0f + (10.0f - 16.0f) * t));
		const int16_t by = static_cast<int16_t>(lroundf(27.5f - 3.0f * 4.0f * t * (1.0f - t)));
		m_matrix.drawPixel(bx, by, BALL_WHITE);
	} else if (m_tick <= 102) {
		// Roll along the green from x=10 into the hole at (3, 28)
		const float t = static_cast<float>(m_tick - 88) / 14.0f;
		const int16_t bx = static_cast<int16_t>(lroundf(10.0f + (3.0f - 10.0f) * t));
		m_matrix.drawPixel(bx, 28, BALL_WHITE);
	} else {
		// Ball is in the hole! Celebratory sparkles + "HOLE IN ONE!"
		const uint16_t celeb_t = m_tick - 102;
		if (celeb_t < 24) {
			const int16_t spread = static_cast<int16_t>(celeb_t / 3);
			const uint16_t col = (celeb_t & 2) ? SPARK_GOLD : SPARK_RED;
			m_matrix.drawPixel(3 - spread, 26 - spread, col);
			m_matrix.drawPixel(3 + spread, 26 - spread, col);
			m_matrix.drawPixel(3, 25 - spread, BALL_WHITE);
			if (spread > 1) {
				m_matrix.drawPixel(3 - spread / 2, 23 - spread, SPARK_GOLD);
				m_matrix.drawPixel(3 + spread / 2, 23 - spread, SPARK_RED);
			}
		}
/*
				if ((celeb_t / 6) % 2 == 0) {
						setFont(TINY_WIDE_FONT);
						m_matrix.setTextColor(SPARK_GOLD);
						m_matrix.setCursor(48, 22);
						m_matrix.print("HOLE IN ONE!");
				}
*/
        
	}

        m_tick = (m_tick + 1) % 150;
}
