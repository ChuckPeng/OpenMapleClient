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
#include <limits>

#include "../../Data/ItemData.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/StoragePackets.h"
#include "../../Util/StringHandling.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"
#include "../UI.h"
#include "UIChatBar.h"
#include "UINotice.h"

namespace ms {
namespace {
bool is_rechargeable(int32_t itemid) {
    int32_t prefix = itemid / 10000;
    return prefix == 207 || prefix == 233;
}
}  // namespace

UIStorage::UIStorage() :
    UIDragElement<PosSTORAGE>(),
    npcid_(0),
    slots_(0),
    meso_(0),
    player_meso_value_(-1),
    tab_(InventoryType::Id::EQUIP),
    cached_inv_tab_(InventoryType::Id::NONE),
    inv_slotmax_(24),
    storage_row_offset_(0),
    inv_row_offset_(0),
    selected_side_(GridSide::NONE),
    selected_ref_(-1) {
    nl::node src = nl::nx::ui["UIWindow2.img"]["Trunk"];

    nl::node backgrnd = src["backgrnd"];

    sprites_.emplace_back(backgrnd);
    sprites_.emplace_back(src["backgrnd2"]);
    sprites_.emplace_back(src["backgrnd3"]);

    disabled_ = src["disabled"];

    buttons_[Buttons::EXIT] = std::make_unique<MapleButton>(src["BtExit"]);
    buttons_[Buttons::GET_ITEM] = std::make_unique<MapleButton>(src["BtGet"]);
    buttons_[Buttons::SORT] = std::make_unique<MapleButton>(src["BtSort"]);
    buttons_[Buttons::SLOT_EX] =
        std::make_unique<MapleButton>(src["BtSlotEx"]);
    buttons_[Buttons::PUT_ITEM] = std::make_unique<MapleButton>(src["BtPut"]);
    buttons_[Buttons::IN_COIN] = std::make_unique<MapleButton>(src["BtInCoin"]);
    buttons_[Buttons::OUT_COIN] =
        std::make_unique<MapleButton>(src["BtOutCoin"]);

    buttons_[Buttons::SLOT_EX]->set_state(Button::State::DISABLED);

    nl::node tab = src["Tab"];
    nl::node taben = tab["enabled"];
    nl::node tabdis = tab["disabled"];

    buttons_[Buttons::TAB_EQUIP] =
        std::make_unique<TwoSpriteButton>(tabdis["0"], taben["0"]);
    buttons_[Buttons::TAB_USE] =
        std::make_unique<TwoSpriteButton>(tabdis["1"], taben["1"]);
    buttons_[Buttons::TAB_ETC] =
        std::make_unique<TwoSpriteButton>(tabdis["2"], taben["2"]);
    buttons_[Buttons::TAB_SETUP] =
        std::make_unique<TwoSpriteButton>(tabdis["3"], taben["3"]);
    buttons_[Buttons::TAB_CASH] =
        std::make_unique<TwoSpriteButton>(tabdis["4"], taben["4"]);

    buttons_[button_by_tab(tab_)]->set_state(Button::State::PRESSED);

    storage_slider_ = Slider(
        Slider::Type::DEFAULT_SILVER,
        Range<int16_t>(SCROLL_TOP_, SCROLL_BOTTOM_),
        LEFT_SCROLL_X_,
        ROWS_,
        ROWS_,
        [this](bool upwards) {
            int16_t total_rows = std::max<int16_t>(
                ROWS_,
                (std::max<int16_t>(int16_t(slots_),
                                   int16_t(storage_cells_.size()))
                 + COLS_ - 1)
                    / COLS_);
            int16_t max_offset = std::max<int16_t>(0, total_rows - ROWS_);
            int16_t next = storage_row_offset_ + (upwards ? -1 : 1);
            storage_row_offset_ = std::clamp<int16_t>(next, 0, max_offset);
        });
    storage_slider_.setenabled(false);

    inv_slider_ = Slider(
        Slider::Type::DEFAULT_SILVER,
        Range<int16_t>(SCROLL_TOP_, SCROLL_BOTTOM_),
        RIGHT_SCROLL_X_,
        ROWS_,
        ROWS_,
        [this](bool upwards) {
            int16_t total_rows =
                std::max<int16_t>(ROWS_, (inv_slotmax_ + COLS_ - 1) / COLS_);
            int16_t max_offset = std::max<int16_t>(0, total_rows - ROWS_);
            int16_t next = inv_row_offset_ + (upwards ? -1 : 1);
            inv_row_offset_ = std::clamp<int16_t>(next, 0, max_offset);
        });
    inv_slider_.setenabled(false);

    meso_input_ = Textfield(Text::Font::A11M,
                            Text::Alignment::LEFT,
                            Color::Name::BLACK,
                            Rectangle<int16_t>(Point<int16_t>(47, 383),
                                               Point<int16_t>(159, 399)),
                            10);

    storage_meso_ = Text(Text::Font::A11M,
                         Text::Alignment::LEFT,
                         Color::Name::DARKGREY,
                         "",
                         112);
    placeholder_ = Text(Text::Font::A11M,
                        Text::Alignment::LEFT,
                        Color::Name::DUSTYGRAY,
                        "Amount",
                        112);

    dimension_ = Texture(backgrnd).get_dimensions();
    drag_area_ = Point<int16_t>(dimension_.x(), 12);
}

void UIStorage::draw(float alpha) const {
    UIElement::draw_sprites(alpha);

    if (npc_.width() > 0 && npc_.height() > 0) {
        npc_.draw(DrawArgument(position_ + Point<int16_t>(48, 86), true));
    }

    draw_grid_cells(nullptr, true);
    draw_grid_cells(&inventory_cells_, false);

    storage_slider_.draw(position_);
    inv_slider_.draw(position_);

    storage_meso_.draw(position_ + Point<int16_t>(223, 384));

    if (meso_input_.get_state() == Textfield::State::NORMAL
        && meso_input_.empty()) {
        placeholder_.draw(position_ + Point<int16_t>(48, 384));
    }

    meso_input_.draw(position_);

    UIElement::draw_buttons(alpha);
}

void UIStorage::draw_grid_cells(const std::map<int16_t, Cell> *inv_cells,
                                bool storage_side) const {
    int16_t left = storage_side ? LEFT_X_ : RIGHT_X_;
    int16_t top = storage_side ? LEFT_Y_ : RIGHT_Y_;
    int16_t row_offset = storage_side ? storage_row_offset_ : inv_row_offset_;

    for (int16_t vis = 0; vis < PAGE_SIZE_; ++vis) {
        Point<int16_t> pos =
            position_
            + Point<int16_t>(left + (vis % COLS_) * CELL_W_,
                             top + (vis / COLS_) * CELL_H_);

        const Cell *cell = nullptr;
        bool is_selected = false;
        bool is_disabled = false;

        if (storage_side) {
            int16_t abs_idx = row_offset * COLS_ + vis;

            if (abs_idx < int16_t(storage_cells_.size())) {
                cell = &storage_cells_[abs_idx];
                is_selected = (selected_side_ == GridSide::STORAGE
                               && selected_ref_ == abs_idx);
            } else if (abs_idx >= slots_) {
                is_disabled = true;
            }
        } else {
            int16_t slot = row_offset * COLS_ + vis + 1;

            auto it = inv_cells->find(slot);
            if (it != inv_cells->end()) {
                cell = &it->second;
                is_selected = (selected_side_ == GridSide::INVENTORY
                               && selected_ref_ == slot);
            } else if (slot > inv_slotmax_) {
                is_disabled = true;
            }
        }

        if (is_disabled) {
            disabled_.draw(pos);
            continue;
        }

        if (!cell) {
            continue;
        }

        if (is_selected) {
            GraphicsGL::get().draw_rectangle(pos.x(),
                                             pos.y(),
                                             SLOT_BOX_SIZE_,
                                             SLOT_BOX_SIZE_,
                                             1.0f,
                                             0.72f,
                                             0.15f,
                                             0.30f);
        }

        const Texture &icon = ItemData::get(cell->itemid).get_icon(false);
        auto dimensions = icon.get_dimensions();

        // Item icons are anchored by their centre, so compensate for it to
        // place them inside the 32x32 slot box.
        icon.draw(pos
                  + icon.get_origin()
                  + Point<int16_t>((SLOT_BOX_SIZE_ - dimensions.x()) / 2,
                                   (SLOT_BOX_SIZE_ - dimensions.y()) / 2));

        if (cell->type != InventoryType::Id::EQUIP
            && (cell->count > 1 || is_rechargeable(cell->itemid))) {
            cell->text.draw(pos + Point<int16_t>(2, 18));
        }
    }
}

void UIStorage::update() {
    UIElement::update();
    refresh_inventory();
}

void UIStorage::remove_cursor() {
    UIDragElement::remove_cursor();
    storage_slider_.remove_cursor();
    inv_slider_.remove_cursor();
    clear_tooltip();
}

Cursor::State UIStorage::send_cursor(bool clicking, Point<int16_t> cursorpos) {
    if (dragged_) {
        clear_tooltip();
        return UIDragElement::send_cursor(clicking, cursorpos);
    }

    last_cursor_relative_ = cursorpos - position_;

    if (Cursor::State state = meso_input_.send_cursor(cursorpos, clicking)) {
        clear_tooltip();
        return state;
    }

    if (storage_slider_.isenabled()) {
        Cursor::State sstate =
            storage_slider_.send_cursor(last_cursor_relative_, clicking);

        if (sstate != Cursor::State::IDLE) {
            clear_tooltip();
            return sstate;
        }
    }

    if (inv_slider_.isenabled()) {
        Cursor::State sstate =
            inv_slider_.send_cursor(last_cursor_relative_, clicking);

        if (sstate != Cursor::State::IDLE) {
            clear_tooltip();
            return sstate;
        }
    }

    int16_t left_vis = visible_cell_at(LEFT_X_, LEFT_Y_, last_cursor_relative_);
    if (left_vis >= 0) {
        int16_t abs_idx = storage_row_offset_ * COLS_ + left_vis;

        if (abs_idx >= 0 && abs_idx < int16_t(storage_cells_.size())) {
            const Cell &cell = storage_cells_[abs_idx];

            if (clicking) {
                if (selected_side_ == GridSide::STORAGE
                    && selected_ref_ == abs_idx) {
                    selected_side_ = GridSide::NONE;
                    selected_ref_ = -1;
                    clear_tooltip();
                    take_item(cell.type, cell.reference);
                } else {
                    selected_side_ = GridSide::STORAGE;
                    selected_ref_ = abs_idx;
                    show_cell_tooltip(GridSide::STORAGE, abs_idx);
                }
                return Cursor::State::CLICKING;
            }

            UIElement::remove_cursor();
            show_cell_tooltip(GridSide::STORAGE, abs_idx);
            return Cursor::State::CAN_CLICK;
        }
    }

    int16_t right_vis =
        visible_cell_at(RIGHT_X_, RIGHT_Y_, last_cursor_relative_);
    if (right_vis >= 0) {
        int16_t slot = inv_row_offset_ * COLS_ + right_vis + 1;

        if (inventory_cells_.count(slot)) {
            if (clicking) {
                if (selected_side_ == GridSide::INVENTORY
                    && selected_ref_ == slot) {
                    selected_side_ = GridSide::NONE;
                    selected_ref_ = -1;
                    clear_tooltip();
                    store_item(tab_, slot);
                } else {
                    selected_side_ = GridSide::INVENTORY;
                    selected_ref_ = slot;
                    show_cell_tooltip(GridSide::INVENTORY, slot);
                }
                return Cursor::State::CLICKING;
            }

            UIElement::remove_cursor();
            show_cell_tooltip(GridSide::INVENTORY, slot);
            return Cursor::State::CAN_CLICK;
        }
    }

    clear_tooltip();
    meso_input_.update(position_);
    refresh_inventory();

    return UIDragElement::send_cursor(clicking, cursorpos);
}

void UIStorage::send_scroll(double yoffset) {
    if (storage_slider_.isenabled()
        && last_cursor_relative_.x() < RIGHT_X_ - 4) {
        storage_slider_.send_scroll(yoffset);
    } else if (inv_slider_.isenabled()
               && last_cursor_relative_.x() >= RIGHT_X_ - 4) {
        inv_slider_.send_scroll(yoffset);
    }
}

void UIStorage::send_key(int32_t keycode, bool pressed, bool escape) {
    if (!pressed) {
        return;
    }

    if (escape) {
        close();
        return;
    }

    if (keycode == KeyAction::Id::TAB) {
        InventoryType::Id next_tab = InventoryType::Id::EQUIP;

        switch (tab_) {
            case InventoryType::Id::EQUIP:
                next_tab = InventoryType::Id::USE;
                break;
            case InventoryType::Id::USE:
                next_tab = InventoryType::Id::ETC;
                break;
            case InventoryType::Id::ETC:
                next_tab = InventoryType::Id::SETUP;
                break;
            case InventoryType::Id::SETUP:
                next_tab = InventoryType::Id::CASH;
                break;
            default: next_tab = InventoryType::Id::EQUIP; break;
        }

        change_tab(next_tab);
    }
}

UIElement::Type UIStorage::get_type() const {
    return TYPE;
}

void UIStorage::reset(int32_t npcid, uint8_t slots, int64_t meso) {
    npcid_ = npcid;
    slots_ = slots;
    storage_row_offset_ = 0;
    inv_row_offset_ = 0;
    selected_side_ = GridSide::NONE;
    selected_ref_ = -1;

    std::string strid = string_format::extend_id(npcid_, 7) + ".img";
    nl::node npc_node = nl::nx::npc[strid];
    if (npc_node && npc_node["stand"] && npc_node["stand"]["0"]) {
        npc_ = npc_node["stand"]["0"];
    } else {
        npc_ = Texture();
    }

    buttons_[Buttons::SLOT_EX]->set_state(Button::State::DISABLED);

    items_.clear();
    storage_cells_.clear();
    cached_inv_tab_ = InventoryType::Id::NONE;

    set_meso(meso);
    update_storage_slider();
    change_tab(InventoryType::Id::EQUIP);
    refresh_inventory();
}

void UIStorage::set_slots(uint8_t slots) {
    slots_ = slots;
    update_storage_slider();
}

void UIStorage::clear_items() {
    items_.clear();
    storage_cells_.clear();
    selected_side_ = GridSide::NONE;
    selected_ref_ = -1;
    update_storage_slider();
}

void UIStorage::set_items(InventoryType::Id type,
                          std::vector<StorageItem> items) {
    items_[type] = std::move(items);

    if (selected_side_ == GridSide::STORAGE) {
        selected_side_ = GridSide::NONE;
        selected_ref_ = -1;
    }

    rebuild_storage_cells();
}

void UIStorage::set_meso(int64_t meso) {
    meso_ = meso;

    std::string formatted = std::to_string(meso);
    string_format::split_number(formatted);
    storage_meso_.change_text("Stg: " + formatted);
}

void UIStorage::change_tab(InventoryType::Id type) {
    if (type == tab_ && cached_inv_tab_ == type) {
        return;
    }

    clear_tooltip();

    if (selected_side_ == GridSide::INVENTORY) {
        selected_side_ = GridSide::NONE;
        selected_ref_ = -1;
    }

    buttons_[button_by_tab(tab_)]->set_state(Button::State::NORMAL);
    tab_ = type;
    buttons_[button_by_tab(tab_)]->set_state(Button::State::PRESSED);

    inv_row_offset_ = 0;
    refresh_inventory();
}

uint16_t UIStorage::button_by_tab(InventoryType::Id type) const {
    switch (type) {
        case InventoryType::Id::EQUIP: return Buttons::TAB_EQUIP;
        case InventoryType::Id::USE: return Buttons::TAB_USE;
        case InventoryType::Id::ETC: return Buttons::TAB_ETC;
        case InventoryType::Id::SETUP: return Buttons::TAB_SETUP;
        case InventoryType::Id::CASH: return Buttons::TAB_CASH;
        default: return Buttons::TAB_EQUIP;
    }
}

void UIStorage::refresh_inventory() {
    const Inventory &inventory = Stage::get().get_player().get_inventory();

    if (int64_t player_meso = inventory.get_meso();
        player_meso != player_meso_value_) {
        player_meso_value_ = player_meso;

        std::string formatted = std::to_string(player_meso);
        string_format::split_number(formatted);
        placeholder_.change_text("Mine: " + formatted);
    }

    uint8_t slotmax = inventory.get_slotmax(tab_);
    std::vector<int32_t> signature;
    signature.push_back(int32_t(tab_));
    signature.push_back(int32_t(slotmax));

    for (int16_t slot = 1; slot <= slotmax; slot++) {
        int32_t itemid = inventory.get_item_id(tab_, slot);

        if (itemid == 0) {
            continue;
        }

        signature.push_back(slot);
        signature.push_back(itemid);
        signature.push_back(inventory.get_item_count(tab_, slot));
    }

    if (signature == inventory_signature_ && cached_inv_tab_ == tab_) {
        return;
    }

    cached_inv_tab_ = tab_;
    inv_slotmax_ = slotmax;
    inventory_signature_ = std::move(signature);
    inventory_cells_.clear();

    for (int16_t slot = 1; slot <= slotmax; slot++) {
        int32_t itemid = inventory.get_item_id(tab_, slot);

        if (itemid == 0) {
            continue;
        }

        int16_t count = inventory.get_item_count(tab_, slot);

        inventory_cells_.emplace(
            slot,
            Cell{ tab_,
                  itemid,
                  count,
                  slot,
                  std::nullopt,
                  Text(Text::Font::A11M,
                       Text::Alignment::LEFT,
                       Color::Name::BLACK,
                       std::to_string(count)) });
    }

    if (selected_side_ == GridSide::INVENTORY
        && inventory_cells_.count(selected_ref_) == 0) {
        selected_side_ = GridSide::NONE;
        selected_ref_ = -1;
        clear_tooltip();
    }

    update_inventory_slider();
}

void UIStorage::rebuild_storage_cells() {
    storage_cells_.clear();

    for (int8_t t = InventoryType::Id::EQUIP; t <= InventoryType::Id::CASH;
         t++) {
        auto type = InventoryType::Id(t);
        const auto &list = items_[type];

        for (const StorageItem &item : list) {
            storage_cells_.push_back(
                { item.type,
                  item.itemid,
                  int16_t(item.count),
                  item.index,
                  item.equip,
                  Text(Text::Font::A11M,
                       Text::Alignment::LEFT,
                       Color::Name::BLACK,
                       std::to_string(item.count)) });
        }
    }

    update_storage_slider();
}

void UIStorage::update_storage_slider() {
    int16_t total_items = std::max<int16_t>(int16_t(slots_),
                                            int16_t(storage_cells_.size()));
    int16_t total_rows =
        std::max<int16_t>(ROWS_, (total_items + COLS_ - 1) / COLS_);
    int16_t max_offset = std::max<int16_t>(0, total_rows - ROWS_);

    storage_row_offset_ =
        std::clamp<int16_t>(storage_row_offset_, 0, max_offset);
    storage_slider_.setrows(storage_row_offset_, ROWS_, total_rows);
    storage_slider_.setenabled(total_rows > ROWS_);
}

void UIStorage::update_inventory_slider() {
    int16_t total_rows =
        std::max<int16_t>(ROWS_, (inv_slotmax_ + COLS_ - 1) / COLS_);
    int16_t max_offset = std::max<int16_t>(0, total_rows - ROWS_);

    inv_row_offset_ = std::clamp<int16_t>(inv_row_offset_, 0, max_offset);
    inv_slider_.setrows(inv_row_offset_, ROWS_, total_rows);
    inv_slider_.setenabled(total_rows > ROWS_);
}

void UIStorage::clear_tooltip() const {
    UI::get().clear_tooltip(Tooltip::Parent::STORAGE);
}

void UIStorage::show_cell_tooltip(GridSide side, int16_t ref) const {
    if (side == GridSide::STORAGE) {
        if (ref < 0 || ref >= int16_t(storage_cells_.size())) {
            return;
        }

        const Cell &cell = storage_cells_[ref];

        if (cell.type == InventoryType::Id::EQUIP && cell.equip) {
            UI::get().show_equip(Tooltip::Parent::STORAGE,
                                 *cell.equip,
                                 static_cast<int16_t>(-1 - ref));
        } else {
            UI::get().show_item(Tooltip::Parent::STORAGE, cell.itemid);
        }
    } else if (side == GridSide::INVENTORY) {
        auto it = inventory_cells_.find(ref);
        if (it == inventory_cells_.end()) {
            return;
        }

        if (tab_ == InventoryType::Id::EQUIP) {
            UI::get().show_equip(Tooltip::Parent::STORAGE, ref);
        } else {
            UI::get().show_item(Tooltip::Parent::STORAGE, it->second.itemid);
        }
    }
}

int16_t UIStorage::visible_cell_at(int16_t left,
                                   int16_t top,
                                   Point<int16_t> relative) const {
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

    return row * COLS_ + col;
}

void UIStorage::store_item(InventoryType::Id type, int16_t slot) {
    const Inventory &inventory = Stage::get().get_player().get_inventory();

    int32_t itemid = inventory.get_item_id(type, slot);
    int16_t count = inventory.get_item_count(type, slot);

    if (itemid == 0 || count <= 0) {
        return;
    }

    if (type != InventoryType::Id::EQUIP && !is_rechargeable(itemid)
        && count > 1) {
        clear_tooltip();
        meso_input_.set_state(Textfield::State::NORMAL);
        UI::get().emplace<UIEnterNumber>(
            "How many will you store?",
            [slot, itemid](int32_t qty) {
                if (qty <= 0) {
                    return;
                }

                StoragePacket packet(StoragePacket::STORE);
                packet.write_store(slot, itemid, static_cast<int16_t>(qty));
                packet.dispatch();
            },
            count,
            count);
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
    if (!meso_input_.empty()) {
        int64_t amount = 0;

        try {
            amount = std::stoll(meso_input_.get_text());
        } catch (const std::exception &) {
            return;
        }

        if (amount <= 0 || amount > std::numeric_limits<int32_t>::max()) {
            return;
        }

        StoragePacket packet(StoragePacket::MESO);

        packet.write_meso(into_storage ? -static_cast<int32_t>(amount)
                                       : static_cast<int32_t>(amount));
        packet.dispatch();

        meso_input_.change_text("");
        meso_input_.set_state(Textfield::State::NORMAL);
        return;
    }

    int64_t available =
        into_storage ? Stage::get().get_player().get_inventory().get_meso()
                     : meso_;
    int32_t max_amount = static_cast<int32_t>(
        std::min<int64_t>(available, std::numeric_limits<int32_t>::max()));

    if (max_amount <= 0) {
        if (auto chatbar = UI::get().get_element<UIChatBar>()) {
            chatbar->get().send_chatline("You do not have enough mesos.",
                                         UIChatBar::LineType::YELLOW);
        }
        return;
    }

    clear_tooltip();
    UI::get().emplace<UIEnterNumber>(
        into_storage ? "How much will you deposit?"
                     : "How much will you withdraw?",
        [into_storage](int32_t amount) {
            if (amount <= 0) {
                return;
            }

            StoragePacket packet(StoragePacket::MESO);
            packet.write_meso(into_storage ? -amount : amount);
            packet.dispatch();
        },
        max_amount,
        max_amount);
}

void UIStorage::close() {
    clear_tooltip();
    meso_input_.set_state(Textfield::State::NORMAL);
    selected_side_ = GridSide::NONE;
    selected_ref_ = -1;

    StoragePacket packet(StoragePacket::CLOSE);

    packet.dispatch();
    deactivate();
}

Button::State UIStorage::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case Buttons::EXIT: close(); break;
        case Buttons::GET_ITEM:
            if (selected_side_ == GridSide::STORAGE && selected_ref_ >= 0
                && selected_ref_ < int16_t(storage_cells_.size())) {
                const Cell &cell = storage_cells_[selected_ref_];
                InventoryType::Id type = cell.type;
                int16_t ref = cell.reference;
                selected_side_ = GridSide::NONE;
                selected_ref_ = -1;
                clear_tooltip();
                take_item(type, ref);
            }
            break;
        case Buttons::SORT:
            clear_tooltip();
            selected_side_ = GridSide::NONE;
            selected_ref_ = -1;
            StoragePacket(StoragePacket::ARRANGE).dispatch();
            break;
        case Buttons::PUT_ITEM:
            if (selected_side_ == GridSide::INVENTORY && selected_ref_ > 0) {
                int16_t slot = selected_ref_;
                selected_side_ = GridSide::NONE;
                selected_ref_ = -1;
                clear_tooltip();
                store_item(tab_, slot);
            }
            break;
        case Buttons::IN_COIN: move_meso(true); break;
        case Buttons::OUT_COIN: move_meso(false); break;
        case Buttons::TAB_EQUIP:
            change_tab(InventoryType::Id::EQUIP);
            return Button::State::IDENTITY;
        case Buttons::TAB_USE:
            change_tab(InventoryType::Id::USE);
            return Button::State::IDENTITY;
        case Buttons::TAB_ETC:
            change_tab(InventoryType::Id::ETC);
            return Button::State::IDENTITY;
        case Buttons::TAB_SETUP:
            change_tab(InventoryType::Id::SETUP);
            return Button::State::IDENTITY;
        case Buttons::TAB_CASH:
            change_tab(InventoryType::Id::CASH);
            return Button::State::IDENTITY;
        default: break;
    }

    return Button::State::NORMAL;
}
}  // namespace ms
