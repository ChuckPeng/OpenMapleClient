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
#include "UIUserList.h"

#include <nlnx/nx.hpp>

#include <algorithm>
#include <map>
#include <utility>

#include "../../Gameplay/Stage.h"
#include "../../Graphics/GraphicsGL.h"
#include "../../Net/Packets/BuddyPackets.h"
#include "../../Util/GameInfo.h"
#include "../Components/MapleButton.h"
#include "../Components/TwoSpriteButton.h"
#include "../UI.h"
#include "UINotice.h"

namespace ms {
namespace {
std::string trim_ascii(std::string value) {
    auto is_ws = [](unsigned char c) { return c == ' ' || c == '\t'; };
    while (!value.empty() && is_ws(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_ws(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}
}  // namespace

UIUserList::UIUserList(uint16_t t) :
    UIDragElement<PosUSERLIST>(Point<int16_t>(264, 20)),
    tab_(t) {
    nl::node close = nl::nx::ui["Basic.img"]["BtClose3"];
    user_list_ = nl::nx::ui["UIWindow2.img"]["UserList"];
    nl::node Main = user_list_["Main"];

    sprites_.emplace_back(Main["backgrnd"]);

    buttons_[Buttons::BT_CLOSE] =
        std::make_unique<MapleButton>(close, Point<int16_t>(244, 7));

    nl::node taben = Main["Tab"]["enabled"];
    nl::node tabdis = Main["Tab"]["disabled"];

    buttons_[Buttons::BT_TAB_FRIEND] =
        std::make_unique<TwoSpriteButton>(tabdis["0"], taben["0"]);
    buttons_[Buttons::BT_TAB_PARTY] =
        std::make_unique<TwoSpriteButton>(tabdis["1"], taben["1"]);
    buttons_[Buttons::BT_TAB_BOSS] =
        std::make_unique<TwoSpriteButton>(tabdis["2"], taben["2"]);
    buttons_[Buttons::BT_TAB_BLACKLIST] =
        std::make_unique<TwoSpriteButton>(tabdis["3"], taben["3"]);

    // Party Tab
    nl::node Party = Main["Party"];
    nl::node PartySearch = Party["PartySearch"];

    party_tab_ = Tab::PARTY_MINE;
    party_title_ = Party["title"];

    for (size_t i = 0; i < party_mine_grid_.size(); i++) {
        party_mine_grid_[i] = user_list_["Sheet2"][i];
    }

    party_mine_name_ = Text(Text::Font::A12M,
                            Text::Alignment::LEFT,
                            Color::Name::BLACK,
                            "none",
                            0);

    nl::node party_taben = Party["Tab"]["enabled"];
    nl::node party_tabdis = Party["Tab"]["disabled"];

    buttons_[Buttons::BT_PARTY_CREATE] =
        std::make_unique<MapleButton>(Party["BtPartyMake"]);
    buttons_[Buttons::BT_PARTY_INVITE] =
        std::make_unique<MapleButton>(Party["BtPartyInvite"]);
    buttons_[Buttons::BT_PARTY_LEAVE] =
        std::make_unique<MapleButton>(Party["BtPartyOut"]);
    buttons_[Buttons::BT_PARTY_SETTINGS] =
        std::make_unique<MapleButton>(Party["BtPartySettings"]);
    buttons_[Buttons::BT_PARTY_CREATE]->set_active(false);
    buttons_[Buttons::BT_PARTY_INVITE]->set_active(false);
    buttons_[Buttons::BT_PARTY_LEAVE]->set_active(false);
    buttons_[Buttons::BT_PARTY_SETTINGS]->set_active(false);

    buttons_[Buttons::BT_TAB_PARTY_MINE] =
        std::make_unique<TwoSpriteButton>(party_tabdis["0"], party_taben["0"]);
    buttons_[Buttons::BT_TAB_PARTY_SEARCH] =
        std::make_unique<TwoSpriteButton>(party_tabdis["1"], party_taben["1"]);
    buttons_[Buttons::BT_TAB_PARTY_MINE]->set_active(false);
    buttons_[Buttons::BT_TAB_PARTY_SEARCH]->set_active(false);

    party_search_grid_[0] = PartySearch["partyName"];
    party_search_grid_[1] = PartySearch["request"];
    party_search_grid_[2] = PartySearch["table"];

    buttons_[Buttons::BT_PARTY_SEARCH_LEVEL] =
        std::make_unique<MapleButton>(PartySearch["BtPartyLevel"]);
    buttons_[Buttons::BT_PARTY_SEARCH_LEVEL]->set_active(false);

    int16_t party_x = 243;
    int16_t party_y = 114;
    int16_t party_height = party_y + 168;
    int16_t party_unitrows = 6;
    int16_t party_rowmax = 6;
    party_slider_ = Slider(Slider::Type::DEFAULT_SILVER,
                           Range<int16_t>(party_y, party_height),
                           party_x,
                           party_unitrows,
                           party_rowmax,
                           [](bool) {});

    // Buddy Tab
    nl::node Friend = Main["Friend"];
    nl::node AddFriend = user_list_["AddFriend"];

    friend_tab_ = Tab::FRIEND_ALL;
    friend_sprites_.emplace_back(Friend["title"]);
    friend_sprites_.emplace_back(
        user_list_["line"],
        DrawArgument(Point<int16_t>(132, 115), Point<int16_t>(230, 0)));

    group_open_tex_ = user_list_["BtSheetIClose"]["normal"]["0"];
    group_closed_tex_ = user_list_["BtSheetIOpen"]["normal"]["0"];
    online_icon_ = Friend["icon0"];
    offline_icon_ = Party["userOff"];

    nl::node SideMenu = Friend["SideMenu"];
    side_top_ = SideMenu["top"];
    side_bottom_ = SideMenu["bottom"];

    buttons_[Buttons::BT_FRIEND_SIDE_DELETE] =
        std::make_unique<MapleButton>(SideMenu["BtDelete"]);
    buttons_[Buttons::BT_FRIEND_SIDE_DELETE]->set_active(false);

    add_bg_ = AddFriend["backgrnd"];
    buttons_[Buttons::BT_ADD_OK] =
        std::make_unique<MapleButton>(AddFriend["BtOK"], ADD_POPUP_OFFSET_);
    buttons_[Buttons::BT_ADD_CANCEL] =
        std::make_unique<MapleButton>(AddFriend["BtCancel"], ADD_POPUP_OFFSET_);
    buttons_[Buttons::BT_ADD_OK]->set_active(false);
    buttons_[Buttons::BT_ADD_CANCEL]->set_active(false);

    add_name_field_ = Textfield(Text::Font::A11M,
                                Text::Alignment::LEFT,
                                Color::Name::BLACK,
                                Rectangle<int16_t>(24, 236, 80, 96),
                                13);
    add_name_field_.set_enter_callback(
        [this](std::string) { submit_add_popup(); });
    add_name_field_.set_key_callback(KeyAction::Id::ESCAPE,
                                     [this]() { close_add_popup(); });

    add_group_field_ = Textfield(Text::Font::A11M,
                                 Text::Alignment::LEFT,
                                 Color::Name::BLACK,
                                 Rectangle<int16_t>(24, 236, 101, 117),
                                 13);
    add_group_field_.set_enter_callback(
        [this](std::string) { submit_add_popup(); });
    add_group_field_.set_key_callback(KeyAction::Id::ESCAPE,
                                      [this]() { close_add_popup(); });

    add_name_placeholder_ = Text(Text::Font::A11M,
                                 Text::Alignment::LEFT,
                                 Color::Name::DUSTYGRAY,
                                 "Character Name",
                                 208);
    add_group_placeholder_ = Text(Text::Font::A11M,
                                  Text::Alignment::LEFT,
                                  Color::Name::DUSTYGRAY,
                                  "Default Group",
                                  208);
    edit_name_text_ = Text(Text::Font::A11M,
                           Text::Alignment::LEFT,
                           Color::Name::DARKGREY,
                           "",
                           208);
    add_hint_text_ = Text(Text::Font::A11M,
                          Text::Alignment::LEFT,
                          Color::Name::DARKGREY,
                          "",
                          208);

    for (size_t i = 0; i < friend_grid_.size(); i++) {
        friend_grid_[i] = user_list_["Sheet1"][i];
    }

    friends_online_text_ = Text(Text::Font::A11M,
                                Text::Alignment::LEFT,
                                Color::Name::WHITE,
                                "(0/20)",
                                0);
    friends_cur_location_ = Text(Text::Font::A11M,
                                 Text::Alignment::LEFT,
                                 Color::Name::DARKGREY,
                                 "My Location - ",
                                 242);
    update_location_text();

    buttons_[Buttons::BT_FRIEND_ADD] =
        std::make_unique<MapleButton>(Friend["BtAddFriend"]);
    buttons_[Buttons::BT_FRIEND_ADD_GROUP] =
        std::make_unique<MapleButton>(Friend["BtAddGroup"]);
    buttons_[Buttons::BT_FRIEND_EXPAND] =
        std::make_unique<MapleButton>(Friend["BtPlusFriend"]);
    buttons_[Buttons::BT_FRIEND_ADD]->set_active(false);
    buttons_[Buttons::BT_FRIEND_ADD_GROUP]->set_active(false);
    buttons_[Buttons::BT_FRIEND_EXPAND]->set_active(false);

    buttons_[Buttons::BT_TAB_FRIEND_ALL] =
        std::make_unique<MapleButton>(Friend["TapShowAll"]);
    buttons_[Buttons::BT_TAB_FRIEND_ONLINE] =
        std::make_unique<MapleButton>(Friend["TapShowOnline"]);
    buttons_[Buttons::BT_TAB_FRIEND_ALL]->set_active(false);
    buttons_[Buttons::BT_TAB_FRIEND_ONLINE]->set_active(false);

    friends_slider_ = Slider(
        Slider::Type::DEFAULT_SILVER,
        Range<int16_t>(115, 257),
        243,
        FRIEND_VISIBLE_ROWS_,
        FRIEND_VISIBLE_ROWS_,
        [this](bool upwards) {
            int16_t max_offset = std::max<int16_t>(
                0, int16_t(friend_rows_.size()) - FRIEND_VISIBLE_ROWS_);
            int16_t next = friend_scroll_offset_ + (upwards ? -1 : 1);
            friend_scroll_offset_ = std::clamp<int16_t>(next, 0, max_offset);
            close_side_menu();
        });
    friends_slider_.setenabled(false);

    // Boss tab
    nl::node Boss = Main["Boss"];

    boss_sprites_.emplace_back(Boss["base"]);
    boss_sprites_.emplace_back(Boss["base3"]);
    boss_sprites_.emplace_back(Boss["base2"]);

    buttons_[Buttons::BT_BOSS_0] = std::make_unique<TwoSpriteButton>(
        Boss["BossList"]["0"]["icon"]["disabled"]["0"],
        Boss["BossList"]["0"]["icon"]["normal"]["0"]);
    buttons_[Buttons::BT_BOSS_1] = std::make_unique<TwoSpriteButton>(
        Boss["BossList"]["1"]["icon"]["disabled"]["0"],
        Boss["BossList"]["1"]["icon"]["normal"]["0"]);
    buttons_[Buttons::BT_BOSS_2] = std::make_unique<TwoSpriteButton>(
        Boss["BossList"]["2"]["icon"]["disabled"]["0"],
        Boss["BossList"]["2"]["icon"]["normal"]["0"]);
    buttons_[Buttons::BT_BOSS_3] = std::make_unique<TwoSpriteButton>(
        Boss["BossList"]["3"]["icon"]["disabled"]["0"],
        Boss["BossList"]["3"]["icon"]["normal"]["0"]);
    buttons_[Buttons::BT_BOSS_4] = std::make_unique<TwoSpriteButton>(
        Boss["BossList"]["4"]["icon"]["disabled"]["0"],
        Boss["BossList"]["4"]["icon"]["normal"]["0"]);
    buttons_[Buttons::BT_BOSS_L] =
        std::make_unique<MapleButton>(Boss["BtArrow"]["Left"]);
    buttons_[Buttons::BT_BOSS_R] =
        std::make_unique<MapleButton>(Boss["BtArrow"]["Right"]);
    buttons_[Buttons::BT_BOSS_DIFF_L] =
        std::make_unique<MapleButton>(Boss["BtArrow2"]["Left"]);
    buttons_[Buttons::BT_BOSS_DIFF_R] =
        std::make_unique<MapleButton>(Boss["BtArrow2"]["Right"]);
    buttons_[Buttons::BT_BOSS_GO] =
        std::make_unique<MapleButton>(Boss["BtEntry"]);
    buttons_[Buttons::BT_BOSS_0]->set_active(false);
    buttons_[Buttons::BT_BOSS_1]->set_active(false);
    buttons_[Buttons::BT_BOSS_2]->set_active(false);
    buttons_[Buttons::BT_BOSS_3]->set_active(false);
    buttons_[Buttons::BT_BOSS_4]->set_active(false);
    buttons_[Buttons::BT_BOSS_L]->set_active(false);
    buttons_[Buttons::BT_BOSS_R]->set_active(false);
    buttons_[Buttons::BT_BOSS_DIFF_L]->set_active(false);
    buttons_[Buttons::BT_BOSS_DIFF_R]->set_active(false);
    buttons_[Buttons::BT_BOSS_GO]->set_active(false);

    // Blacklist tab
    nl::node Blacklist = Main["BlackList"];

    blacklist_title_ = Blacklist["base"];

    for (size_t i = 0; i < blacklist_grid_.size(); i++) {
        blacklist_grid_[i] = user_list_["Sheet6"][i];
    }

    blacklist_name_ = Text(Text::Font::A12M,
                           Text::Alignment::LEFT,
                           Color::Name::BLACK,
                           "none",
                           0);

    buttons_[Buttons::BT_BLACKLIST_ADD] =
        std::make_unique<MapleButton>(Blacklist["BtAdd"]);
    buttons_[Buttons::BT_BLACKLIST_DELETE] =
        std::make_unique<MapleButton>(Blacklist["BtDelete"]);
    buttons_[Buttons::BT_TAB_BLACKLIST_INDIVIDUAL] =
        std::make_unique<MapleButton>(Blacklist["TapShowIndividual"]);
    buttons_[Buttons::BT_TAB_BLACKLIST_GUILD] =
        std::make_unique<MapleButton>(Blacklist["TapShowGuild"]);
    buttons_[Buttons::BT_BLACKLIST_ADD]->set_active(false);
    buttons_[Buttons::BT_BLACKLIST_DELETE]->set_active(false);
    buttons_[Buttons::BT_TAB_BLACKLIST_INDIVIDUAL]->set_active(false);
    buttons_[Buttons::BT_TAB_BLACKLIST_GUILD]->set_active(false);

    dimension_ = Point<int16_t>(264, 382);
    change_tab(tab_);
}

void UIUserList::draw(float alpha) const {
    UIElement::draw_sprites(alpha);

    background_.draw(position_);

    if (tab_ == Buttons::BT_TAB_PARTY) {
        party_title_.draw(position_);

        if (party_tab_ == Buttons::BT_TAB_PARTY_MINE) {
            party_mine_grid_[0].draw(position_ + Point<int16_t>(10, 115));
            party_mine_grid_[4].draw(position_ + Point<int16_t>(10, 133));
            party_mine_name_.draw(position_ + Point<int16_t>(27, 130));
        } else if (party_tab_ == Buttons::BT_TAB_PARTY_SEARCH) {
            party_search_grid_[0].draw(position_);
            party_search_grid_[1].draw(position_);
            party_search_grid_[2].draw(position_);
            party_slider_.draw(position_);
        }
    } else if (tab_ == Buttons::BT_TAB_FRIEND) {
        for (const auto &sprite : friend_sprites_) {
            sprite.draw(position_, alpha);
        }

        friends_online_text_.draw(position_ + Point<int16_t>(204, 65));
        friends_cur_location_.draw(position_ + Point<int16_t>(11, 279));

        for (int16_t vis = 0; vis < FRIEND_VISIBLE_ROWS_; ++vis) {
            int16_t row_y = FRIEND_LIST_Y_ + vis * FRIEND_ROW_H_;
            Point<int16_t> row_pos =
                position_ + Point<int16_t>(FRIEND_LIST_X_, row_y);
            int16_t idx = friend_scroll_offset_ + vis;

            if (idx >= 0 && idx < int16_t(friend_rows_.size())) {
                const FriendRow &row = friend_rows_[idx];

                if (row.is_group) {
                    friend_grid_[0].draw(row_pos);
                    const Texture &chevron =
                        row.collapsed ? group_closed_tex_ : group_open_tex_;
                    chevron.draw(row_pos + Point<int16_t>(3, 3));
                    row.label.draw(row_pos + Point<int16_t>(19, 2));
                } else {
                    const Texture &cell =
                        (vis == FRIEND_VISIBLE_ROWS_ - 1) ? friend_grid_[3]
                                                          : friend_grid_[2];
                    cell.draw(row_pos);

                    if (row.cid == selected_cid_) {
                        GraphicsGL::get().draw_rectangle(row_pos.x() + 1,
                                                         row_pos.y() + 1,
                                                         FRIEND_ROW_W_ - 2,
                                                         FRIEND_ROW_H_ - 2,
                                                         0.25f,
                                                         0.55f,
                                                         0.90f,
                                                         0.25f);
                    }

                    const Texture &dot =
                        row.online ? online_icon_ : offline_icon_;
                    dot.draw(row_pos + Point<int16_t>(5, 6));
                    row.label.draw(row_pos + Point<int16_t>(16, 2));
                    row.status.draw(row_pos + Point<int16_t>(121, 2));
                }
            } else {
                const Texture &cell =
                    (vis == FRIEND_VISIBLE_ROWS_ - 1) ? friend_grid_[3]
                                                      : friend_grid_[2];
                cell.draw(row_pos);
            }
        }

        friends_slider_.draw(position_);
    } else if (tab_ == Buttons::BT_TAB_BOSS) {
        for (const auto &sprite : boss_sprites_) {
            sprite.draw(position_, alpha);
        }
    } else if (tab_ == Buttons::BT_TAB_BLACKLIST) {
        blacklist_title_.draw(position_ + Point<int16_t>(24, 104));
        blacklist_grid_[0].draw(position_ + Point<int16_t>(24, 134));
        blacklist_name_.draw(position_ + Point<int16_t>(24, 134));
    }

    for (const auto &iter : buttons_) {
        if (iter.first != Buttons::BT_FRIEND_SIDE_DELETE
            && iter.first != Buttons::BT_ADD_OK
            && iter.first != Buttons::BT_ADD_CANCEL) {
            if (const Button *button = iter.second.get()) {
                button->draw(position_);
            }
        }
    }

    if (tab_ == Buttons::BT_TAB_FRIEND && side_menu_open_ && !add_popup_open_) {
        Point<int16_t> sm_pos = position_ + side_menu_pos_;
        side_top_.draw(sm_pos);
        side_bottom_.draw(sm_pos + Point<int16_t>(0, 14));
        buttons_.at(Buttons::BT_FRIEND_SIDE_DELETE)->draw(position_);
    }

    if (tab_ == Buttons::BT_TAB_FRIEND && add_popup_open_) {
        GraphicsGL::get().draw_rectangle(position_.x(),
                                         position_.y(),
                                         dimension_.x(),
                                         dimension_.y(),
                                         0.0f,
                                         0.0f,
                                         0.0f,
                                         0.45f);

        Point<int16_t> popup_pos = position_ + ADD_POPUP_OFFSET_;
        add_bg_.draw(popup_pos);

        if (edit_group_mode_) {
            GraphicsGL::get().draw_rectangle(popup_pos.x() + 22,
                                             popup_pos.y() + 80,
                                             216,
                                             16,
                                             0.88f,
                                             0.88f,
                                             0.88f,
                                             1.0f);
            edit_name_text_.draw(popup_pos + Point<int16_t>(25, 81));
        } else {
            if (add_name_field_.empty()
                && add_name_field_.get_state() == Textfield::State::NORMAL) {
                add_name_placeholder_.draw(popup_pos + Point<int16_t>(25, 81));
            }
            add_name_field_.draw(popup_pos);
        }

        if (add_group_field_.empty()
            && add_group_field_.get_state() == Textfield::State::NORMAL) {
            add_group_placeholder_.draw(popup_pos + Point<int16_t>(25, 102));
        }
        add_group_field_.draw(popup_pos);

        add_hint_text_.draw(popup_pos + Point<int16_t>(28, 128));

        buttons_.at(Buttons::BT_ADD_OK)->draw(position_);
        buttons_.at(Buttons::BT_ADD_CANCEL)->draw(position_);
    }
}

void UIUserList::update() {
    UIElement::update();

    if (tab_ == Buttons::BT_TAB_FRIEND) {
        for (auto &sprite : friend_sprites_) {
            sprite.update();
        }

        update_location_text();

        if (add_popup_open_) {
            Point<int16_t> popup_pos = position_ + ADD_POPUP_OFFSET_;
            add_name_field_.update(popup_pos);
            add_group_field_.update(popup_pos);
        }
    }

    if (tab_ == Buttons::BT_TAB_BOSS) {
        for (auto &sprite : boss_sprites_) {
            sprite.update();
        }
    }
}

void UIUserList::remove_cursor() {
    UIDragElement::remove_cursor();
    friends_slider_.remove_cursor();
}

Cursor::State UIUserList::send_cursor(bool clicking, Point<int16_t> cursorpos) {
    if (dragged_) {
        return UIDragElement::send_cursor(clicking, cursorpos);
    }

    if (add_popup_open_) {
        Point<int16_t> popup_pos = position_ + ADD_POPUP_OFFSET_;
        add_name_field_.update(popup_pos);
        add_group_field_.update(popup_pos);

        if (!edit_group_mode_) {
            Cursor::State nstate =
                add_name_field_.send_cursor(cursorpos, clicking);
            if (nstate != Cursor::State::IDLE) {
                return nstate;
            }
        }

        Cursor::State gstate =
            add_group_field_.send_cursor(cursorpos, clicking);
        if (gstate != Cursor::State::IDLE) {
            return gstate;
        }

        // Background buttons are deactivated while modal is open, so only
        // BT_ADD_OK, BT_ADD_CANCEL, and header dragging respond here.
        return UIDragElement::send_cursor(clicking, cursorpos);
    }

    if (tab_ == Buttons::BT_TAB_FRIEND) {
        Point<int16_t> rel = cursorpos - position_;

        if (side_menu_open_) {
            Rectangle<int16_t> sm_bounds(side_menu_pos_,
                                         side_menu_pos_ + Point<int16_t>(88, 36));
            if (sm_bounds.contains(rel)) {
                return UIElement::send_cursor(clicking, cursorpos);
            }
        }

        if (friends_slider_.isenabled()) {
            Cursor::State sstate = friends_slider_.send_cursor(rel, clicking);
            if (sstate != Cursor::State::IDLE) {
                if (clicking) {
                    close_side_menu();
                }
                return sstate;
            }
        }

        Rectangle<int16_t> list_bounds(
            Point<int16_t>(FRIEND_LIST_X_, FRIEND_LIST_Y_),
            Point<int16_t>(FRIEND_LIST_X_ + FRIEND_ROW_W_,
                           FRIEND_LIST_Y_ + FRIEND_VISIBLE_ROWS_ * FRIEND_ROW_H_));
        if (clicking && list_bounds.contains(rel)) {
            int16_t vis_row = (rel.y() - FRIEND_LIST_Y_) / FRIEND_ROW_H_;
            int16_t idx = friend_scroll_offset_ + vis_row;

            if (idx >= 0 && idx < int16_t(friend_rows_.size())) {
                const FriendRow &row = friend_rows_[idx];
                if (row.is_group) {
                    if (collapsed_groups_.count(row.group_name)) {
                        collapsed_groups_.erase(row.group_name);
                    } else {
                        collapsed_groups_.insert(row.group_name);
                    }
                    close_side_menu();
                    rebuild_friend_rows();
                } else if (side_menu_open_ && selected_cid_ == row.cid) {
                    close_side_menu();
                } else {
                    selected_cid_ = row.cid;
                    update_friend_controls();
                    open_side_menu(vis_row, row.cid);
                }
                return Cursor::State::CLICKING;
            }

            close_side_menu();
        } else if (clicking && side_menu_open_) {
            close_side_menu();
        }
    }

    return UIDragElement::send_cursor(clicking, cursorpos);
}

void UIUserList::send_scroll(double yoffset) {
    if (tab_ == Buttons::BT_TAB_FRIEND && !add_popup_open_
        && friends_slider_.isenabled()) {
        friends_slider_.send_scroll(yoffset);
    }
}

void UIUserList::send_key(int32_t keycode, bool pressed, bool escape) {
    if (!pressed) {
        return;
    }

    if (add_popup_open_) {
        if (escape) {
            close_add_popup();
        } else if (keycode == KeyAction::Id::RETURN) {
            submit_add_popup();
        } else if (keycode == KeyAction::Id::TAB && !edit_group_mode_) {
            if (add_name_field_.get_state() == Textfield::State::FOCUSED) {
                add_group_field_.set_state(Textfield::State::FOCUSED);
            } else {
                add_name_field_.set_state(Textfield::State::FOCUSED);
            }
        }
        return;
    }

    if (escape) {
        close_side_menu();
        deactivate();
    } else if (keycode == KeyAction::Id::TAB) {
        uint16_t new_tab = tab_;

        if (new_tab < Buttons::BT_TAB_BLACKLIST) {
            new_tab++;
        } else {
            new_tab = Buttons::BT_TAB_FRIEND;
        }

        change_tab(static_cast<uint8_t>(new_tab));
    }
}

void UIUserList::toggle_active() {
    if (active_) {
        close_add_popup();
        close_side_menu();
    } else {
        update_location_text();
        rebuild_friend_rows();
    }
    UIElement::toggle_active();
}

UIElement::Type UIUserList::get_type() const {
    return TYPE;
}

void UIUserList::refresh_buddies() {
    rebuild_friend_rows();
}

Button::State UIUserList::button_pressed(uint16_t buttonid) {
    switch (buttonid) {
        case Buttons::BT_CLOSE:
            close_add_popup();
            close_side_menu();
            deactivate();
            break;
        case Buttons::BT_TAB_FRIEND:
        case Buttons::BT_TAB_PARTY:
        case Buttons::BT_TAB_BOSS:
        case Buttons::BT_TAB_BLACKLIST:
            change_tab(static_cast<uint8_t>(buttonid));
            return Button::State::PRESSED;
        case Buttons::BT_TAB_PARTY_MINE:
        case Buttons::BT_TAB_PARTY_SEARCH:
            change_party_tab(static_cast<uint8_t>(buttonid));
            return Button::State::PRESSED;
        case Buttons::BT_TAB_FRIEND_ALL:
            change_friend_tab(Tab::FRIEND_ALL);
            return Button::State::IDENTITY;
        case Buttons::BT_TAB_FRIEND_ONLINE:
            change_friend_tab(Tab::FRIEND_ONLINE);
            return Button::State::IDENTITY;
        case Buttons::BT_TAB_BLACKLIST_INDIVIDUAL:
        case Buttons::BT_TAB_BLACKLIST_GUILD:
            change_blacklist_tab(static_cast<uint8_t>(buttonid));
            return Button::State::PRESSED;
        case Buttons::BT_FRIEND_ADD:
            close_side_menu();
            open_add_popup(false);
            return Button::State::NORMAL;
        case Buttons::BT_FRIEND_ADD_GROUP:
            close_side_menu();
            if (selected_cid_ != 0
                && Stage::get().get_player().get_buddylist().find_by_cid(
                       selected_cid_)) {
                open_add_popup(true);
            }
            return Button::State::IDENTITY;
        case Buttons::BT_FRIEND_EXPAND:
            close_side_menu();
            UI::get().emplace<UIOk>(
                "Speak to Mr. Goldstein in Lith Harbor to increase your Buddy "
                "List capacity.",
                [](bool) {});
            return Button::State::NORMAL;
        case Buttons::BT_FRIEND_SIDE_DELETE: {
            int32_t cid = selected_cid_;
            std::string target_name;
            if (const BuddyEntry *entry =
                    Stage::get().get_player().get_buddylist().find_by_cid(
                        cid)) {
                target_name = entry->name;
            }
            close_side_menu();
            if (cid != 0 && !target_name.empty()) {
                UI::get().emplace<UIYesNo>(
                    "Are you sure you want to delete \"" + target_name
                        + "\" from your Buddy List?",
                    [cid](bool yes) {
                        if (yes) {
                            DeleteBuddyPacket(cid).dispatch();
                        }
                    });
            }
            return Button::State::NORMAL;
        }
        case Buttons::BT_ADD_OK:
            submit_add_popup();
            return Button::State::NORMAL;
        case Buttons::BT_ADD_CANCEL:
            close_add_popup();
            return Button::State::NORMAL;
        default: return Button::State::NORMAL;
    }

    return Button::State::NORMAL;
}

void UIUserList::change_tab(uint8_t tabid) {
    close_add_popup();
    close_side_menu();

    uint8_t oldtab = static_cast<uint8_t>(tab_);
    tab_ = tabid;

    background_ = tabid == Buttons::BT_TAB_BOSS
                      ? user_list_["Main"]["Boss"]["backgrnd3"]
                      : user_list_["Main"]["backgrnd2"];

    if (oldtab != tab_) {
        buttons_[Buttons::BT_TAB_FRIEND + oldtab]->set_state(
            Button::State::NORMAL);
    }

    buttons_[Buttons::BT_TAB_FRIEND + tab_]->set_state(Button::State::PRESSED);

    if (tab_ == Buttons::BT_TAB_PARTY) {
        buttons_[Buttons::BT_PARTY_CREATE]->set_active(true);
        buttons_[Buttons::BT_PARTY_INVITE]->set_active(true);
        buttons_[Buttons::BT_TAB_PARTY_MINE]->set_active(true);
        buttons_[Buttons::BT_TAB_PARTY_SEARCH]->set_active(true);

        change_party_tab(Tab::PARTY_MINE);
    } else {
        buttons_[Buttons::BT_PARTY_CREATE]->set_active(false);
        buttons_[Buttons::BT_PARTY_INVITE]->set_active(false);
        buttons_[Buttons::BT_TAB_PARTY_MINE]->set_active(false);
        buttons_[Buttons::BT_TAB_PARTY_SEARCH]->set_active(false);
        buttons_[Buttons::BT_PARTY_SEARCH_LEVEL]->set_active(false);
    }

    if (tab_ == Buttons::BT_TAB_FRIEND) {
        update_location_text();
        change_friend_tab(static_cast<uint8_t>(friend_tab_));
    } else {
        buttons_[Buttons::BT_FRIEND_ADD]->set_active(false);
        buttons_[Buttons::BT_FRIEND_ADD_GROUP]->set_active(false);
        buttons_[Buttons::BT_FRIEND_EXPAND]->set_active(false);
        buttons_[Buttons::BT_TAB_FRIEND_ALL]->set_active(false);
        buttons_[Buttons::BT_TAB_FRIEND_ONLINE]->set_active(false);
        buttons_[Buttons::BT_FRIEND_SIDE_DELETE]->set_active(false);
    }

    if (tab_ == Buttons::BT_TAB_BOSS) {
        buttons_[Buttons::BT_BOSS_0]->set_active(true);
        buttons_[Buttons::BT_BOSS_1]->set_active(true);
        buttons_[Buttons::BT_BOSS_2]->set_active(true);
        buttons_[Buttons::BT_BOSS_3]->set_active(true);
        buttons_[Buttons::BT_BOSS_4]->set_active(true);
        buttons_[Buttons::BT_BOSS_L]->set_active(true);
        buttons_[Buttons::BT_BOSS_R]->set_active(true);
        buttons_[Buttons::BT_BOSS_DIFF_L]->set_active(true);
        buttons_[Buttons::BT_BOSS_DIFF_R]->set_active(true);
        buttons_[Buttons::BT_BOSS_GO]->set_active(true);
        buttons_[Buttons::BT_BOSS_L]->set_state(Button::State::DISABLED);
        buttons_[Buttons::BT_BOSS_R]->set_state(Button::State::DISABLED);
        buttons_[Buttons::BT_BOSS_GO]->set_state(Button::State::DISABLED);
        buttons_[Buttons::BT_BOSS_DIFF_L]->set_state(Button::State::DISABLED);
        buttons_[Buttons::BT_BOSS_DIFF_R]->set_state(Button::State::DISABLED);
    } else {
        buttons_[Buttons::BT_BOSS_0]->set_active(false);
        buttons_[Buttons::BT_BOSS_1]->set_active(false);
        buttons_[Buttons::BT_BOSS_2]->set_active(false);
        buttons_[Buttons::BT_BOSS_3]->set_active(false);
        buttons_[Buttons::BT_BOSS_4]->set_active(false);
        buttons_[Buttons::BT_BOSS_L]->set_active(false);
        buttons_[Buttons::BT_BOSS_R]->set_active(false);
        buttons_[Buttons::BT_BOSS_DIFF_L]->set_active(false);
        buttons_[Buttons::BT_BOSS_DIFF_R]->set_active(false);
        buttons_[Buttons::BT_BOSS_GO]->set_active(false);
    }

    if (tab_ == Buttons::BT_TAB_BLACKLIST) {
        buttons_[Buttons::BT_BLACKLIST_ADD]->set_active(true);
        buttons_[Buttons::BT_BLACKLIST_DELETE]->set_active(true);
        buttons_[Buttons::BT_TAB_BLACKLIST_INDIVIDUAL]->set_active(true);
        buttons_[Buttons::BT_TAB_BLACKLIST_GUILD]->set_active(true);
    } else {
        buttons_[Buttons::BT_BLACKLIST_ADD]->set_active(false);
        buttons_[Buttons::BT_BLACKLIST_DELETE]->set_active(false);
        buttons_[Buttons::BT_TAB_BLACKLIST_INDIVIDUAL]->set_active(false);
        buttons_[Buttons::BT_TAB_BLACKLIST_GUILD]->set_active(false);
    }
}

uint16_t UIUserList::get_tab() {
    return tab_;
}

void UIUserList::change_party_tab(uint8_t tabid) {
    uint8_t oldtab = static_cast<uint8_t>(party_tab_);
    party_tab_ = tabid;

    if (oldtab != party_tab_) {
        buttons_[Buttons::BT_TAB_FRIEND + oldtab]->set_state(
            Button::State::NORMAL);
    }

    buttons_[Buttons::BT_TAB_FRIEND + party_tab_]->set_state(
        Button::State::PRESSED);

    if (party_tab_ == Buttons::BT_TAB_PARTY_SEARCH) {
        buttons_[Buttons::BT_PARTY_SEARCH_LEVEL]->set_active(true);
    } else {
        buttons_[Buttons::BT_PARTY_SEARCH_LEVEL]->set_active(false);
    }
}

void UIUserList::change_friend_tab(uint8_t tabid) {
    friend_tab_ = tabid;
    close_side_menu();
    friend_scroll_offset_ = 0;
    rebuild_friend_rows();
}

void UIUserList::change_blacklist_tab(uint8_t tabid) {
    uint8_t oldtab = static_cast<uint8_t>(blacklist_tab_);
    blacklist_tab_ = tabid;

    if (oldtab != blacklist_tab_) {
        buttons_[Buttons::BT_TAB_FRIEND + oldtab]->set_state(
            Button::State::NORMAL);
    }

    buttons_[Buttons::BT_TAB_FRIEND + blacklist_tab_]->set_state(
        Button::State::PRESSED);
}

void UIUserList::rebuild_friend_rows() {
    const BuddyList &bl = Stage::get().get_player().get_buddylist();
    const auto &buddies = bl.get_buddies();

    friends_online_text_.change_text("(" + std::to_string(buddies.size()) + "/"
                                     + std::to_string(bl.get_capacity()) + ")");

    if (selected_cid_ != 0 && !bl.find_by_cid(selected_cid_)) {
        selected_cid_ = 0;
        close_side_menu();
    }

    std::vector<std::string> group_order;
    std::map<std::string, std::vector<const BuddyEntry *>> grouped;
    std::map<std::string, int32_t> group_totals;
    std::map<std::string, int32_t> group_onlines;

    for (const auto &entry : buddies) {
        const std::string &group =
            entry.group.empty() ? std::string("Default Group") : entry.group;
        if (grouped.find(group) == grouped.end()) {
            group_order.push_back(group);
        }

        group_totals[group]++;
        if (entry.channel >= 0) {
            group_onlines[group]++;
        }

        if (friend_tab_ == Tab::FRIEND_ALL || entry.channel >= 0) {
            grouped[group].push_back(&entry);
        } else {
            grouped[group];
        }
    }

    friend_rows_.clear();

    if (group_order.empty()) {
        FriendRow empty_group;
        empty_group.is_group = true;
        empty_group.group_name = "Default Group";
        empty_group.collapsed =
            collapsed_groups_.count(empty_group.group_name) > 0;
        empty_group.label = Text(Text::Font::A11M,
                                 Text::Alignment::LEFT,
                                 Color::Name::WHITE,
                                 "Default Group (0/0)",
                                 204);
        friend_rows_.push_back(std::move(empty_group));
    } else {
        for (const auto &group : group_order) {
            const auto &members = grouped[group];
            int32_t online = group_onlines[group];
            int32_t total = group_totals[group];

            if (friend_tab_ == Tab::FRIEND_ONLINE && members.empty()) {
                continue;
            }

            FriendRow g_row;
            g_row.is_group = true;
            g_row.group_name = group;
            g_row.collapsed = collapsed_groups_.count(group) > 0;
            g_row.label =
                Text(Text::Font::A11M,
                     Text::Alignment::LEFT,
                     Color::Name::WHITE,
                     group + " (" + std::to_string(online) + "/"
                         + std::to_string(total) + ")",
                     204);
            bool collapsed = g_row.collapsed;
            friend_rows_.push_back(std::move(g_row));

            if (collapsed) {
                continue;
            }

            for (const BuddyEntry *entry : members) {
                FriendRow b_row;
                b_row.is_group = false;
                b_row.cid = entry->cid;
                b_row.group_name = group;
                b_row.name = entry->name;
                b_row.online = entry->channel >= 0;
                b_row.label = Text(Text::Font::A11M,
                                   Text::Alignment::LEFT,
                                   Color::Name::BLACK,
                                   entry->name,
                                   96);
                std::string ch_text =
                    b_row.online
                        ? ("Channel " + std::to_string(entry->channel + 1))
                        : "Offline";
                b_row.status = Text(
                    Text::Font::A11M,
                    Text::Alignment::LEFT,
                    b_row.online ? Color::Name::BLACK : Color::Name::DARKGREY,
                    ch_text,
                    108);
                friend_rows_.push_back(std::move(b_row));
            }
        }

        if (friend_rows_.empty()) {
            FriendRow empty_online;
            empty_online.is_group = true;
            empty_online.group_name = "Default Group";
            empty_online.label = Text(Text::Font::A11M,
                                      Text::Alignment::LEFT,
                                      Color::Name::WHITE,
                                      "Default Group (0/0)",
                                      204);
            friend_rows_.push_back(std::move(empty_online));
        }
    }

    int16_t max_offset = std::max<int16_t>(
        0, int16_t(friend_rows_.size()) - FRIEND_VISIBLE_ROWS_);
    friend_scroll_offset_ =
        std::clamp<int16_t>(friend_scroll_offset_, 0, max_offset);
    friends_slider_.setrows(friend_scroll_offset_,
                            FRIEND_VISIBLE_ROWS_,
                            std::max<int16_t>(FRIEND_VISIBLE_ROWS_,
                                              int16_t(friend_rows_.size())));

    update_friend_controls();
}

void UIUserList::update_friend_controls() {
    if (tab_ != Buttons::BT_TAB_FRIEND) {
        return;
    }

    if (add_popup_open_) {
        buttons_[Buttons::BT_TAB_FRIEND]->set_active(false);
        buttons_[Buttons::BT_TAB_PARTY]->set_active(false);
        buttons_[Buttons::BT_TAB_BOSS]->set_active(false);
        buttons_[Buttons::BT_TAB_BLACKLIST]->set_active(false);
        buttons_[Buttons::BT_CLOSE]->set_active(false);
        buttons_[Buttons::BT_FRIEND_ADD]->set_active(false);
        buttons_[Buttons::BT_FRIEND_ADD_GROUP]->set_active(false);
        buttons_[Buttons::BT_FRIEND_EXPAND]->set_active(false);
        buttons_[Buttons::BT_TAB_FRIEND_ALL]->set_active(false);
        buttons_[Buttons::BT_TAB_FRIEND_ONLINE]->set_active(false);
        buttons_[Buttons::BT_FRIEND_SIDE_DELETE]->set_active(false);
        buttons_[Buttons::BT_ADD_OK]->set_active(true);
        buttons_[Buttons::BT_ADD_CANCEL]->set_active(true);
        return;
    }

    buttons_[Buttons::BT_TAB_FRIEND]->set_active(true);
    buttons_[Buttons::BT_TAB_PARTY]->set_active(true);
    buttons_[Buttons::BT_TAB_BOSS]->set_active(true);
    buttons_[Buttons::BT_TAB_BLACKLIST]->set_active(true);
    buttons_[Buttons::BT_CLOSE]->set_active(true);

    buttons_[Buttons::BT_ADD_OK]->set_active(false);
    buttons_[Buttons::BT_ADD_CANCEL]->set_active(false);

    buttons_[Buttons::BT_FRIEND_ADD]->set_active(true);
    buttons_[Buttons::BT_FRIEND_ADD]->set_state(Button::State::NORMAL);

    const bool has_selected =
        selected_cid_ != 0
        && Stage::get().get_player().get_buddylist().find_by_cid(
               selected_cid_)
               != nullptr;
    buttons_[Buttons::BT_FRIEND_ADD_GROUP]->set_active(true);
    buttons_[Buttons::BT_FRIEND_ADD_GROUP]->set_state(
        has_selected ? Button::State::NORMAL : Button::State::DISABLED);

    buttons_[Buttons::BT_FRIEND_EXPAND]->set_active(true);
    buttons_[Buttons::BT_FRIEND_EXPAND]->set_state(Button::State::NORMAL);

    buttons_[Buttons::BT_TAB_FRIEND_ALL]->set_active(true);
    buttons_[Buttons::BT_TAB_FRIEND_ALL]->set_state(
        friend_tab_ == Tab::FRIEND_ALL ? Button::State::DISABLED
                                       : Button::State::NORMAL);

    buttons_[Buttons::BT_TAB_FRIEND_ONLINE]->set_active(true);
    buttons_[Buttons::BT_TAB_FRIEND_ONLINE]->set_state(
        friend_tab_ == Tab::FRIEND_ONLINE ? Button::State::DISABLED
                                          : Button::State::NORMAL);

    buttons_[Buttons::BT_FRIEND_SIDE_DELETE]->set_active(side_menu_open_);
}

void UIUserList::close_side_menu() {
    if (!side_menu_open_) {
        return;
    }
    side_menu_open_ = false;
    buttons_[Buttons::BT_FRIEND_SIDE_DELETE]->set_active(false);
}

void UIUserList::open_side_menu(int16_t vis_row, int32_t cid) {
    selected_cid_ = cid;
    int16_t row_y = FRIEND_LIST_Y_ + vis_row * FRIEND_ROW_H_;
    int16_t sm_y = std::min<int16_t>(row_y, 222);
    side_menu_pos_ = Point<int16_t>(150, sm_y);
    side_menu_open_ = true;

    buttons_[Buttons::BT_FRIEND_SIDE_DELETE]->set_position(
        side_menu_pos_ + Point<int16_t>(5, 11));
    buttons_[Buttons::BT_FRIEND_SIDE_DELETE]->set_state(Button::State::NORMAL);
    buttons_[Buttons::BT_FRIEND_SIDE_DELETE]->set_active(true);
}

void UIUserList::open_add_popup(bool edit_group) {
    edit_group_mode_ = edit_group;
    edit_target_name_.clear();
    add_popup_open_ = true;

    Point<int16_t> popup_pos = position_ + ADD_POPUP_OFFSET_;
    add_name_field_.update(popup_pos);
    add_group_field_.update(popup_pos);

    if (edit_group) {
        if (const BuddyEntry *entry =
                Stage::get().get_player().get_buddylist().find_by_cid(
                    selected_cid_)) {
            edit_target_name_ = entry->name;
            edit_name_text_.change_text(entry->name);
            add_group_field_.change_text(entry->group);
        } else {
            edit_group_mode_ = false;
        }
    }

    if (edit_group_mode_) {
        add_hint_text_.change_text(
            "Enter a new group name (max 13 chars) for this buddy and tap OK.");
        add_name_field_.change_text(edit_target_name_);
        add_name_field_.set_state(Textfield::State::DISABLED);
        add_group_field_.set_state(Textfield::State::FOCUSED);
    } else {
        add_hint_text_.change_text(
            "Top: Character Name (4-13 chars)\nMiddle: Group Name (optional)");
        add_name_field_.change_text("");
        add_group_field_.change_text("Default Group");
        add_group_field_.set_state(Textfield::State::NORMAL);
        add_name_field_.set_state(Textfield::State::FOCUSED);
    }

    update_friend_controls();
}

void UIUserList::close_add_popup() {
    if (!add_popup_open_) {
        return;
    }

    add_popup_open_ = false;
    edit_group_mode_ = false;
    edit_target_name_.clear();

    // Return both fields to NORMAL so UI clears focus and hides the Android IME.
    if (add_name_field_.get_state() == Textfield::State::FOCUSED) {
        add_name_field_.set_state(Textfield::State::NORMAL);
    }
    if (add_group_field_.get_state() == Textfield::State::FOCUSED) {
        add_group_field_.set_state(Textfield::State::NORMAL);
    }
    add_name_field_.set_state(Textfield::State::NORMAL);
    add_group_field_.set_state(Textfield::State::NORMAL);

    update_friend_controls();
}

void UIUserList::submit_add_popup() {
    std::string target_name = edit_group_mode_
                                  ? edit_target_name_
                                  : trim_ascii(add_name_field_.get_text());
    std::string group_name = trim_ascii(add_group_field_.get_text());
    if (group_name.empty()) {
        group_name = "Default Group";
    }

    close_add_popup();

    if (target_name.size() < 4 || target_name.size() > 13) {
        UI::get().emplace<UIOk>(
            "Character name must be 4 to 13 characters.",
            [](bool) {});
        return;
    }

    AddBuddyPacket(target_name, group_name).dispatch();
}

void UIUserList::update_location_text() {
    int32_t mapid = Stage::get().get_mapid();
    if (mapid == last_mapid_) {
        return;
    }

    last_mapid_ = mapid;
    friends_cur_location_.change_text("My Location - " + get_cur_location());
}

std::string UIUserList::get_cur_location() const {
    int32_t mapid = Stage::get().get_mapid();
    if (mapid <= 0) {
        return "Unknown";
    }

    NxHelper::Map::MapInfo info = NxHelper::Map::get_map_info_by_id(mapid);
    if (!info.name.empty()) {
        return info.name;
    }
    if (!info.street_name.empty()) {
        return info.street_name;
    }
    return std::to_string(mapid);
}
}  // namespace ms
