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
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <limits>

#include "../../Data/ItemData.h"
#include "../../Gameplay/MapleMap/Npc.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/QuestPackets.h"
#include "../../Util/GameInfo.h"
#include "../../Util/StringHandling.h"
#include "../Components/MapleButton.h"
#include "../UI.h"
#include "UIChatBar.h"

namespace ms {
namespace {
bool has_unrenderable_glyphs(const std::string &utf8) {
    for (uint32_t cp : string_conversion::to_utf8_vector(utf8)) {
        if (cp < 256) {
            continue;
        }
        switch (cp) {
            case 0x2018:
            case 0x2019:
            case 0x201C:
            case 0x201D:
            case 0x2013:
            case 0x2014:
            case 0x2026: continue;
            default: return true;
        }
    }

    return false;
}

int64_t safe_get_int(const nl::node &n, int64_t def = 0) {
    switch (n.data_type()) {
        case nl::node::type::integer:
        case nl::node::type::real:
            return n.get_integer(def);
        case nl::node::type::string: {
            std::string s = n.get_string();
            if (s.empty()) {
                return def;
            }
            char *end = nullptr;
            long long v = std::strtoll(s.c_str(), &end, 10);
            if (end == s.c_str() || *end != '\0') {
                return def;
            }
            return static_cast<int64_t>(v);
        }
        default:
            return def;
    }
}

bool is_digits(const std::string &s) {
    return !s.empty()
           && std::all_of(s.begin(), s.end(), [](unsigned char c) {
                  return std::isdigit(c) != 0;
              });
}

std::string to_lower_ascii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string replace_id_tag(const std::string &input,
                           const char *tag,
                           const std::function<std::string(int32_t, const std::string &)> &lookup) {
    std::string out = input;
    const size_t tag_len = std::strlen(tag);
    size_t begin = 0;

    while ((begin = out.find(tag, begin)) != std::string::npos) {
        size_t end = out.find('#', begin + tag_len);

        if (end == std::string::npos) {
            break;
        }

        std::string raw = out.substr(begin + tag_len, end - begin - tag_len);

        if (!is_digits(raw)) {
            begin = end + 1;
            continue;
        }

        std::string replacement;

        try {
            int32_t id = std::stoi(raw);
            replacement = lookup(id, raw);
        } catch (const std::exception &) {
            replacement.clear();
        }

        out.replace(begin, end - begin + 1, replacement);
        begin += replacement.length();
    }

    return out;
}

// Quest descriptions use the same inline markup as npc dialogue.
std::string format_quest_text(const std::string &text) {
    std::string formatted = text;

    formatted.erase(
        std::remove(formatted.begin(), formatted.end(), '\r'),
        formatted.end());

    // #h0# / #h # stand for the player's name.
    {
        std::string player_name = Stage::get().get_player().get_name();
        size_t begin = 0;

        while ((begin = formatted.find("#h", begin)) != std::string::npos) {
            size_t end = formatted.find('#', begin + 2);

            if (end == std::string::npos) {
                break;
            }

            formatted.replace(begin, end - begin + 1, player_name);
            begin += player_name.length();
        }
    }

    formatted = replace_id_tag(formatted, "#t", [](int32_t id, const std::string &) {
        return ItemData::get(id).get_name();
    });
    formatted = replace_id_tag(formatted, "#p", [](int32_t, const std::string &raw) {
        return nl::nx::string["Npc.img"][raw]["name"].get_string();
    });
    formatted = replace_id_tag(formatted, "#o", [](int32_t, const std::string &raw) {
        return nl::nx::string["Mob.img"][raw]["name"].get_string();
    });
    formatted = replace_id_tag(formatted, "#m", [](int32_t id, const std::string &) {
        return NxHelper::Map::get_map_info_by_id(id).name;
    });

    // Tags that would need inline images, icons, or progress counters are
    // dropped together with their argument instead of leaving raw markup.
    for (const char *tag : { "#i", "#v", "#c", "#L", "#a", "#f", "#q", "#u" }) {
        size_t begin = 0;

        while ((begin = formatted.find(tag, begin)) != std::string::npos) {
            size_t end = formatted.find('#', begin + 2);

            if (end == std::string::npos) {
                break;
            }

            formatted.erase(begin, end - begin + 1);
        }
    }

    for (const char *tag : { "#b", "#r", "#k", "#n", "#e", "#g", "#d", "#m" }) {
        size_t begin = 0;

        while ((begin = formatted.find(tag, begin)) != std::string::npos) {
            formatted.erase(begin, 2);
        }
    }

    return formatted;
}

int32_t area_for_map(int32_t map_id) {
    if (map_id < 100000000) {
        return 20;  // Maple Island
    }
    if (map_id < 130000000) {
        return 30;  // Victoria Island
    }
    if (map_id < 140000000) {
        return 15;  // Ereve / Cygnus Knights
    }
    if (map_id < 150000000) {
        return 6;   // Rien
    }
    if (map_id < 220000000 || (map_id >= 230000000 && map_id < 240000000)) {
        return 33;  // El Nath Mt. + Aqua Road
    }
    if (map_id < 230000000) {
        return 37;  // Ludus Lake
    }
    if (map_id < 250000000) {
        return 41;  // Leafre
    }
    if (map_id < 270000000) {
        return 44;  // Mu Lung + Nihal Desert
    }
    if (map_id < 280000000) {
        return 46;  // Temple of Time
    }
    if ((map_id >= 500000000 && map_id < 540000000)
        || (map_id >= 700000000 && map_id < 900000000)) {
        return 48;  // World Tour
    }
    if (map_id >= 540000000 && map_id < 600000000) {
        return 49;  // Singapore / Malaysia
    }
    if (map_id >= 600000000 && map_id < 700000000) {
        return 45;  // Masteria
    }
    return -1;
}

bool is_npc_on_current_map(int32_t npc_id) {
    if (npc_id == 0) {
        return false;
    }

    for (auto &entry : *Stage::get().get_npcs().get_npcs()) {
        const Npc *npc = static_cast<const Npc *>(entry.second.get());

        if (npc && npc->get_id() == npc_id) {
            return true;
        }
    }

    return false;
}

bool check_npc_nearby_with_notice(int32_t npc_id,
                                  bool skip_proximity_check,
                                  const char *action_verb) {
    if (npc_id == 0 || skip_proximity_check || is_npc_on_current_map(npc_id)) {
        return true;
    }

    std::string npcname =
        nl::nx::string["Npc.img"][std::to_string(npc_id)]["name"].get_string();

    if (auto chatbar = UI::get().get_element<UIChatBar>()) {
        chatbar->get().send_chatline(
            "Go to " + npcname + " to " + action_verb + " this quest.",
            UIChatBar::LineType::YELLOW);
    }

    return false;
}

bool parse_quest_id(const std::string &raw, int16_t &qid) {
    if (!is_digits(raw)) {
        return false;
    }

    char *end = nullptr;
    long value = std::strtol(raw.c_str(), &end, 10);

    if (end == raw.c_str() || *end != '\0' || value <= 0
        || value > std::numeric_limits<int16_t>::max()) {
        return false;
    }

    qid = static_cast<int16_t>(value);
    return true;
}

std::string format_today_yyyymmdd() {
    std::time_t now = std::time(nullptr);
    std::tm tm_now{};
    localtime_r(&now, &tm_now);

    char buf[16];
    std::snprintf(buf,
                  sizeof(buf),
                  "%04d%02d%02d",
                  tm_now.tm_year + 1900,
                  tm_now.tm_mon + 1,
                  tm_now.tm_mday);
    return std::string(buf);
}
}  // namespace

UIQuestLog::UIQuestLog(const QuestLog &ql) :
    UIDragElement<PosQUEST>(),
    questlog_(ql),
    offset_(0),
    selected_(-1),
    list_width_(0),
    detail_npc_(0),
    all_levels_(false),
    my_location_(false),
    detail_can_start_(false),
    detail_can_complete_(false),
    detail_auto_start_(false),
    detail_auto_complete_(false),
    detail_scripted_start_(false),
    detail_scripted_end_(false) {
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
    buttons_[Buttons::MY_LEVEL] =
        std::make_unique<MapleButton>(list["BtMyLevel"]);
    buttons_[Buttons::MY_LOCATION] =
        std::make_unique<MapleButton>(list["BtMyLocation"]);
    buttons_[Buttons::ALL_LOCATION] =
        std::make_unique<MapleButton>(list["BtAllLocation"]);

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
    search_.set_enter_callback([this](const std::string &) { rebuild_rows(); });
    search_.set_key_callback(KeyAction::Id::ESCAPE,
                             [this]() { release_search_focus(); });
    placeholder_ = Text(Text::Font::A11M,
                        Text::Alignment::LEFT,
                        Color::Name::BOULDER,
                        "Enter the quest name.");

    slider_ = Slider(Slider::Type::DEFAULT_SILVER,
                     Range<int16_t>(0, 279),
                     150,
                     1,
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
    buttons_[Buttons::INFO_ACCEPT] =
        std::make_unique<MapleButton>(quest_info["BtAccept"], list_width_, 0);

    dimension_ = Point<int16_t>(list_width_ + DETAIL_WIDTH_,
                                std::max<int16_t>(list_dimension.y(),
                                                  DETAIL_HEIGHT_));
    drag_area_ = Point<int16_t>(dimension_.x(), 20);

    update_filter_buttons();
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
            release_search_focus();
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
        if (Cursor::State sstate = slider_.send_cursor(
                cursorpos - position_ - Point<int16_t>(126, 75), clicking)) {
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
            && relative.y() >= LIST_TOP_
            && relative.y() < LIST_TOP_ + VISIBLE_ROWS_ * ROW_HEIGHT_) {
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

bool UIQuestLog::can_start_quest(int16_t qid,
                                 const nl::node &check_zero,
                                 const nl::node &info,
                                 const std::string &today_yyyymmdd) const {
    if (!check_zero) {
        return false;
    }

    if (questlog_.is_started(qid)) {
        return false;
    }

    // Cosmic marks a quest repeatable only when Check.img/<qid>/0 defines an
    // interval node.
    if (questlog_.is_completed(qid) && !check_zero["interval"]) {
        return false;
    }

    std::string end_date = check_zero["end"].get_string();

    if (end_date.size() >= 8) {
        std::string end_day = end_date.substr(0, 8);

        if (is_digits(end_day) && end_day < today_yyyymmdd) {
            return false;
        }
    }

    const Player &player = Stage::get().get_player();
    const CharStats &stats = player.get_stats();
    uint16_t level = stats.get_stat(MapleStat::Id::LEVEL);
    int64_t lvmin = safe_get_int(check_zero["lvmin"]);
    int64_t lvmax = safe_get_int(check_zero["lvmax"]);

    if (level < lvmin || (lvmax > 0 && level > lvmax)) {
        return false;
    }

    if (!all_levels_ && lvmin > 0 && int32_t(level) - int32_t(lvmin) > 10) {
        return false;
    }

    nl::node jobs = check_zero["job"];

    if (jobs && jobs.size() > 0) {
        uint16_t player_job = stats.get_job().get_id();

        if (player_job != 900 && player_job != 910) {
            bool matched = false;

            for (nl::node job_entry : jobs) {
                int64_t req_job = safe_get_int(job_entry, -1);

                if (req_job >= 0 && uint16_t(req_job) == player_job) {
                    matched = true;
                    break;
                }
            }

            if (!matched) {
                return false;
            }
        }
    }

    for (nl::node entry : check_zero["quest"]) {
        auto req_id = int16_t(safe_get_int(entry["id"]));
        auto req_state = int32_t(safe_get_int(entry["state"]));

        if (req_state == 0) {
            if (questlog_.is_started(req_id) || questlog_.is_completed(req_id)) {
                return false;
            }
        } else if (req_state == 1) {
            if (!questlog_.is_started(req_id)) {
                return false;
            }
        } else if (req_state == 2) {
            if (!questlog_.is_completed(req_id)) {
                return false;
            }
        } else {
            return false;
        }
    }

    int64_t req_completed = safe_get_int(check_zero["questComplete"]);

    if (req_completed > 0
        && int64_t(questlog_.get_completed().size()) < req_completed) {
        return false;
    }

    nl::node field_enter = check_zero["fieldEnter"]["0"];

    if (field_enter) {
        int32_t req_map = int32_t(safe_get_int(field_enter, -1));

        if (req_map >= 0 && req_map != Stage::get().get_mapid()) {
            return false;
        }
    }

    const Inventory &inv = player.get_inventory();
    int64_t money = safe_get_int(check_zero["money"]);

    if (money > 0 && inv.get_meso() < money) {
        return false;
    }

    for (nl::node item_entry : check_zero["item"]) {
        int32_t item_id = int32_t(safe_get_int(item_entry["id"]));

        if (item_id == 0) {
            continue;
        }

        int32_t count_needed = int32_t(safe_get_int(item_entry["count"]));
        int32_t count = inv.get_total_item_count(item_id);

        if ((count_needed > 0 && count < count_needed)
            || (count_needed <= 0 && count > 0)) {
            return false;
        }
    }

    if (my_location_) {
        int32_t map_id = Stage::get().get_mapid();
        int32_t npc_id = int32_t(safe_get_int(check_zero["npc"]));
        bool matches_location = is_npc_on_current_map(npc_id);

        if (!matches_location && npc_id != 0) {
            nl::node locations =
                nl::nx::etc["NpcLocation.img"][std::to_string(npc_id)];

            for (nl::node loc : locations) {
                if (int32_t(safe_get_int(loc, -1)) == map_id) {
                    matches_location = true;
                    break;
                }
            }
        }

        if (!matches_location) {
            int32_t map_area = area_for_map(map_id);
            int32_t quest_area = int32_t(safe_get_int(info["area"], -1));

            if (map_area >= 0 && quest_area == map_area) {
                matches_location = true;
            }
        }

        if (!matches_location) {
            return false;
        }
    }

    return true;
}

void UIQuestLog::rebuild_rows() {
    int16_t previous = -1;

    if (selected_ >= 0 && size_t(selected_) < rows_.size()) {
        previous = rows_[selected_].qid;
    }

    rows_.clear();
    offset_ = 0;
    selected_ = -1;

    std::string needle =
        (tab_ != Buttons::TAB2 && !search_.empty())
            ? to_lower_ascii(search_.get_text())
            : std::string();

    auto add = [&](int16_t qid, const std::string &name) {
        // Quests without a name, such as the hidden quests that carry the
        // progress of another one, are not shown.
        if (name.empty()) {
            return;
        }

        if (!needle.empty()
            && to_lower_ascii(name).find(needle) == std::string::npos) {
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

    if (tab_ == Buttons::TAB0) {
        std::string today = format_today_yyyymmdd();
        nl::node check_root = nl::nx::quest["Check.img"];
        nl::node info_root = nl::nx::quest["QuestInfo.img"];
        std::vector<std::pair<int16_t, std::string>> available;

        for (nl::node check_entry : check_root) {
            int16_t qid = 0;
            std::string key = check_entry.name();

            if (!parse_quest_id(key, qid)) {
                continue;
            }

            nl::node info = info_root[key];
            std::string name = info["name"].get_string();

            if (name.empty() || has_unrenderable_glyphs(name)) {
                continue;
            }

            if (can_start_quest(qid, check_entry["0"], info, today)) {
                available.emplace_back(qid, std::move(name));
            }
        }

        std::sort(available.begin(),
                  available.end(),
                  [](const auto &lhs, const auto &rhs) {
                      return lhs.first < rhs.first;
                  });

        for (const auto &entry : available) {
            add(entry.first, entry.second);
        }
    } else if (tab_ == Buttons::TAB1) {
        nl::node info_root = nl::nx::quest["QuestInfo.img"];

        for (const auto &entry : questlog_.get_started()) {
            add(entry.first,
                info_root[std::to_string(entry.first)]["name"].get_string());
        }
    } else if (tab_ == Buttons::TAB2) {
        nl::node info_root = nl::nx::quest["QuestInfo.img"];

        for (const auto &entry : questlog_.get_completed()) {
            add(entry.first,
                info_root[std::to_string(entry.first)]["name"].get_string());
        }
    }

    int16_t rows = int16_t(rows_.size());
    int16_t pages = rows > VISIBLE_ROWS_
                        ? (rows + VISIBLE_ROWS_ - 1) / VISIBLE_ROWS_
                        : 1;

    slider_.setrows(1, pages);
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

void UIQuestLog::toggle_active() {
    UIElement::toggle_active();

    if (is_active()) {
        rebuild_rows();
    } else {
        release_search_focus();
    }
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
        case Buttons::CLOSE:
            release_search_focus();
            deactivate();
            break;
        case Buttons::SEARCH:
            release_search_focus();
            rebuild_rows();
            break;
        case Buttons::ALL_LEVEL:
            all_levels_ = true;
            update_filter_buttons();
            rebuild_rows();
            break;
        case Buttons::MY_LEVEL:
            all_levels_ = false;
            update_filter_buttons();
            rebuild_rows();
            break;
        case Buttons::MY_LOCATION:
            my_location_ = true;
            update_filter_buttons();
            rebuild_rows();
            break;
        case Buttons::ALL_LOCATION:
            my_location_ = false;
            update_filter_buttons();
            rebuild_rows();
            break;
        case Buttons::INFO_CLOSE:
            selected_ = -1;

            clear_detail();

            return Button::State::NORMAL;
        case Buttons::INFO_GIVEUP: give_up(); break;
        case Buttons::INFO_COMPLETE: complete(); break;
        case Buttons::INFO_ACCEPT: accept(); break;
        default: break;
    }

    return Button::State::NORMAL;
}

void UIQuestLog::build_detail() {
    detail_npc_ = 0;
    detail_can_start_ = false;
    detail_can_complete_ = false;
    detail_auto_start_ = false;
    detail_auto_complete_ = false;
    detail_scripted_start_ = false;
    detail_scripted_end_ = false;

    if (selected_ < 0 || size_t(selected_) >= rows_.size()) {
        clear_detail();

        return;
    }

    int16_t qid = rows_[selected_].qid;
    std::string qidstr = std::to_string(qid);

    nl::node info = nl::nx::quest["QuestInfo.img"][qidstr];
    nl::node check = nl::nx::quest["Check.img"][qidstr];

    const char *desc_key =
        tab_ == Buttons::TAB0 ? "0" : (tab_ == Buttons::TAB2 ? "2" : "1");

    detail_title_.change_text(format_quest_text(info["name"].get_string()));
    detail_body_.change_text(
        format_quest_text(info[desc_key].get_string()));
    detail_required_.change_text("");

    bool available = tab_ == Buttons::TAB0;
    bool started = tab_ == Buttons::TAB1;
    bool auto_start = safe_get_int(info["autoStart"]) == 1;
    bool auto_complete = auto_start
                         || safe_get_int(info["autoComplete"]) == 1
                         || safe_get_int(info["autoPreComplete"]) == 1;

    if (available) {
        nl::node check_zero = check["0"];

        detail_npc_ = int32_t(safe_get_int(check_zero["npc"]));
        detail_auto_start_ = auto_start;
        detail_auto_complete_ = auto_complete;
        detail_scripted_start_ = !check_zero["startscript"].get_string().empty();
        detail_can_start_ = detail_npc_ != 0 || detail_auto_start_;

        if (detail_npc_ != 0) {
            std::string npcname =
                nl::nx::string["Npc.img"][std::to_string(detail_npc_)]["name"]
                    .get_string();

            if (!npcname.empty()) {
                detail_required_.change_text("NPC: " + npcname);
            }
        }
    } else if (started) {
        std::string required;

        for (nl::node item : check["1"]["item"]) {
            auto itemid = int32_t(safe_get_int(item["id"]));
            auto count = int32_t(safe_get_int(item["count"]));

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
        detail_npc_ = int32_t(safe_get_int(check["1"]["npc"]));

        if (detail_npc_ == 0) {
            detail_npc_ = int32_t(safe_get_int(check["0"]["npc"]));
        }

        detail_auto_start_ = auto_start;
        detail_auto_complete_ = auto_complete;
        detail_scripted_end_ = !check["1"]["endscript"].get_string().empty();
        detail_can_complete_ = detail_npc_ != 0 || detail_auto_complete_;
    }

    buttons_[Buttons::INFO_CLOSE]->set_active(true);
    buttons_[Buttons::INFO_ACCEPT]->set_active(available && detail_can_start_);
    buttons_[Buttons::INFO_GIVEUP]->set_active(started);
    buttons_[Buttons::INFO_COMPLETE]->set_active(started
                                                 && detail_can_complete_);
}

void UIQuestLog::clear_detail() {
    detail_npc_ = 0;
    detail_can_start_ = false;
    detail_can_complete_ = false;
    detail_auto_start_ = false;
    detail_auto_complete_ = false;
    detail_scripted_start_ = false;
    detail_scripted_end_ = false;

    detail_title_.change_text("");
    detail_body_.change_text("");
    detail_required_.change_text("");

    buttons_[Buttons::INFO_CLOSE]->set_active(false);
    buttons_[Buttons::INFO_ACCEPT]->set_active(false);
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

void UIQuestLog::accept() {
    if (tab_ != Buttons::TAB0 || selected_ < 0
        || size_t(selected_) >= rows_.size()) {
        return;
    }

    if (!check_npc_nearby_with_notice(
            detail_npc_, detail_auto_start_ || detail_auto_complete_, "start")) {
        return;
    }

    int16_t qid = rows_[selected_].qid;
    QuestActionPacket packet(detail_scripted_start_
                                 ? QuestActionPacket::SCRIPTED_START
                                 : QuestActionPacket::START,
                             qid);

    packet.write_npc(detail_npc_, Stage::get().get_player().get_position());
    packet.dispatch();
}

void UIQuestLog::give_up() {
    if (selected_ < 0 || size_t(selected_) >= rows_.size()) {
        return;
    }

    QuestActionPacket packet(QuestActionPacket::FORFEIT, rows_[selected_].qid);

    packet.dispatch();
}

void UIQuestLog::complete() {
    if (tab_ != Buttons::TAB1 || selected_ < 0
        || size_t(selected_) >= rows_.size()) {
        return;
    }

    if (!check_npc_nearby_with_notice(
            detail_npc_, detail_auto_complete_, "finish")) {
        return;
    }

    int16_t qid = rows_[selected_].qid;
    QuestActionPacket packet(detail_scripted_end_
                                 ? QuestActionPacket::SCRIPTED_END
                                 : QuestActionPacket::COMPLETE,
                             qid);

    packet.write_npc(detail_npc_, Stage::get().get_player().get_position());
    packet.dispatch();
}

void UIQuestLog::update_filter_buttons() {
    bool available = tab_ == Buttons::TAB0;

    buttons_[Buttons::ALL_LEVEL]->set_active(available && !all_levels_);
    buttons_[Buttons::MY_LEVEL]->set_active(available && all_levels_);
    buttons_[Buttons::MY_LOCATION]->set_active(available && !my_location_);
    buttons_[Buttons::ALL_LOCATION]->set_active(available && my_location_);
}

void UIQuestLog::release_search_focus() {
    if (search_.get_state() == Textfield::State::FOCUSED) {
        search_.set_state(Textfield::State::NORMAL);
    }
}

void UIQuestLog::change_tab(uint16_t tabid) {
    uint16_t oldtab = tab_;
    tab_ = tabid;

    if (oldtab != tab_) {
        buttons_[Buttons::TAB0 + oldtab]->set_state(Button::State::NORMAL);
        buttons_[Buttons::SEARCH]->set_active(tab_ != Buttons::TAB2);

        if (tab_ == Buttons::TAB2) {
            release_search_focus();
            search_.set_state(Textfield::State::DISABLED);
        } else {
            search_.set_state(Textfield::State::NORMAL);
        }
    }

    update_filter_buttons();
    buttons_[Buttons::TAB0 + tab_]->set_state(Button::State::PRESSED);

    rebuild_rows();
}
}  // namespace ms