//	This file is part of the continued Journey MMORPG client
//	Copyright (C) 2015-2024  Daniel Allendorf, Ryan Payton
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

#include <cstdio>
#include <ostream>
#include <sstream>
#include <string>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace ms {
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

// Accumulates one line of output and writes it when the temporary is destroyed,
// so that a line can be assembled with the usual stream operators.
class LogLine {
public:
    explicit LogLine(LogLevel level) : level_(level) {}

    LogLine(const LogLine &) = delete;
    LogLine &operator=(const LogLine &) = delete;

    ~LogLine() { flush(); }

    template<typename T>
    LogLine &operator<<(const T &value) {
        stream_ << value;
        return *this;
    }

    LogLine &operator<<(std::ostream &(*manipulator)(std::ostream &)) {
        stream_ << manipulator;
        return *this;
    }

private:
    // Logcat rejects messages over ~4k, so long dumps are split into chunks.
    static constexpr size_t MAX_CHUNK = 3500;

#ifdef __ANDROID__
    int android_priority() const {
        switch (level_) {
            case LogLevel::DEBUG:
                return ANDROID_LOG_DEBUG;
            case LogLevel::INFO:
                return ANDROID_LOG_INFO;
            case LogLevel::WARN:
                return ANDROID_LOG_WARN;
            case LogLevel::ERROR:
                return ANDROID_LOG_ERROR;
        }

        return ANDROID_LOG_DEFAULT;
    }
#endif

    void flush() const {
        const std::string text = stream_.str();

        if (text.empty()) {
            return;
        }

#ifdef __ANDROID__
        static const char *TAG = "OpenMapleClient";

        for (size_t offset = 0; offset < text.size(); offset += MAX_CHUNK) {
            const std::string chunk = text.substr(offset, MAX_CHUNK);
            __android_log_print(android_priority(), TAG, "%s", chunk.c_str());
        }
#else
        std::fputs(text.c_str(), stderr);
        std::fputc('\n', stderr);
#endif
    }

    LogLevel level_;
    std::ostringstream stream_;
};
}  // namespace ms

#define MS_LOG_DEBUG ::ms::LogLine(::ms::LogLevel::DEBUG)
#define MS_LOG_INFO ::ms::LogLine(::ms::LogLevel::INFO)
#define MS_LOG_WARN ::ms::LogLine(::ms::LogLevel::WARN)
#define MS_LOG_ERROR ::ms::LogLine(::ms::LogLevel::ERROR)
