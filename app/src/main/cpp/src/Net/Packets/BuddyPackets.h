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
// Adds a buddy by name or updates an existing buddy's group.
// Opcode: BUDDYLIST_MODIFY(130), mode 1
class AddBuddyPacket : public OutPacket {
public:
    AddBuddyPacket(const std::string &name, const std::string &group) :
        OutPacket(OutPacket::Opcode::BUDDYLIST_MODIFY) {
        write_byte(1);
        write_string(name);
        write_string(group.empty() ? "Default Group" : group);
    }
};

// Accepts an incoming buddy request from another character.
// Opcode: BUDDYLIST_MODIFY(130), mode 2
class AcceptBuddyPacket : public OutPacket {
public:
    explicit AcceptBuddyPacket(int32_t cid_from) :
        OutPacket(OutPacket::Opcode::BUDDYLIST_MODIFY) {
        write_byte(2);
        write_int(cid_from);
    }
};

// Removes a buddy or rejects a pending request from another character.
// Opcode: BUDDYLIST_MODIFY(130), mode 3
class DeleteBuddyPacket : public OutPacket {
public:
    explicit DeleteBuddyPacket(int32_t other_cid) :
        OutPacket(OutPacket::Opcode::BUDDYLIST_MODIFY) {
        write_byte(3);
        write_int(other_cid);
    }
};
}  // namespace ms
