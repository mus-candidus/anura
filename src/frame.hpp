/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef FRAME_HPP_INCLUDED
#define FRAME_HPP_INCLUDED

#include <boost/array.hpp>

#include <string>
#include <vector>

#include "kre/Geometry.hpp"
#include "kre/Material.hpp"

#include "formula.hpp"
#include "obj_reader.hpp"
#include "solid_map_fwd.hpp"
#include "variant.hpp"
#include <glm/glm.hpp>

namespace graphics {
class blit_queue;
}

class frame : public game_logic::FormulaCallable
{
public:
	//exception thrown when there's a loading error.
	struct error {};

	struct collision_area {
		std::string name;
		rect area;

		//if this flag is set, then the entire area is considered to
		//collide, rather than just the pixels that have non-zero alpha.
		bool no_alpha_check;
	};

	static void build_patterns(variant obj_variant);

	static void setColor_palette(unsigned int palettes);

	explicit frame(variant node);
	~frame();

	//ID of the frame. Not unique, but is the name of the element the frame
	//came from. Useful to tell what kind of frame it is.
	const std::string& id() const { return id_; }
	const variant& variant_id() const { return variant_id_; }
	const std::string& image_name() const { return image_; }

	//play a sound. 'object' is just the address of the object playing the
	//sound, useful if the sound is later cancelled.
	void play_sound(const void* object=NULL) const;
	bool isAlpha(int x, int y, int time, bool face_right) const;

	//Low level interface to alpha information.
	std::vector<bool>::const_iterator getAlpha_itor(int x, int y, int time, bool face_right) const;
	const std::vector<bool>& getAlpha_buf() const { return alpha_; }

	void draw_into_blit_queue(graphics::blit_queue& blit, int x, int y, bool face_right=true, bool upside_down=false, int time=0) const;
	void draw(int x, int y, bool face_right=true, bool upside_down=false, int time=0, float rotate=0, KRE::Color=KRE::Color::colorWhite()) const;
	void draw(int x, int y, bool face_right, bool upside_down, int time, float rotate, float scale) const;
	void draw(int x, int y, const rect& area, bool face_right=true, bool upside_down=false, int time=0, float rotate=0) const;
	void draw3(int time, int va, int tc) const;

	struct CustomPoint {
		CustomPoint() : pos(0) {}
		float pos;
		point offset;
	};

	void draw_custom(int x, int y, const std::vector<CustomPoint>& points, const rect* area, bool face_right, bool upside_down, int time, float rotate) const;

	void draw_custom(int x, int y, const float* xy, const float* uv, int nelements, bool face_right, bool upside_down, int time, float rotate, int cycle) const;
	void set_image_as_solid();
	const_solid_info_ptr solid() const { return solid_; }
	int collide_x() const { return static_cast<int>(collide_rect_.x()*scale_); }
	int collide_y() const { return static_cast<int>(collide_rect_.y()*scale_); }
	int collide_w() const { return static_cast<int>(collide_rect_.w()*scale_); }
	int collide_h() const { return static_cast<int>(collide_rect_.h()*scale_); }
	int hit_x() const { return static_cast<int>(hit_rect_.x()*scale_); }
	int hit_y() const { return static_cast<int>(hit_rect_.y()*scale_); }
	int hit_w() const { return static_cast<int>(hit_rect_.w()*scale_); }
	int hit_h() const { return static_cast<int>(hit_rect_.h()*scale_); }
	int platform_x() const { return static_cast<int>(platform_rect_.x()*scale_); }
	int platform_y() const { return static_cast<int>(platform_rect_.y()*scale_); }
	int platform_w() const { return static_cast<int>(platform_rect_.w()*scale_); }
	bool has_platform() const { return platform_rect_.w() > 0; }
	int getFeetX() const { return static_cast<int>(feet_x_*scale_); }
	int getFeetY() const { return static_cast<int>(feet_y_*scale_); }
	int accel_x() const { return accel_x_; }
	int accel_y() const { return accel_y_; }
	int velocityX() const { return velocity_x_; }
	int velocityY() const { return velocity_y_; }
	int width() const { return static_cast<int>(img_rect_.w()*scale_); }
	int height() const { return static_cast<int>(img_rect_.h()*scale_); }
	int duration() const;
	bool hit(int time_in_frame) const;
	const KRE::MaterialPtr& img() const { return texture_; }
	const rect& area() const { return img_rect_; }
	int num_frames() const { return nframes_; }
	int num_frames_per_row() const { return nframes_per_row_ > 0 && nframes_per_row_ < nframes_ ? nframes_per_row_ : nframes_; }
	int pad() const { return pad_; }
	int blur() const { return blur_; }
	bool rotate_on_slope() const { return rotate_on_slope_; }
	int damage() const { return damage_; }

	const std::string* get_event(int time_in_frame) const;

	const std::vector<collision_area>& collision_areas() const { return collision_areas_; }
	bool collision_areas_inside_frame() const { return collision_areas_inside_frame_; }

	int enter_event_id() const { return enter_event_id_; }
	int end_event_id() const { return end_event_id_; }
	int leave_event_id() const { return leave_event_id_; }
	int processEvent_id() const { return processEvent_id_; }

	struct frame_info {
		frame_info() : x_adjust(0), y_adjust(0), x2_adjust(0), y2_adjust(0), draw_rect_init(false)
		{}
		int x_adjust, y_adjust, x2_adjust, y2_adjust;
		rect area;

		mutable bool draw_rect_init;
		mutable float draw_rect[4];
	};

	const std::vector<frame_info>& frame_layout() const { return frames_; }

	point pivot(const std::string& name, int time_in_frame) const;
	int frame_number(int time_in_frame) const;
private:

	void get_rect_in_texture(int time, float* output_rect, const frame_info*& info) const;
	void get_rect_in_frame_number(int nframe, float* output_rect, const frame_info*& info) const;
	std::string id_, image_;

	//ID as a variant, useful to be able to get a variant of the ID
	//very efficiently.
	variant variant_id_;

	//ID's used to signal events that occur on this animation.
	int enter_event_id_, end_event_id_, leave_event_id_, processEvent_id_;
	KRE::MaterialPtr texture_;
	const_solid_info_ptr solid_;
	rect collide_rect_;
	rect hit_rect_;
	rect img_rect_;

	std::vector<frame_info> frames_;

	rect platform_rect_;
	std::vector<int> hit_frames_;
	int platform_x_, platform_y_, platform_w_;
	int feet_x_, feet_y_;
	int accel_x_, accel_y_;
	int velocity_x_, velocity_y_;
	int nframes_;
	int nframes_per_row_;
	int frame_time_;
	bool reverse_frame_;
	bool play_backwards_;
	float scale_;
	int pad_;
	int rotate_;
	int blur_;
	bool rotate_on_slope_;
	int damage_;

	std::vector<int> event_frames_;
	std::vector<std::string> event_names_;
	std::vector <std::string> sounds_;

	std::vector<collision_area> collision_areas_;
	bool collision_areas_inside_frame_;

	void build_alpha_from_frame_info();
	void build_alpha();
	std::vector<bool> alpha_;
	bool force_no_alpha_;

	bool no_remove_alpha_borders_;

	std::vector<int> palettes_recognized_;
	int current_palette_;

	struct pivot_schedule {
		std::string name;
		std::vector<point> points;
	};

	std::vector<pivot_schedule> pivots_;

	void set_palettes(unsigned int palettes);

	variant getValue(const std::string& key) const;

	bool back_face_culling_;
};

#endif
