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
#include "NpcInteractionHandlers.h"

#include "../IO/UI.h"
#include "../IO/UITypes/UIChatBar.h"
#include "../IO/UITypes/UINpcTalk.h"
#include "../IO/UITypes/UIShop.h"
#include "../IO/UITypes/UIStorage.h"
#include "../../Util/Log.h"
#include "Helpers/ItemParser.h"

#include <map>
#include <vector>

namespace ms {
namespace {
// Reads the item infos that follow a storage packet.
std::vector<UIStorage::StorageItem> read_storage_items(InPacket &recv,
                                                       int count) {
    std::vector<UIStorage::StorageItem> items;
    Inventory temporary;

    for (int i = 0; i < count; i++) {
        auto parse_type = InventoryType::by_value(recv.inspect_byte());
        int16_t slot = int16_t(i + 1);

        ItemParser::parse_item(recv, parse_type, slot, temporary);

        int32_t itemid = temporary.get_item_id(parse_type, slot);
        int16_t item_count = temporary.get_item_count(parse_type, slot);
        InventoryType::Id real_type = InventoryType::by_item_id(itemid);
        if (real_type == InventoryType::Id::NONE) {
            real_type = parse_type;
        }

        std::optional<Equip> equip;
        if (parse_type == InventoryType::Id::EQUIP) {
            if (auto eq = temporary.get_equip(InventoryType::Id::EQUIP, slot)) {
                equip = eq->get();
            }
        }

        items.push_back({ real_type,
                          itemid,
                          item_count,
                          int16_t(i),
                          std::move(equip) });
    }

    return items;
}

// The packet encodes an inventory type as 2 << type.
InventoryType::Id type_from_encoding(int16_t encoding) {
    int8_t type = 0;

    while (encoding > 2) {
        encoding >>= 1;
        type++;
    }

    return InventoryType::by_value(type);
}

void replace_storage_items(UIStorage &storage,
                           std::vector<UIStorage::StorageItem> items) {
    std::map<InventoryType::Id, std::vector<UIStorage::StorageItem>> by_type;

    for (auto &item: items) {
        by_type[item.type].push_back(item);
    }

    for (auto &entry: by_type) {
        storage.set_items(entry.first, std::move(entry.second));
    }
}

void show_storage_message(const std::string &message) {
    MS_LOG_INFO << "storage: " << message;

    if (auto chatbar = UI::get().get_element<UIChatBar>()) {
        chatbar->get().send_chatline(message, UIChatBar::LineType::YELLOW);
    }
}
}  // namespace
void NpcDialogueHandler::handle(InPacket &recv) const {
    recv.skip(1);

    int32_t npcid = recv.read_int();
    int8_t msgtype = recv.read_byte();  // 0 - textonly, 1 - yes/no, 4 -
                                        // selection, 12 - accept/decline
    int8_t speaker = recv.read_byte();
    std::string text = recv.read_string();

    uint8_t style_b0 = 0;
    uint8_t style_b1 = 0;
    std::string default_text;
    int32_t default_number = 0;
    int32_t min_number = 0;
    int32_t max_number = 0;

    if (msgtype == 0 && recv.length() >= 2) {
        // {b0, b1}
        // {0, 1} = next
        // {1, 0} = prev
        // {1, 1} = nextPrev
        // {0, 0} = ok
        style_b0 = recv.read_ubyte();
        style_b1 = recv.read_ubyte();
    } else if (msgtype == 2 && recv.length() >= 2) {
        default_text = recv.read_string();
    } else if (msgtype == 3 && recv.length() >= 12) {
        default_number = recv.read_int();
        min_number = recv.read_int();
        max_number = recv.read_int();
    }

    UI::get().emplace<UINpcTalk>();
    UI::get().enable();

    MS_LOG_INFO << "NPC dialogue npc=" << npcid << " type=" << int32_t(msgtype)
                << " speaker=" << int32_t(speaker) << " styles="
                << int32_t(style_b0) << "," << int32_t(style_b1) << " len="
                << text.size();

    if (auto npctalk = UI::get().get_element<UINpcTalk>()) {
        npctalk->get().change_text(npcid,
                                   msgtype,
                                   style_b0,
                                   style_b1,
                                   speaker,
                                   text,
                                   default_text,
                                   default_number,
                                   min_number,
                                   max_number);
    }
}

void OpenNpcShopHandler::handle(InPacket &recv) const {
    int32_t npcid = recv.read_int();
    auto oshop = UI::get().get_element<UIShop>();

    if (!oshop) {
        return;
    }

    UIShop &shop = *oshop;

    shop.reset(npcid);

    int16_t size = recv.read_short();

    for (int i = 0; i < size; i++) {
        int32_t itemid = recv.read_int();
        int32_t price = recv.read_int();
        int32_t pitch = recv.read_int();
        int32_t time = recv.read_int();

        recv.skip(4);

        bool norecharge = recv.read_short() == 1;

        if (norecharge) {
            int16_t buyable = recv.read_short();

            shop.add_item(itemid, price, pitch, time, buyable);
        } else {
            recv.skip(4);

            int16_t rechargeprice = recv.read_short();
            int16_t slotmax = recv.read_short();

            shop.add_rechargable(itemid,
                                 price,
                                 pitch,
                                 time,
                                 rechargeprice,
                                 slotmax);
        }
    }
}

void ShopTransactionHandler::handle(InPacket &recv) const {
    int8_t code = recv.read_byte();

    // 0 means a purchase went through and 8 a sale or a recharge; either way the
    // inventory packet that follows updates the slots themselves.
    std::string message;

    switch (code) {
        case 0:
        case 8: return;
        case 1:
        case 5: message = "You don't have enough in stock."; break;
        case 2: message = "You do not have enough mesos."; break;
        case 3: message = "Please check if your inventory is full or not."; break;
        case 6:
        case 7: message = "Due to an error, the trade did not happen."; break;
        case 13: message = "You need more items."; break;
        default: message = "The trade did not happen."; break;
    }

    if (auto chatbar = UI::get().get_element<UIChatBar>()) {
        chatbar->get().send_chatline(message, UIChatBar::LineType::YELLOW);
    }
}

void StorageHandler::handle(InPacket &recv) const {
    int8_t mode = recv.read_byte();

    switch (mode) {
        case 0x16: {  // the storage is opened
            int32_t npcid = recv.read_int();
            uint8_t slots = recv.read_ubyte();

            recv.read_short();  // 0x7E
            recv.read_short();
            recv.read_int();

            int32_t meso = recv.read_int();

            recv.read_short();

            int count = recv.read_ubyte();

            UI::get().emplace<UIStorage>();

            auto element = UI::get().get_element<UIStorage>();

            if (!element) {
                return;
            }

            UIStorage &storage = element->get();

            storage.reset(npcid, slots, meso);
            replace_storage_items(storage, read_storage_items(recv, count));
        } break;
        case 0x09:    // items were taken out
        case 0x0D: {  // items were stored
            uint8_t slots = recv.read_ubyte();

            auto type = type_from_encoding(recv.read_short());

            recv.read_short();
            recv.read_int();

            int count = recv.read_ubyte();

            auto element = UI::get().get_element<UIStorage>();

            if (!element) {
                return;
            }

            element->get().set_slots(slots);
            element->get().set_items(type, read_storage_items(recv, count));
        } break;
        case 0x0F: {  // storage was arranged
            uint8_t slots = recv.read_ubyte();
            recv.skip(11);
            int count = recv.read_ubyte();

            auto element = UI::get().get_element<UIStorage>();

            if (!element) {
                return;
            }

            UIStorage &storage = element->get();
            storage.set_slots(slots);
            storage.clear_items();
            replace_storage_items(storage, read_storage_items(recv, count));
        } break;
        case 0x13: {  // the mesos of the storage changed
            recv.read_ubyte();
            recv.read_short();
            recv.read_short();
            recv.read_int();

            int32_t meso = recv.read_int();

            if (auto element = UI::get().get_element<UIStorage>()) {
                element->get().set_meso(meso);
            }
        } break;
        case 0x0A: show_storage_message("Your inventory is full."); break;
        case 0x0B: show_storage_message("You do not have enough mesos."); break;
        case 0x0C:
            show_storage_message("You can only have one of this item.");
            break;
        case 0x11: show_storage_message("The storage is full."); break;
        default: break;
    }

    recv.skip(recv.length());
}
}  // namespace ms