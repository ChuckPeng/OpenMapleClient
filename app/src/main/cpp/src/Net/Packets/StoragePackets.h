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

#include "../../Character/Inventory/InventoryType.h"
#include "../OutPacket.h"

namespace ms {
// Packet which asks the server to operate the storage of the player
// Opcode: STORAGE(62)
class StoragePacket : public OutPacket {
public:
    enum Mode : int8_t {
        TAKE_OUT = 4,
        STORE = 5,
        ARRANGE = 6,
        MESO = 7,
        CLOSE = 8
    };

    explicit StoragePacket(Mode mode) :
        OutPacket(OutPacket::Opcode::STORAGE) {
        write_byte(mode);
    }

    // Takes an item out of the storage, by its index within the inventory
    // type it belongs to.
    void write_takeout(InventoryType::Id type, int16_t index) {
        write_byte(type);
        write_byte(index);
    }

    // Puts an item of the player inventory into the storage.
    void write_store(int16_t slot, int32_t itemid, int16_t count) {
        write_short(slot);
        write_int(itemid);
        write_short(count);
    }

    // Moves mesos: a positive amount is taken out of the storage.
    void write_meso(int32_t amount) { write_int(amount); }
};
}  // namespace ms
