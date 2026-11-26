//	This file is part of MSClient.
//	Copyright (C) 2020 filoper
//
//	MSClient is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	MSClient is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Affero General Public License for more details.
//
//	You should have received a copy of the GNU Affero General Public License
//	along with MSClient.  If not, see <https://www.gnu.org/licenses/>.

#include "StringHandling.h"

#include <sstream>

namespace ms::string_conversion {
std::string to_utf8_string(uint32_t value) {
    //     0x00000000 - 0x0000007F:
    //        0xxxxxxx

    //    0x00000080 - 0x000007FF:
    //        110xxxxx 10xxxxxx

    //    0x00000800 - 0x0000FFFF:
    //        1110xxxx 10xxxxxx 10xxxxxx

    //    0x00010000 - 0x001FFFFF:
    //        11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

    if (value < 0x00000080) {
        std::stringstream ss;
        char c = static_cast<char>(value);
        ss << c;
        return ss.str();
    }

    if (value < 0x00000800) {
        std::stringstream ss;
        unsigned char byte1 = 0b11000000;
        unsigned char byte0 = 0b10000000;

        uint32_t aa = value & uint32_t(0b00000000000000000000011111000000);
        uint32_t bb = value & uint32_t(0b00000000000000000000000000111111);

        aa >>= uint32_t(6);

        byte1 |= aa;
        byte0 |= bb;

        ss << byte1;
        ss << byte0;

        return ss.str();
    }

    if (value < 0x00010000) {
        std::stringstream ss;
        unsigned char byte2 = 0b11100000;
        unsigned char byte1 = 0b10000000;
        unsigned char byte0 = 0b10000000;

        uint32_t aa = value & uint32_t(0b1111000000000000);
        uint32_t bb = value & uint32_t(0b0000111111000000);
        uint32_t cc = value & uint32_t(0b0000000000111111);

        aa >>= uint32_t(11);
        bb >>= uint32_t(6);

        byte2 |= aa;
        byte1 |= bb;
        byte0 |= cc;

        ss << byte2;
        ss << byte1;
        ss << byte0;

        return ss.str();
    }

    std::stringstream ss;
    unsigned char byte3 = 0b11110000;
    unsigned char byte2 = 0b10000000;
    unsigned char byte1 = 0b10000000;
    unsigned char byte0 = 0b10000000;

    uint32_t aa = value & uint32_t(0b00000000000111000000000000000000);
    uint32_t bb = value & uint32_t(0b00000000000000111111000000000000);
    uint32_t cc = value & uint32_t(0b00000000000000000000111111000000);
    uint32_t dd = value & uint32_t(0b00000000000000000000000000111111);

    aa >>= uint32_t(19);
    bb >>= uint32_t(12);
    cc >>= uint32_t(6);

    byte3 |= aa;
    byte2 |= bb;
    byte1 |= cc;
    byte0 |= dd;

    ss << byte3;
    ss << byte2;
    ss << byte1;
    ss << byte0;

    return ss.str();
}

std::vector<uint32_t> to_utf8_vector(const std::string &text) {
    std::vector<uint32_t> vec;

    for (size_t i = 0; i < text.length(); i++) {
        uint32_t val = '?';
        auto b0 = static_cast<uint8_t>(text[i]);

        if ((b0 & 0b11111000u) == 0b11110000u && i + 3 < text.length()) {
            val = uint32_t((b0 & 0b00000111u) << 18)
                  | uint32_t((static_cast<uint8_t>(text[i + 1]) & 0b00111111u) << 12)
                  | uint32_t((static_cast<uint8_t>(text[i + 2]) & 0b00111111u) << 6)
                  | uint32_t(static_cast<uint8_t>(text[i + 3]) & 0b00111111u);

            i += 3;
        } else if ((b0 & 0b11110000u) == 0b11100000u && i + 2 < text.length()) {
            uint32_t hi = uint8_t(b0 << 4);
            uint32_t lo1 = static_cast<uint8_t>(text[i + 1]) & 0b00111111u;
            uint32_t lo0 = static_cast<uint8_t>(text[i + 2]) & 0b00111111u;

            val = hi << 8;
            val |= (lo1 << 6);
            val |= lo0;

            i += 2;
        } else if ((b0 & 0b11100000u) == 0b11000000u && i + 1 < text.length()) {
            uint32_t hi = uint8_t(b0 << 3);
            uint32_t lo = static_cast<uint8_t>(text[i + 1]) & 0b00111111u;

            val = hi << 3;
            val |= lo;

            i += 1;
        } else if ((b0 & 0b10000000u) == 0) {
            val = b0;  // 1 byte character
        }

        vec.emplace_back(val);
    }

    return vec;
}
}  // namespace ms::string_conversion