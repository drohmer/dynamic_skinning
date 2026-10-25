#pragma once

#include "cgp/05_vec/vec.hpp"

namespace cgp
{
	struct velocity_tracker
	{
		vec3 position_record = {0,0,0};
		float time_record = 0.0f;

		vec3 velocity = {0,0,0};
		float smoothing_weight = 0.6f;

		void set_record(vec3 const& position, float time);
		void add(vec3 const& position, float time);
	};
}