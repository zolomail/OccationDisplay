#include "ShootingStar.h"

ShootingStar::ShootingStar(Adafruit_GFX &matrix, int16_t max_delay_next_launch) :
	Effect(matrix),
	m_max_delay_next_launch(max_delay_next_launch)
{
	reset();
}

void ShootingStar::reset()
{
	m_launch_x = random(0, m_matrix.width()-1);
	m_angle = ((random(0, 160)+100)%180)*M_PI/180.0;
	m_pos = random(m_max_delay_next_launch, 0);
	m_speed = random(1, 5);
}

#define STREAK_LENGTH 6
void ShootingStar::render()
{
	m_pos += m_speed;

	if(m_pos < 0) {
	} else if(m_pos < 40){
		for(int i=0;i<STREAK_LENGTH;i++){
			m_r = (255*(i+1))/STREAK_LENGTH;
			m_g = (200*(i+1))/STREAK_LENGTH;
			m_b = (150*(i+1))/STREAK_LENGTH;
			m_matrix.drawPixel(m_launch_x+cos(m_angle)*(2*m_pos+2*i), sin(m_angle)*((m_pos+i)/2), color565(m_r, m_g, m_b));
		}
	} else if(m_pos < 60){
		for(int i=STREAK_LENGTH-1;i>=0;i--){
			m_r = max(m_r-2,0);
			m_g = max(m_g-4,0);
			m_b = max(m_b-6,0);
			m_matrix.drawPixel(m_launch_x+cos(m_angle)*(2*m_pos+2*i), sin(m_angle)*((m_pos+i)/2), color565(m_r, m_g, m_b));
		}
	}else{
		reset();
	}
}
