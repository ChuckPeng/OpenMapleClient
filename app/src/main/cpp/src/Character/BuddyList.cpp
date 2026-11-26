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
#include "BuddyList.h"

#include <cctype>
#include <utility>

namespace ms {
namespace {
bool iequals_ascii(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i]))
            != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }

    return true;
}
}  // namespace

void BuddyList::set_capacity(uint8_t capacity) {
    capacity_ = capacity;
}

uint8_t BuddyList::get_capacity() const {
    return capacity_;
}

void BuddyList::set_buddies(std::vector<BuddyEntry> entries) {
    buddies_ = std::move(entries);
}

const std::vector<BuddyEntry> &BuddyList::get_buddies() const {
    return buddies_;
}

bool BuddyList::update_channel(int32_t cid,
                               int32_t channel,
                               std::string *out_name,
                               bool *out_changed) {
    for (auto &entry : buddies_) {
        if (entry.cid == cid) {
            bool was_online = entry.channel >= 0;
            bool now_online = channel >= 0;
            bool changed = (entry.channel != channel)
                           && (was_online != now_online || now_online);
            entry.channel = channel;

            if (out_name) {
                *out_name = entry.name;
            }
            if (out_changed) {
                *out_changed = changed;
            }
            return true;
        }
    }

    if (out_changed) {
        *out_changed = false;
    }
    return false;
}

const BuddyEntry *BuddyList::find_by_cid(int32_t cid) const {
    for (const auto &entry : buddies_) {
        if (entry.cid == cid) {
            return &entry;
        }
    }
    return nullptr;
}

const BuddyEntry *BuddyList::find_by_name(const std::string &name) const {
    for (const auto &entry : buddies_) {
        if (iequals_ascii(entry.name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

int32_t BuddyList::get_online_count() const {
    int32_t online = 0;
    for (const auto &entry : buddies_) {
        if (entry.channel >= 0) {
            ++online;
        }
    }
    return online;
}
}  // namespace ms
