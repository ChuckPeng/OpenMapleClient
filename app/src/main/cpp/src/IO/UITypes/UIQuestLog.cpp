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
#include "UIQuestLog.h"

#include <nlnx/nx.hpp>

#include <algorithm>

#include "../../Data/ItemData.h"
#include "../../Gameplay/MapleMap/Npc.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/QuestPackets.h"
#include "../Components/MapleButton.h"
#include "../UI.h"
#include "UIChatBar.h"

namespace ms {
namespace {
// Quest descriptions use the same inline markup as npc dialogue.
std::string format_quest_text(const std::string &text) {
    std::string formatted = text;

    // #t<id># stands for the name of an item and can appear more than once.
    size_t begin = 0;

    while ((begin = formatted.find("#t", begin)) != std::string::npos) {
        size_t end = formatted.find("#", begin + 2);

        if (end == std::string::npos) {
            break;
        }

        std::string name;

        try {
            int32_t itemid =
                std::stoi(formatted.substr(begin + 2, end - begin - 2));
            name = ItemData::get(itemid).get_name();
        } catch (const std::exception &) {
            name = "";
        }

        formatted.replace(begin, end - begin + 1, name);
        begin += name.length();
    }

    // Tags that would need inline images or colours are dropped together with
    // their argument instead of leaving the raw code in the sentence.
    for (const char *tag: { "#i", "#v", "#c", "#L" }) {
        begin = 0;

        while ((begin = formatted.find(tag, begin)) != std::string::npos) {
            size_t end = formatted.find("#", begin + 2);

            if (end == std::string::npos) {
                break;
            }

            formatted.erase(begin, end - begin + 1);
        }
    }

    for (const char *tag: { "#b", "#r", "#k", "#n", "#e", "#g" }) {
        begin = 0;

        while ((begin = formatted.find(tag, begin)) != std::string::npos) {
            formatted.erase(begin, 2);
        }
    }

    return formatted;
}
}  // namespace

UIQuestLog::UIQuestLog(const QuestLog &ql) :
    UIDragElement<PosQUEST>(),
    questlog_(ql),
    offset_(0),
    selected_(-1),
    list_width_(0),
    detail_npc_(0),
    detail_can_complete_(false) {
    tab_ = Buttons::TAB0;

    nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
    nl::node quest = nl::nx::ui["UIWindow2.img"]["Quest"];
    nl::node list = quest["list"];

    nl::node backgrnd = list["backgrnd"];

    sprites_.emplace_back(backgrnd);
    sprites_.emplace_back(list["backgrnd2"]);

    notice_sprites_.emplace_back(list["notice0"]);
    notice_sprites_.emplace_back(list["notice1"]);
    notice_sprites_.emplace_back(list["notice2"]);

    nl::node taben = list["Tab"]["enabled"];
    nl::node tabdis = list["Tab"]["disabled"];

    buttons_[Buttons::TAB0] =
        std::make_unique<TwoSpriteButton>(tabdis["0"], taben["0"]);
    buttons_[Buttons::TAB1] =
        std::make_unique<TwoSpriteButton>(tabdis["1"], taben["1"]);
    buttons_[Buttons::TAB2] =
        std::make_unique<TwoSpriteButton>(tabdis["2"], taben["2"]);
    buttons_[Buttons::CLOSE] =
        std::make_unique<MapleButton>(close, Point<int16_t>(275, 6));
    buttons_[Buttons::SEARCH] = std::make_unique<MapleButton>(list["BtSearch"]);
    buttons_[Buttons::ALL_LEVEL] =
        std::make_unique<MapleButton>(list["BtAllLevel"]);
    buttons_[Buttons::MY_LOCATION] =
        std::make_unique<MapleButton>(list["BtMyLocation"]);

    search_area_ = list["searchArea"];
    auto search_area_dim = search_area_.get_dimensions();
    auto search_area_origin = search_area_.get_origin().abs();

    auto search_pos_adj = Point<int16_t>(29, 0);
    auto search_dim_adj = Point<int16_t>(-80, 0);

    auto search_pos = search_area_origin + search_pos_adj;
    auto search_dim = search_pos + search_area_dim + search_dim_adj;

    search_ = Textfield(Text::Font::A11M,
                        Text::Alignment::LEFT,
                        Color::Name::BOULDER,
                        Rectangle<int16_t>(search_pos, search_dim),
                        19);
    placeholder_ = Text(Text::Font::A11M,
                        Text::Alignment::LEFT,
                        Color::Name::BOULDER,
                        "Enter the quest name.");

    slider_ = Slider(Slider::Type::DEFAULT_SILVER,
                     Range<int16_t>(0, 279),
                     150,
                     VISIBLE_ROWS_,
                     1,
                     [&](bool upwards) { scroll_rows(upwards); });

    nl::node quest_info = quest["quest_info"];

    detail_backgrnd_ = Sprite(quest_info["backgrnd"]);
    detail_backgrnd2_ = Sprite(quest_info["backgrnd2"]);
    detail_title_ = Text(Text::Font::A12B,
                         Text::Alignment::LEFT,
                         Color::Name::DARKGREY,
                         "",
                         DETAIL_TEXT_WIDTH_);
    detail_body_ = Text(Text::Font::A12M,
                        Text::Alignment::LEFT,
                        Color::Name::DARKGREY,
                        "",
                        DETAIL_TEXT_WIDTH_);
    detail_required_ = Text(Text::Font::A12M,
                            Text::Alignment::LEFT,
                            Color::Name::BLUE,
                            "",
                            DETAIL_TEXT_WIDTH_);

    Point<int16_t> list_dimension = Texture(backgrnd).get_dimensions();
    list_width_ = list_dimension.x();

    buttons_[Buttons::INFO_CLOSE] =
        std::make_unique<MapleButton>(quest_info["BtClose"], list_width_, 0);
    buttons_[Buttons::INFO_GIVEUP] =
        std::make_unique<MapleButton>(quest_info["BtGiveup"], list_width_, 0);
    buttons_[Buttons::INFO_COMPLETE] =
        std::make_unique<MapleButton>(quest_info["BtFinish"], list_width_, 0);

    dimension_ = Point<int16_t>(list_width_ + DETAIL_WIDTH_,
                                std::max<int16_t>(list_dimension.y(),
                                                  DETAIL_HEIGHT_));
    drag_area_ = Point<int16_t>(dimension_.x(), 20);

    change_tab(tab_);
}

void UIQuestLog::draw(float alpha) const {
    UIElement::draw_sprites(alpha);

    if (rows_.empty()) {
        Point<int16_t> notice_position = Point<int16_t>(0, 26);

        if (tab_ == Buttons::TAB0) {
            notice_sprites_[tab_].draw(
                position_ + notice_position + Point<int16_t>(9, 0),
                alpha);
        } else if (tab_ == Buttons::TAB1) {
            notice_sprites_[tab_].draw(
                position_ + notice_position + Point<int16_t>(0, 0),
                alpha);
        } else {
            notice_sprites_[tab_].draw(
                position_ + notice_position + Point<int16_t>(-10, 0),
                alpha);
        }
    } else {
        int16_t row_y = LIST_TOP_;

        for (int16_t i = 0; i < VISIBLE_ROWS_; ++i) {
            size_t index = size_t(offset_) + size_t(i);

            if (index >= rows_.size()) {
                break;
            }

            const Row &row = rows_[index];
            bool selected = int16_t(index) == selected_;
            const Text &label = selected ? row.selected_name : row.name;

            label.draw(position_ + Point<int16_t>(LIST_LEFT_, row_y));
            row_y += ROW_HEIGHT_;
        }
    }

    if (selected_ >= 0 && size_t(selected_) < rows_.size()) {
        draw_detail(alpha);
    }

    if (tab_ != Buttons::TAB2) {
        search_area_.draw(position_);
        search_.draw(position_);

        if (search_.get_state() == Textfield::State::NORMAL
            && search_.empty()) {
            placeholder_.draw(position_ + Point<int16_t>(39, 51));
        }
    }

    slider_.draw(position_ + Point<int16_t>(126, 75));

    UIElement::draw_buttons(alpha);
}

void UIQuestLog::send_key(int32_t keycode, bool pressed, bool escape) {
    if (pressed) {
        if (escape) {
            deactivate();
        } else if (keycode == KeyAction::Id::TAB) {
            uint16_t new_tab = tab_;

            if (new_tab < Buttons::TAB2) {
                new_tab++;
            } else {
                new_tab = Buttons::TAB0;
            }

            change_tab(new_tab);
        }
    }
}

Cursor::State UIQuestLog::send_cursor(bool clicking, Point<int16_t> cursorpos) {
    if (slider_.isenabled()) {
        if (Cursor::State sstate =
                slider_.send_cursor(cursorpos - position_, clicking)) {
            return sstate;
        }
    }

    if (Cursor::State new_state = search_.send_cursor(cursorpos, clicking)) {
        return new_state;
    }

    if (clicking && !rows_.empty()) {
        Point<int16_t> relative = cursorpos - position_;

        if (relative.x() >= LIST_LEFT_
            && relative.x() <= LIST_LEFT_ + LIST_WIDTH_
            && relative.y() >= LIST_TOP_) {
            int16_t row = offset_
                          + (relative.y() - LIST_TOP_) / ROW_HEIGHT_;

            if (row >= 0 && size_t(row) < rows_.size()) {
                if (selected_ != row) {
                    selected_ = row;

                    build_detail();
                }
            }
        }
    }

    search_.update(position_);

    return UIDragElement::send_cursor(clicking, cursorpos);
}

void UIQuestLog::rebuild_rows() {
    int16_t previous = -1;

    if (selected_ >= 0 && size_t(selected_) < rows_.size()) {
        previous = rows_[selected_].qid;
    }

    rows_.clear();
    offset_ = 0;
    selected_ = -1;

    auto add = [&](int16_t qid) {
        std::string name =
            nl::nx::quest["QuestInfo.img"][std::to_string(qid)]["name"];

        // Quests without a name, such as the hidden quests that carry the
        // progress of another one, are not shown.
        if (name.empty()) {
            return;
        }

        rows_.push_back(
            { qid,
              Text(Text::Font::A12M,
                   Text::Alignment::LEFT,
                   Color::Name::DARKGREY,
                   name,
                   LIST_WIDTH_),
              Text(Text::Font::A12M,
                   Text::Alignment::LEFT,
                   Color::Name::BLUE,
                   name,
                   LIST_WIDTH_) });
    };

    if (tab_ == Buttons::TAB1) {
        for (const auto &entry: questlog_.get_started()) {
            add(entry.first);
        }
    } else if (tab_ == Buttons::TAB2) {
        for (const auto &entry: questlog_.get_completed()) {
            add(entry.first);
        }
    }

    int16_t rows = int16_t(rows_.size());
    int16_t pages = rows > VISIBLE_ROWS_
                        ? (rows + VISIBLE_ROWS_ - 1) / VISIBLE_ROWS_
                        : 1;

    slider_.setrows(pages, pages);
    slider_.setenabled(rows > VISIBLE_ROWS_);

    if (previous >= 0) {
        for (size_t i = 0; i < rows_.size(); i++) {
            if (rows_[i].qid == previous) {
                selected_ = int16_t(i);

                break;
            }
        }
    }

    build_detail();
}

void UIQuestLog::scroll_rows(bool upwards) {
    int16_t max_offset = std::max<int16_t>(0, int16_t(rows_.size()) - VISIBLE_ROWS_);
    int16_t shift = upwards ? -VISIBLE_ROWS_ : VISIBLE_ROWS_;

    offset_ = std::clamp<int16_t>(offset_ + shift, 0, max_offset);
}

UIElement::Type UIQuestLog::get_type() const {
    return TYPE;
}

void UIQuestLog::refresh() {
    rebuild_rows();
}

Button::State UIQuestLog::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case Buttons::TAB0:
        case Buttons::TAB1:
        case Buttons::TAB2:
            change_tab(buttonid);

