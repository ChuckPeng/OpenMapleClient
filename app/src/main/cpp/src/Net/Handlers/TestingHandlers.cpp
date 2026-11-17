//	This file is part of the continued Journey MMORPG client
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton
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
#include "TestingHandlers.h"

#include "../../Gameplay/Stage.h"
#include "../../IO/UI.h"
#include "../../IO/UITypes/Login/UILoginNotice.h"
#include "../../IO/UITypes/UIGaugeBoss.h"
#include "../../Util/Log.h"

namespace ms {
void CheckSpwResultHandler::handle(InPacket &recv) const {
    auto reason = recv.read_byte();

    if (reason == 0) {
        UI::get().emplace<UILoginNotice>(UILoginNotice::Message::INCORRECT_PIC);
    } else {
        MS_LOG_WARN << "[CheckSpwResultHandler]: Unknown reason: [" << reason
                    << "]";
    }

    UI::get().enable();
}

void FieldEffectHandler::handle(InPacket &recv) const {
    auto rand = recv.read_byte();

    // Effect
    if (rand == 3) {
        std::string path = recv.read_string();
        Stage::get().add_effect(path);
    } else if (rand == 5) {
        int32_t mob_id = recv.read_int();
        int32_t curr_hp = recv.read_int();
        int32_t max_hp = recv.read_int();
        uint8_t tag_color = recv.read_ubyte();
        uint8_t tag_bg_color = recv.read_ubyte();

        float percent = float(curr_hp) / max_hp;

        if (curr_hp == -1) {
            UI::get().remove(UIElement::Type::GAUGE_BOSS);
        } else {
            if (auto elem = UI::get().get_element<UIGaugeBoss>()) {
                elem->get().update(percent);
            } else {
                UI::get().emplace<UIGaugeBoss>(800,
                                               tag_color,
                                               tag_bg_color,
                                               mob_id,
                                               percent);
            }
        }
    } else {
        MS_LOG_WARN << "[FieldEffectHandler]: Unknown value: [" << rand << "]";
    }
}

void ClaimStatusChangedHandler::handle(InPacket &recv) const {
    recv.read_bool();  // whether reporting is enabled
}

void LastConnectedWorldHandler::handle(InPacket &recv) const {
    recv.read_int();  // world id
}

void SetGenderHandler::handle(InPacket &recv) const {
    recv.read_byte();  // gender, only used while creating a character
}

void QuickslotInitHandler::handle(InPacket &recv) const {
    if (!recv.read_bool()) {
        return;  // the bar keeps its default key mapping
    }

    for (int i = 0; i < 8; ++i) {
        recv.read_int();  // key bound to quickslot i
    }
}

void AutoPotHandler::handle(InPacket &recv) const {
    int32_t itemid = recv.read_int();

    MS_LOG_DEBUG << "auto pot item=" << itemid;
}

void IgnoredPacketHandler::handle(InPacket &recv) const {
    recv.skip(recv.length());
}
}  // namespace ms