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
#pragma once

#include "../OutPacket.h"

namespace ms {
// Packet which asks the server to perform an operation on a quest
// Opcode: QUEST_ACTION(107)
class QuestActionPacket : public OutPacket {
public:
    enum Action : int8_t {
        RESTORE_LOST_ITEM,
        START,
        COMPLETE,
        FORFEIT,
        SCRIPTED_START,
        SCRIPTED_END
    };

    QuestActionPacket(Action action, int16_t questid) :
        OutPacket(OutPacket::Opcode::QUEST_ACTION) {
        write_byte(action);
        write_short(questid);
    }

    // The server checks the distance to the npc against the position of the
    // player that follows it, so both have to be written for actions involving
    // an npc.
    void write_npc(int32_t npcid, Point<int16_t> playerpos) {
        write_int(npcid);
        write_short(playerpos.x());
        write_short(playerpos.y());
    }

    void write_selection(int16_t selection) {
        write_short(selection);
    }
};
}  // namespace ms
