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
#include <nlnx/node.hpp>
#include <nlnx/nx.hpp>

#include "VirtualJoyStick.h"
#include "Singleton.h"
#include "KeyAction.h"
#include "Stage.h"
#include "UI.h"

namespace ms {
    // How far, in game pixels, a finger has to slide before it steers instead of
    // counting as a tap on whatever is under it.
    static constexpr double DRAG_THRESHOLD = 8.0;

    VirtualJoyStick::VirtualJoyStick(Point<int16_t> position, int16_t radius) :
            position_(position),
            background_(position.x(), position.y(), radius, Color::Name::BLACK, 0.535f) {
        radius_ = radius;
        angle_ = 0;
        bind_touch_id_ = -1;
    }

    void VirtualJoyStick::draw() const {
        background_.draw(position_);
    }

    void VirtualJoyStick::update() {
        const std::unordered_map<int16_t, TouchInfo> &touch_phase_map = UI::get().get_touch_phase();
        auto it = touch_phase_map.find(bind_touch_id_);
        if (it != touch_phase_map.end()) {
            GLFMTouchPhase current_phase = it->second.phase;
            if (dragging_ && (current_phase == GLFMTouchPhaseBegan ||
                current_phase == GLFMTouchPhaseMoved)) {
                if (angle_ < 135.0 && angle_ >= 45.0) {
                    UI::get().send_key(GLFMKeyCodeArrowDown, true);
                } else if (angle_ < 225.0 && angle_ >= 135.0) {
                    UI::get().send_key(GLFMKeyCodeArrowLeft, true);
                } else if (angle_ < 325.0 && angle_ >= 225.0) {
                    UI::get().send_key(GLFMKeyCodeArrowUp, true);
                } else {
                    UI::get().send_key(GLFMKeyCodeArrowRight, true);
                }
            } else if (current_phase == GLFMTouchPhaseEnded) {
                UI::get().remove_touch_phase(bind_touch_id_);
                bind_touch_id_ = -1;
                UI::get().send_key(GLFMKeyCodeArrowDown, false);
                UI::get().send_key(GLFMKeyCodeArrowLeft, false);
                UI::get().send_key(GLFMKeyCodeArrowUp, false);
                UI::get().send_key(GLFMKeyCodeArrowRight, false);
            }
        }
    }

    bool VirtualJoyStick::set_state(int16_t touch_id, TouchInfo touchInfo) {
        double distance_x = touchInfo.relative_pos.x() - position_.x();
        double distance_y = touchInfo.relative_pos.y() - position_.y();
        bool inside = distance_x * distance_x + distance_y * distance_y
                      < double(radius_) * radius_;

        if (touchInfo.phase == GLFMTouchPhaseBegan) {
            // Only a finger that starts inside the circle can steer, and only one
            // finger steers at a time.
            if (!inside || steering_id_ != -1) {
                return false;
            }

            steering_id_ = touch_id;
            begin_pos_ = touchInfo.relative_pos;
            dragging_ = false;
        }

        if (touch_id != steering_id_) {
            return false;
        }

        if (touchInfo.phase == GLFMTouchPhaseEnded) {
            steering_id_ = -1;
            return dragging_;
        }

        if (inside) {
            angle_ = atan2(distance_y, distance_x) * 180.0 / M_PI;
            if (angle_ < 0)
                angle_ += 360.0;
        }

        // A stationary finger is a tap on whatever is under it, not a drag.
        if (!dragging_) {
            double moved_x = touchInfo.relative_pos.x() - begin_pos_.x();
            double moved_y = touchInfo.relative_pos.y() - begin_pos_.y();
            dragging_ = moved_x * moved_x + moved_y * moved_y
                        >= DRAG_THRESHOLD * DRAG_THRESHOLD;
        }

        return true;
    }

    void VirtualJoyStick::bind_touch_id(int16_t touch_id) {
        bind_touch_id_ = touch_id;
    }

    int16_t VirtualJoyStick::get_bind_touch_id() {
        return bind_touch_id_;
    }

}  // namespace ms