            return Button::State::IDENTITY;
        case Buttons::CLOSE: deactivate(); break;
        case Buttons::INFO_CLOSE:
            selected_ = -1;

            clear_detail();

            return Button::State::NORMAL;
        case Buttons::INFO_GIVEUP: give_up(); break;
        case Buttons::INFO_COMPLETE: complete(); break;
        default: break;
    }

    return Button::State::NORMAL;
}

void UIQuestLog::build_detail() {
    detail_npc_ = 0;
    detail_can_complete_ = false;

    if (selected_ < 0 || size_t(selected_) >= rows_.size()) {
        clear_detail();

        return;
    }

    int16_t qid = rows_[selected_].qid;
    std::string qidstr = std::to_string(qid);

    nl::node info = nl::nx::quest["QuestInfo.img"][qidstr];

    detail_title_.change_text(format_quest_text(info["name"]));
    detail_body_.change_text(
        format_quest_text(info[tab_ == Buttons::TAB2 ? "2" : "1"]));
    detail_required_.change_text("");

    bool started = tab_ == Buttons::TAB1;

    if (started) {
        nl::node check = nl::nx::quest["Check.img"][qidstr];

        std::string required;

        for (nl::node item: check["1"]["item"]) {
            auto itemid = int32_t(item["id"].get_integer());
            auto count = int32_t(item["count"].get_integer());

            if (!required.empty()) {
                required += ", ";
            }

            required += ItemData::get(itemid).get_name() + " x"
                        + std::to_string(count);
        }

        if (!required.empty()) {
            detail_required_.change_text("Needed: " + required);
        }

        // The server only accepts an operation from next to the npc that is
        // tied to it. Quests that start or finish on their own need none.
        detail_npc_ = int32_t(check["1"]["npc"].get_integer());

        if (detail_npc_ == 0) {
            detail_npc_ = int32_t(check["0"]["npc"].get_integer());
        }

        detail_can_complete_ = detail_npc_ != 0
                               || info["autoComplete"].get_integer() == 1
                               || info["autoStart"].get_integer() == 1;
    }

    buttons_[Buttons::INFO_CLOSE]->set_active(true);
    buttons_[Buttons::INFO_GIVEUP]->set_active(started);
    buttons_[Buttons::INFO_COMPLETE]->set_active(started
                                                 && detail_can_complete_);
}

