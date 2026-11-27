#include "instrument/Instruments.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace gi {
namespace {

constexpr std::array<InstrumentDef, 7> kInstruments{{
    {"windsong_lyre", "Windsong Lyre", "windsong_lyre", 21, false, false, true},
    {"floral_zither", "Floral Zither", "floral_zither_new", 21, false, false, false},
    {"vintage_lyre", "Vintage Lyre", "vintage_lyre", 21, false, false, false},
    {"glorious_drum", "Arataki's Great and Glorious Drum", "glorious_drum", 2, true, true, true},
    {"nightwind_horn", "Nightwind Horn", "nightwind_horn_attack", 14, false, false, true},
    {"ukulele", "Ukulele", "ukulele", 21, false, false, false},
    {"djem_djem_drum", "Djem Djem Drum", "djem_djem_drum", 8, true, false, false},
}};

// Cells 0..6 -> bottom row (lowest octave), 7..13 -> middle, 14..20 -> top.
constexpr std::array<int, kCells> kKeyCodes{{
    'Z', 'X', 'C', 'V', 'B', 'N', 'M',
    'A', 'S', 'D', 'F', 'G', 'H', 'J',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U',
}};

constexpr std::array<char const*, kCells> kKeyLabels{{
    "Z", "X", "C", "V", "B", "N", "M",
    "A", "S", "D", "F", "G", "H", "J",
    "Q", "W", "E", "R", "T", "Y", "U",
}};

} // namespace

std::span<InstrumentDef const> allInstruments() { return kInstruments; }

InstrumentDef const* findInstrument(std::string_view id) {
    for (auto const& def : kInstruments) {
        if (def.id == id) {
            return &def;
        }
    }
    return nullptr;
}

InstrumentDef const& instrumentAt(int index) {
    if (index < 0 || index >= static_cast<int>(kInstruments.size())) {
        return kInstruments.front();
    }
    return kInstruments[static_cast<size_t>(index)];
}

int instrumentIndex(std::string_view id) {
    for (size_t i = 0; i < kInstruments.size(); ++i) {
        if (kInstruments[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string soundNameFor(InstrumentDef const& def, int cell, bool stereo) {
    std::string name{"genshin."};
    name += def.soundGroup;
    name += '.';

    bool useStereo = stereo && def.hasStereo;
    if (def.named) {
        // Only "ka" ships a stereo take; fall back to the mono sample otherwise.
        bool const isDon = (cell % 2 == 0);
        name += isDon ? "don" : "ka";
        if (isDon) {
            useStereo = false;
        }
    } else {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", cell % def.sampleCount);
        name += buf;
    }

    if (useStereo) {
        name += "_stereo";
    }
    return name;
}

float pitchFor(InstrumentDef const& def, int cell, int transposeOctaves) {
    if (def.named) {
        return 1.0f; // don/ka are one-shots, no pitch maths
    }
    // Sample sets shorter than 21 notes cover the grid octave by octave; drums
    // keep their register instead of following Shift/Alt.
    int const wrap      = cell / def.sampleCount;
    int const octaves   = wrap + (def.percussion ? 0 : transposeOctaves);
    return std::pow(2.0f, static_cast<float>(12 * octaves) / 12.0f);
}

int cellForKeyCode(int vkCode) {
    for (int i = 0; i < kCells; ++i) {
        if (kKeyCodes[static_cast<size_t>(i)] == vkCode) {
            return i;
        }
    }
    return -1;
}

char const* keyLabelFor(int cell) {
    if (cell < 0 || cell >= kCells) {
        return "";
    }
    return kKeyLabels[static_cast<size_t>(cell)];
}

int cellForPoint(float x, float y, float gridX, float gridY, float cellW, float cellH) {
    if (cellW <= 0.f || cellH <= 0.f) {
        return -1;
    }
    float const lx = x - gridX;
    float const ly = y - gridY;
    if (lx < 0.f || ly < 0.f) {
        return -1;
    }
    int const col = static_cast<int>(lx / cellW);
    int const row = static_cast<int>(ly / cellH);
    if (col < 0 || col >= kCols || row < 0 || row >= kRows) {
        return -1;
    }
    return cellAt(col, row);
}

} // namespace gi
