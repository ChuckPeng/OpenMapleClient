//	This file is part of the continued Journey MMORPG client
//	Copyright (C) 2015-2024  Daniel Allendorf, Ryan Payton, Bizhou Xing
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Affero General Public License for more details.
//
//	You should have received a copy of the GNU Affero General Public License
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once
#include "IconCover.h"
#include "Button.h"
#include "TouchInfo.h"

namespace ms {
class VirtualJoyStick {
public:
    VirtualJoyStick(Point<int16_t> position, int16_t radius);

    void draw() const;

    void update();

    // Returns whether the touch is steering the joystick. A touch that begins
    // inside the circle but never moves is a tap, so it is left to the map.
    bool set_state(int16_t touch_id, TouchInfo touchInfo);

    bool is_dragging() const { return dragging_; }

    void bind_touch_id(int16_t touch_id);

    int16_t get_bind_touch_id();

private:
    Point<int16_t> position_;
    ColorCircle background_;
    int16_t radius_;
    int16_t bind_touch_id_;
    int16_t steering_id_ = -1;
    Point<double_t> begin_pos_;
    bool dragging_ = false;
    double angle_;
};
}  // namespace ms