void UIQuestLog::clear_detail() {
    detail_npc_ = 0;
    detail_can_complete_ = false;

    detail_title_.change_text("");
    detail_body_.change_text("");
    detail_required_.change_text("");

    buttons_[Buttons::INFO_CLOSE]->set_active(false);
    buttons_[Buttons::INFO_GIVEUP]->set_active(false);
    buttons_[Buttons::INFO_COMPLETE]->set_active(false);
}

void UIQuestLog::draw_detail(float alpha) const {
    Point<int16_t> origin = position_ + Point<int16_t>(list_width_, 0);

    detail_backgrnd_.draw(origin, alpha);
    detail_backgrnd2_.draw(origin, alpha);

    detail_title_.draw(
        origin + Point<int16_t>(DETAIL_TEXT_LEFT_, DETAIL_TITLE_Y_));

    int16_t title_height = detail_title_.height();
    int16_t body_y = DETAIL_BODY_Y_
                     + std::max<int16_t>(0, title_height - 14);

    detail_body_.draw(origin + Point<int16_t>(DETAIL_TEXT_LEFT_, body_y),
                      Range<int16_t>(origin.y() + body_y,
                                     origin.y() + DETAIL_BODY_BOTTOM_));

    if (!detail_required_.empty()) {
        int16_t required_y = body_y + detail_body_.height() + 10;

        if (required_y < DETAIL_BODY_BOTTOM_) {
            detail_required_.draw(
                origin + Point<int16_t>(DETAIL_TEXT_LEFT_, required_y));
        }
    }
}

