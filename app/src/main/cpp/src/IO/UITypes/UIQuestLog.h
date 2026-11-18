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

#include "../../Character/QuestLog.h"
#include "../Components/Slider.h"
#include "../Components/Textfield.h"
#include "../UIDragElement.h"

#include <utility>
#include <vector>

namespace ms {
class UIQuestLog : public UIDragElement<PosQUEST> {
public:
    static constexpr Type TYPE = UIElement::Type::QUEST_LOG;
    static constexpr bool FOCUSED = false;
    static constexpr bool TOGGLED = true;

    UIQuestLog(const QuestLog &questLog);

    void draw(float inter) const override;

    void send_key(int32_t keycode, bool pressed, bool escape) override;

    Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;

    UIElement::Type get_type() const override;

    // Rebuilds the list after the quest log changed.
    void refresh();

protected:
    Button::State button_pressed(uint16_t buttonid) override;

private:
    void change_tab(uint16_t tabid);

    // Rebuilds the visible rows for the current tab.
    void rebuild_rows();

    void scroll_rows(bool upwards);

    // Shows the detail window of the selected quest, if there is one.
    void build_detail();

    void clear_detail();

    void draw_detail(float alpha) const;

    void give_up();

    void complete();

    struct Row {
        int16_t qid;
        Text name;
        Text selected_name;
    };

    enum Buttons : uint16_t {
        TAB0,
        TAB1,
        TAB2,
        CLOSE,
        SEARCH,
        ALL_LEVEL,
        MY_LOCATION,
        INFO_CLOSE,
        INFO_GIVEUP,
        INFO_COMPLETE
    };

    static constexpr int16_t ROW_HEIGHT_ = 15;
    static constexpr int16_t LIST_TOP_ = 76;
    static constexpr int16_t LIST_LEFT_ = 16;
    static constexpr int16_t LIST_WIDTH_ = 250;
    static constexpr int16_t VISIBLE_ROWS_ = 19;

    // The quest detail is a second window attached to the right of the list.
    static constexpr int16_t DETAIL_WIDTH_ = 296;
    static constexpr int16_t DETAIL_HEIGHT_ = 396;
    static constexpr int16_t DETAIL_TEXT_LEFT_ = 16;
    static constexpr int16_t DETAIL_TEXT_WIDTH_ = 264;
    static constexpr int16_t DETAIL_TITLE_Y_ = 30;
    static constexpr int16_t DETAIL_BODY_Y_ = 52;
    static constexpr int16_t DETAIL_BODY_BOTTOM_ = 350;

    const QuestLog &questlog_;
    uint16_t tab_;
    std::vector<Row> rows_;
    int16_t offset_;
    int16_t selected_;
    int16_t list_width_;
    int32_t detail_npc_;
    bool detail_can_complete_;
    Sprite detail_backgrnd_;
    Sprite detail_backgrnd2_;
    Text detail_title_;
    Text detail_body_;
    Text detail_required_;
    std::vector<Sprite> notice_sprites_;
    Textfield search_;
    Text placeholder_;
    Slider slider_;
    Texture search_area_;
};
}  // namespace ms