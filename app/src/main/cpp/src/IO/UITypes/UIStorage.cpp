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
#include "UIStorage.h"

#include <nlnx/nx.hpp>

#include <algorithm>

#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/StoragePackets.h"
#include "../Components/MapleButton.h"

namespace ms {
UIStorage::UIStorage() :
    UIDragElement<PosSTORAGE>(),
    npcid_(0),
    slots_(0),
    meso_(0),
    player_meso_value_(-1) {
    nl::node src = nl::nx::ui["UIWindow2.img"]["Trunk"];

    nl::node backgrnd = src["backgrnd"];

    sprites_.emplace_back(backgrnd);
    sprites_.emplace_back(src["backgrnd2"]);
    sprites_.emplace_back(src["backgrnd3"]);

    buttons_[Buttons::EXIT] = std::make_unique<MapleButton>(src["BtExit"]);
    buttons_[Buttons::IN_COIN] = std::make_unique<MapleButton>(src["BtInCoin"]);
    buttons_[Buttons::OUT_COIN] = std::make_unique<MapleButton>(src["BtOutCoin"]);

    meso_input_ = Textfield(Text::Font::A11M,
                            Text::Alignment::LEFT,
                            Color::Name::BLACK,
                            Rectangle<int16_t>(Point<int16_t>(42, 383),
                                               Point<int16_t>(150, 399)),
                            10);

    title_ = Text(Text::Font::A11B,
                  Text::Alignment::LEFT,
                  Color::Name::WHITE,
                  "Storage");
    storage_meso_ = Text(Text::Font::A11M,
                         Text::Alignment::LEFT,
                         Color::Name::DARKGREY,
                         "",
                         120);
    placeholder_ = Text(Text::Font::A11M,
                        Text::Alignment::LEFT,
                        Color::Name::DUSTYGRAY,
                        "Amount");

    dimension_ = Texture(backgrnd).get_dimensions();
    drag_area_ = Point<int16_t>(dimension_.x(), 20);
}

void UIStorage::draw(float alpha) const {
    UIElement::draw_sprites(alpha);

    draw_cells(storage_cells_, LEFT_X_, LEFT_Y_);
    draw_cells(inventory_cells_, RIGHT_X_, RIGHT_Y_);

    title_.draw(position_ + Point<int16_t>(20, 20));
    storage_meso_.draw(position_ + Point<int16_t>(222, 385));

    if (meso_input_.get_state() == Textfield::State::NORMAL
        && meso_input_.empty()) {
        placeholder_.draw(position_ + Point<int16_t>(45, 384));
    }

    meso_input_.draw(position_);

    UIElement::draw_buttons(alpha);
}

void UIStorage::draw_cells(const std::vector<Cell> &cells,
                           int16_t left,
                           int16_t top) const {
    for (size_t i = 0; i < cells.size(); i++) {
        int16_t row = int16_t(i / COLS_);

        if (row >= ROWS_) {
            break;
        }

        const Cell &cell = cells[i];

        Point<int16_t> pos =
            position_
            + Point<int16_t>(left + int16_t(i % COLS_) * CELL_W_,
                             top + row * CELL_H_);

        const Texture &icon = ItemData::get(cell.itemid).get_icon(false);
        auto dimensions = icon.get_dimensions();

        // Item icons are anchored by their centre, so compensate for it to
        // place them inside the slot.
        icon.draw(pos
                  + icon.get_origin()
                  + Point<int16_t>((CELL_W_ - dimensions.x()) / 2,
                                   (CELL_H_ - dimensions.y()) / 2));

        if (cell.count > 1) {
            cell.text.draw(pos + Point<int16_t>(16, 18));
        }
    }
}

void UIStorage::update() {
    UIElement::update();
    refresh_inventory();
}

Cursor::State UIStorage::send_cursor(bool clicking, Point<int16_t> cursorpos) {
    if (Cursor::State state = meso_input_.send_cursor(cursorpos, clicking)) {
        return state;
    }

    if (clicking) {
        Point<int16_t> relative = cursorpos - position_;

        auto cell_at = [&](int16_t left,
                           int16_t top,
                           const std::vector<Cell> &cells) -> int16_t {
            int16_t x = relative.x() - left;
            int16_t y = relative.y() - top;

            if (x < 0 || y < 0) {
                return -1;
            }

            int16_t col = x / CELL_W_;
            int16_t row = y / CELL_H_;

            if (col >= COLS_ || row >= ROWS_) {
                return -1;
            }

            int16_t index = row * COLS_ + col;

            return index < int16_t(cells.size()) ? index : -1;
        };

        if (int16_t index = cell_at(LEFT_X_, LEFT_Y_, storage_cells_);
            index >= 0) {
            const Cell &cell = storage_cells_[index];

            take_item(cell.type, cell.reference);
        } else if (int16_t index = cell_at(RIGHT_X_,
                                           RIGHT_Y_,
                                           inventory_cells_);
                   index >= 0) {
            const Cell &cell = inventory_cells_[index];

            store_item(cell.type, cell.reference);
        }
    }

    meso_input_.update(position_);
    refresh_inventory();

    return UIDragElement::send_cursor(clicking, cursorpos);
}

void UIStorage::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed && escape) {
        close();
    }
}

