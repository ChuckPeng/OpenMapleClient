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

#include <map>
#include <string>
#include <vector>

#include "../../Character/Inventory/InventoryType.h"
#include "../Components/Textfield.h"
#include "../UIDragElement.h"

namespace ms {
// The window of an npc storage: the storage itself on the left, the
// inventory of the player on the right.
class UIStorage : public UIDragElement<PosSTORAGE> {
public:
    static constexpr Type TYPE = UIElement::Type::STORAGE;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = false;

    struct StorageItem {
        InventoryType::Id type;
        int32_t itemid;
        int32_t count;
    };

    UIStorage();

    void draw(float alpha) const override;

    void update() override;

    Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    UIElement::Type get_type() const override;

    // Opens the window with the contents the server sent.
    void reset(int32_t npcid, uint8_t slots, int64_t meso);

    // Replaces the items of one inventory type after a store or take out.
    void set_items(InventoryType::Id type, std::vector<StorageItem> items);

    void set_meso(int64_t meso);

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    enum Buttons : uint16_t { EXIT, IN_COIN, OUT_COIN };

    static constexpr int16_t LEFT_X_ = 11;
    static constexpr int16_t RIGHT_X_ = 184;
    static constexpr int16_t LEFT_Y_ = 100;
    static constexpr int16_t RIGHT_Y_ = 108;
    static constexpr int16_t CELL_W_ = 36;
    static constexpr int16_t CELL_H_ = 35;
    static constexpr int16_t COLS_ = 4;
    static constexpr int16_t ROWS_ = 8;

    // One item in a grid. The reference is the slot of the inventory or the
    // index within the type for the storage.
    struct Cell {
        InventoryType::Id type;
        int32_t itemid;
        int16_t count;
        int16_t reference;
        Text text;
    };

    void close();

    void store_item(InventoryType::Id type, int16_t slot);

    void take_item(InventoryType::Id type, int16_t index);

    void move_meso(bool into_storage);

    void refresh_inventory();

    void rebuild_storage_cells();

    void draw_cells(const std::vector<Cell> &cells,
                    int16_t left,
                    int16_t top) const;

    int32_t npcid_;
    uint8_t slots_;
    int64_t meso_;
    int64_t player_meso_value_;
    std::map<InventoryType::Id, std::vector<StorageItem>> items_;
    std::vector<Cell> storage_cells_;
    std::vector<Cell> inventory_cells_;
    std::vector<int32_t> inventory_signature_;
    Textfield meso_input_;
    Text title_;
    Text storage_meso_;
    Text placeholder_;
};
}  // namespace ms
