#include "FireWorks.h"

FireWorks::FireWorks(Adafruit_GFX &matrix, int16_t max_delay_next_launch) :
	Effect(matrix),
	m_max_delay_next_launch(max_delay_next_launch)
{
	reset();
}

void FireWorks::reset()
{
	m_launch_x = random(0, m_matrix.width()-1);
	m_max_alt = random(20, m_matrix.height()-1);
	m_delta_x = random(-m_matrix.width()/2, m_matrix.width()/2);
	m_rocket_pos = random(m_max_delay_next_launch, 0);
	m_rocket_speed = random(1, 6);
	m_explosion_size = random(9, 25);
	c_r = random(25, 255);
	c_g = random(25, 255);
	c_b = random(25, 255);
}

void FireWorks::render()
{
	m_rocket_pos += m_rocket_speed;

	if(m_rocket_pos < 0) {
	} else if(m_rocket_pos < 90){
		for(int i=0;i<5;i++){
			m_matrix.drawPixel(m_launch_x+cos((m_rocket_pos-4+i)*M_PI/180.0)*m_delta_x, m_matrix.height()-sin((m_rocket_pos-4+i)*M_PI/180.0)*m_max_alt, color565((255*(i+1))/5, (230*(i+1))/5, (110*(i+1))/5));
		}
	}else if(m_rocket_pos < 110){
		int16_t xx = m_launch_x+cos(90*M_PI/180.0)*m_delta_x;
		int16_t yy = m_matrix.height()-sin(m_rocket_pos*M_PI/180.0)*m_max_alt;
		for(int i=1;i<m_rocket_pos-90;i+=2)
		{
			for(int rr = random(0,19); rr <360; rr+=35-i){
				m_matrix.drawPixel(xx+sin(rr*M_PI/180.0)*(m_explosion_size*i/20.0), yy+cos(rr*M_PI/180.0)*(m_explosion_size*i/20.0)+(i/3), color565(c_r*(i+10)/30.0, c_g*(i+10)/30.0, c_b*(i+10)/30.0));
			}
		}
	}else if( m_rocket_pos< 130){
		int16_t xx = m_launch_x+cos(90*M_PI/180.0)*m_delta_x;
		int16_t yy = m_matrix.height()-sin(m_rocket_pos*M_PI/180.0)*m_max_alt;
		for(int i=1;i<20;i+=2)
		{
			for(int rr = random(0,19); rr <360; rr+=35-i){
				m_matrix.drawPixel(xx+sin(rr*M_PI/180.0)*(m_explosion_size*i/20.0), yy+cos(rr*M_PI/180.0)*(m_explosion_size*i/20.0)+(i/3), color565(c_r*(i+10)/30.0, c_g*(i+10)/30.0, c_b*(i+10)/30.0));
			}
			c_r = max(c_r-1,0);
			c_g = max(c_g-2,0);
			c_b = max(c_b-3,0);
		}
	}else{
		reset();
	}
}