void UIQuestLog::give_up() {
    if (selected_ < 0 || size_t(selected_) >= rows_.size()) {
        return;
    }

    QuestActionPacket packet(QuestActionPacket::FORFEIT, rows_[selected_].qid);

    packet.dispatch();
}

void UIQuestLog::complete() {
    if (selected_ < 0 || size_t(selected_) >= rows_.size()) {
        return;
    }

    int16_t qid = rows_[selected_].qid;

    // The server drops a completion silently when the npc is not on the map at
    // all, so tell the player what is missing instead of doing nothing.
    if (detail_npc_ != 0) {
        bool npc_on_map = false;

        for (auto &entry: *Stage::get().get_npcs().get_npcs()) {
            const Npc *npc = static_cast<const Npc *>(entry.second.get());

            if (npc && npc->get_id() == detail_npc_) {
                npc_on_map = true;

                break;
            }
        }

        if (!npc_on_map) {
            std::string npcname =
                nl::nx::string["Npc.img"][std::to_string(detail_npc_)]["name"];

            if (auto chatbar = UI::get().get_element<UIChatBar>()) {
                chatbar->get().send_chatline(
                    "Go to " + npcname + " to finish this quest.",
                    UIChatBar::LineType::YELLOW);
            }

            return;
        }
    }

    QuestActionPacket packet(QuestActionPacket::COMPLETE, qid);

    packet.write_npc(detail_npc_, Stage::get().get_player().get_position());
    packet.dispatch();
}

void UIQuestLog::change_tab(uint16_t tabid) {
    uint16_t oldtab = tab_;
    tab_ = tabid;

    if (oldtab != tab_) {
        buttons_[Buttons::TAB0 + oldtab]->set_state(Button::State::NORMAL);
        buttons_[Buttons::MY_LOCATION]->set_active(tab_ == Buttons::TAB0);
        buttons_[Buttons::ALL_LEVEL]->set_active(tab_ == Buttons::TAB0);
        buttons_[Buttons::SEARCH]->set_active(tab_ != Buttons::TAB2);

        if (tab_ == Buttons::TAB2) {
            search_.set_state(Textfield::State::DISABLED);
        } else {
            search_.set_state(Textfield::State::NORMAL);
        }
    }

    buttons_[Buttons::TAB0 + tab_]->set_state(Button::State::PRESSED);

    rebuild_rows();
}
}  // namespace ms