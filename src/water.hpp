/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/
#pragma once

#include <vector>

#include "Color.hpp"

#include "entity_fwd.hpp"
#include "formula_fwd.hpp"
#include "kre/Geometry.hpp"
#include "variant.hpp"

class Level;

class water
{
public:
	water();
	explicit water(variant node);

	variant write() const;

	void add_rect(const rect& r, const KRE::Color& color, variant obj);
	void delete_rect(const rect& r);

	bool draw(int x, int y, int w, int h) const;
	int zorder() const { return zorder_; }

	void beginDrawing();
	void endDrawing() const;
	void process(const Level& lvl);

	void getCurrent(const Entity& e, int* velocity_x, int* velocity_y) const;

	bool isUnderwater(const rect& r, rect* water_area=NULL, variant* obj=NULL) const;

	void addWave(const point& p, double xvelocity, double height, double length, double delta_height, double delta_length);

	struct wave {
		double xpos;
		double xvelocity;
		double height;
		double length;
		double delta_height;
		double delta_length;

		int left_bound, right_bound;

		void process();
	};
	
private:

	struct area {
		area(const rect& r, const unsigned char* color, variant obj);
		rect rect_;
		graphics::water_distortion distortion_;
		std::vector<char> draw_detection_buf_;

		std::vector<wave> waves_;

		//segments of the surface without solid.
		std::vector<std::pair<int, int> > surface_segments_;
		bool surface_segments_init_;

		unsigned char color_[4];
		variant obj_;
	};

	std::vector<area> areas_;

	static void init_area_surface_segments(const Level& lvl, area& a);

	bool drawArea(const area& a, int x, int y, int w, int h) const;

	bool drawArea(const area& a, int begin_layer, int end_layer, int x, int y, int w, int h) const;

	int zorder_;

	enum { BadOffset = -100000 };

	game_logic::const_formula_ptr current_x_formula_, current_y_formula_;
};
