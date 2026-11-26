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

#include "../PacketHandler.h"

namespace ms {
// TODO: Comment
class CheckSpwResultHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// TODO: Comment
class FieldEffectHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Toggles whether the player may report other characters
// Opcode: CLAIM_STATUS_CHANGED(47)
class ClaimStatusChangedHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Tells the client which world it was last connected to
// Opcode: LAST_CONNECTED_WORLD(26)
class LastConnectedWorldHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Tells the client the gender stored on the account
// Opcode: SET_GENDER(58)
class SetGenderHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Key bindings of the quickslot bar
// Opcode: QUICKSLOT_INIT(159)
class QuickslotInitHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Potion the server uses automatically when hp or mp run low
// Opcodes: AUTO_HP_POT(336), AUTO_MP_POT(337)
class AutoPotHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Map obstacle animation states sent on entering a map
// Opcode: FIELD_OBSTACLE_ONOFF_LIST(140)
class FieldObstacleOnOffListHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Tutorial UI lock toggle
// Opcode: LOCK_UI(221)
class LockUiHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Tutorial UI disable toggle
// Opcode: DISABLE_UI(222)
class DisableUiHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};

// Packets of systems the client does not show yet
// Opcodes: FAMILY_INFO_RESULT(95), FAMILY_PRIVILEGE_LIST(100), ADMIN_RESULT(144)
class IgnoredPacketHandler : public PacketHandler {
    void handle(InPacket &recv) const override;
};
}  // namespace ms