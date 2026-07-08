#pragma once

#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace sim {

struct WallJson {
    float minX, minY, maxX, maxY;
};

struct MapJson {
    std::string name;
    float half = 50.0f;
    std::vector<WallJson> walls;
};

inline MapJson parse_map_json(const std::string &text) {
    MapJson map;
    map.half = 50.0f;

    auto skip_ws = [&](size_t &i) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t' ||
                                   text[i] == '\n' || text[i] == '\r'))
            ++i;
    };

    auto expect = [&](size_t &i, char c) {
        skip_ws(i);
        if (i < text.size() && text[i] == c) {
            ++i;
            return true;
        }
        return false;
    };

    auto parse_string = [&](size_t &i) -> std::string {
        skip_ws(i);
        if (i >= text.size() || text[i] != '"')
            return {};
        ++i;
        size_t start = i;
        while (i < text.size() && text[i] != '"')
            ++i;
        std::string s = text.substr(start, i - start);
        if (i < text.size())
            ++i;
        return s;
    };

    auto parse_number = [&](size_t &i) -> float {
        skip_ws(i);
        size_t start = i;
        if (i < text.size() && (text[i] == '-' || text[i] == '+'))
            ++i;
        while (i < text.size() && (std::isdigit(text[i]) || text[i] == '.'))
            ++i;
        return std::atof(text.substr(start, i - start).c_str());
    };

    size_t pos = 0;
    expect(pos, '{');

    while (pos < text.size()) {
        skip_ws(pos);
        if (pos >= text.size() || text[pos] == '}')
            break;
        std::string key = parse_string(pos);
        expect(pos, ':');

        if (key == "name") {
            map.name = parse_string(pos);
        } else if (key == "bounds") {
            expect(pos, '{');
            while (pos < text.size()) {
                skip_ws(pos);
                if (pos >= text.size() || text[pos] == '}') {
                    ++pos;
                    break;
                }
                std::string bk = parse_string(pos);
                expect(pos, ':');
                float bv = parse_number(pos);
                if (bk == "half")
                    map.half = bv;
                expect(pos, ',');
            }
        } else if (key == "walls") {
            expect(pos, '[');
            while (pos < text.size()) {
                skip_ws(pos);
                if (pos >= text.size() || text[pos] == ']') {
                    ++pos;
                    break;
                }
                WallJson w{};
                expect(pos, '{');
                while (pos < text.size()) {
                    skip_ws(pos);
                    if (pos >= text.size() || text[pos] == '}') {
                        ++pos;
                        break;
                    }
                    std::string wk = parse_string(pos);
                    expect(pos, ':');
                    float wv = parse_number(pos);
                    if (wk == "minX")
                        w.minX = wv;
                    else if (wk == "minY")
                        w.minY = wv;
                    else if (wk == "maxX")
                        w.maxX = wv;
                    else if (wk == "maxY")
                        w.maxY = wv;
                    expect(pos, ',');
                }
                map.walls.push_back(w);
                expect(pos, ',');
            }
        }
        expect(pos, ',');
    }
    return map;
}

} // namespace sim
