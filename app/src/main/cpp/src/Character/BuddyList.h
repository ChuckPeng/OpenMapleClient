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

#include <cstdint>
#include <string>
#include <vector>

namespace ms {
// One friend entry in the player's buddy list.
// Matches Cosmic's wire convention: channel >= 0 is 0-based online channel
// (display = channel + 1), while channel < 0 means offline.
struct BuddyEntry {
    int32_t cid;
    std::string name;
    std::string group;
    int32_t channel;
};

// Stores the character's buddy list capacity and current visible buddies.
class BuddyList {
public:
    void set_capacity(uint8_t capacity);
    uint8_t get_capacity() const;

    void set_buddies(std::vector<BuddyEntry> entries);
    const std::vector<BuddyEntry> &get_buddies() const;

    bool update_channel(int32_t cid,
                        int32_t channel,
                        std::string *out_name = nullptr,
                        bool *out_changed = nullptr);

    const BuddyEntry *find_by_cid(int32_t cid) const;
    const BuddyEntry *find_by_name(const std::string &name) const;
    int32_t get_online_count() const;

private:
    uint8_t capacity_{20};
    std::vector<BuddyEntry> buddies_;
};
}  // namespace ms