UIElement::Type UIStorage::get_type() const {
    return TYPE;
}

void UIStorage::reset(int32_t npcid, uint8_t slots, int64_t meso) {
    npcid_ = npcid;
    slots_ = slots;

    items_.clear();
    storage_cells_.clear();

    set_meso(meso);
    refresh_inventory();
}

void UIStorage::set_items(InventoryType::Id type,
                          std::vector<StorageItem> items) {
    items_[type] = std::move(items);

    rebuild_storage_cells();
}

void UIStorage::set_meso(int64_t meso) {
    meso_ = meso;

    storage_meso_.change_text("Storage: " + std::to_string(meso)
                              + " mesos");
}

void UIStorage::refresh_inventory() {
    const Inventory &inventory = Stage::get().get_player().get_inventory();

    if (int64_t player_meso = inventory.get_meso(); player_meso
                                                     != player_meso_value_) {
        player_meso_value_ = player_meso;

        // The amount to move is typed into the left field, so its hint is the
        // amount of mesos the player owns.
        placeholder_.change_text("Mine: " + std::to_string(player_meso));
    }

    std::vector<int32_t> signature;

    for (int8_t t = InventoryType::Id::EQUIP; t <= InventoryType::Id::CASH;
         t++) {
        auto type = InventoryType::Id(t);
        int16_t slotmax = inventory.get_slotmax(type);

        for (int16_t slot = 1; slot <= slotmax; slot++) {
            int32_t itemid = inventory.get_item_id(type, slot);

            if (itemid == 0) {
                continue;
            }

            signature.push_back(int32_t(type) * 1000000 + slot);
            signature.push_back(itemid);
            signature.push_back(inventory.get_item_count(type, slot));
        }
    }

    if (signature == inventory_signature_) {
        return;
    }

    inventory_signature_ = std::move(signature);
    inventory_cells_.clear();

    for (int8_t t = InventoryType::Id::EQUIP; t <= InventoryType::Id::CASH;
         t++) {
        auto type = InventoryType::Id(t);
        int16_t slotmax = inventory.get_slotmax(type);

        for (int16_t slot = 1; slot <= slotmax; slot++) {
            int32_t itemid = inventory.get_item_id(type, slot);

            if (itemid == 0) {
                continue;
            }

            int16_t count = inventory.get_item_count(type, slot);

            inventory_cells_.push_back(
                { type,
                  itemid,
                  count,
                  slot,
                  Text(Text::Font::A11M,
                       Text::Alignment::LEFT,
                       Color::Name::BLACK,
                       std::to_string(count)) });
        }
    }
}

void UIStorage::rebuild_storage_cells() {
    storage_cells_.clear();

    for (int8_t t = InventoryType::Id::EQUIP; t <= InventoryType::Id::CASH;
         t++) {
        auto type = InventoryType::Id(t);
        const auto &list = items_[type];

        for (size_t i = 0; i < list.size(); i++) {
            const StorageItem &item = list[i];

            storage_cells_.push_back(
                { item.type,
                  item.itemid,
                  int16_t(item.count),
                  int16_t(i),
                  Text(Text::Font::A11M,
                       Text::Alignment::LEFT,
                       Color::Name::BLACK,
                       std::to_string(item.count)) });
        }
    }
}

void UIStorage::store_item(InventoryType::Id type, int16_t slot) {
    const Inventory &inventory = Stage::get().get_player().get_inventory();

    int32_t itemid = inventory.get_item_id(type, slot);
    int16_t count = inventory.get_item_count(type, slot);

    if (itemid == 0 || count <= 0) {
        return;
    }

    StoragePacket packet(StoragePacket::STORE);

    packet.write_store(slot, itemid, count);
    packet.dispatch();
}

void UIStorage::take_item(InventoryType::Id type, int16_t index) {
    StoragePacket packet(StoragePacket::TAKE_OUT);

    packet.write_takeout(type, index);
    packet.dispatch();
}

void UIStorage::move_meso(bool into_storage) {
    int32_t amount;

    try {
        amount = std::stoi(meso_input_.get_text());
    } catch (const std::exception &) {
        return;
    }

    if (amount <= 0) {
        return;
    }

    StoragePacket packet(StoragePacket::MESO);

    packet.write_meso(into_storage ? -amount : amount);
    packet.dispatch();

    meso_input_.change_text("");
}

void UIStorage::close() {
    StoragePacket packet(StoragePacket::CLOSE);

    packet.dispatch();
    deactivate();
}

Button::State UIStorage::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case Buttons::EXIT: close(); break;
        case Buttons::IN_COIN: move_meso(true); break;
        case Buttons::OUT_COIN: move_meso(false); break;
        default: break;
    }

    return Button::State::NORMAL;
}
}  // namespace ms
