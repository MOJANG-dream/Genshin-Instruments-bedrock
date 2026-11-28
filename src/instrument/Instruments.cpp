#include "instrument/Instruments.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace gi {
namespace {

// rows / cols / rowMajor are the grid shape; see the header for the two layouts.
constexpr std::array<InstrumentDef, 7> kInstruments{{
    {"windsong_lyre", "Windsong Lyre", "windsong_lyre", 21, false, false, true, 7, 3, false},
    {"floral_zither", "Floral Zither", "floral_zither_new", 21, false, false, false, 7, 3, false},
    {"vintage_lyre", "Vintage Lyre", "vintage_lyre", 21, false, false, false, 7, 3, false},
    {"glorious_drum", "Arataki's Great and Glorious Drum", "glorious_drum", 2, true, true, true, 7, 3, false},
    {"nightwind_horn", "Nightwind Horn", "nightwind_horn_attack", 14, false, false, true, 7, 3, false},
    {"ukulele", "Ukulele", "ukulele", 21, false, false, false, 7, 3, false},
    // Two rows of four: the samples run left to right, top row first.
    {"djem_djem_drum", "Djem Djem Drum", "djem_djem_drum", 8, true, false, false, 2, 4, true},
}};

// Three-octave grid: cells 0..6 are the bottom row (lowest octave), 7..13 the
// middle and 14..20 the top one.
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

// Djem djem drum: the Java mod maps its top row to A S D F and its bottom row
// to Q W E R (InstrumentKeyMappings.GRID_INSTRUMENT_MAPPINGS[1 - row][column]).
constexpr std::array<int, 8> kDjemKeyCodes{{'A', 'S', 'D', 'F', 'Q', 'W', 'E', 'R'}};

constexpr std::array<char const*, 8> kDjemKeyLabels{{"A", "S", "D", "F", "Q", "W", "E", "R"}};

bool usesDjemLayout(InstrumentDef const& def) { return def.rowMajor; }

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
    // A grid with more cells than samples repeats the samples octave by octave;
    // drums keep their register instead of following Shift/Alt. The djem djem
    // drum has exactly one sample per cell, so nothing is transposed there.
    int const wrap    = cell / def.sampleCount;
    int const octaves = wrap + (def.percussion ? 0 : transposeOctaves);
    return std::pow(2.0f, static_cast<float>(12 * octaves) / 12.0f);
}

int cellForKeyCode(InstrumentDef const& def, int vkCode) {
    int const count = cellsOf(def);
    if (usesDjemLayout(def)) {
        for (int i = 0; i < count; ++i) {
            if (kDjemKeyCodes[static_cast<size_t>(i)] == vkCode) {
                return i;
            }
        }
        return -1;
    }
    for (int i = 0; i < count; ++i) {
        if (kKeyCodes[static_cast<size_t>(i)] == vkCode) {
            return i;
        }
    }
    return -1;
}

char const* keyLabelFor(InstrumentDef const& def, int cell) {
    if (cell < 0 || cell >= cellsOf(def)) {
        return "";
    }
    if (usesDjemLayout(def)) {
        return kDjemKeyLabels[static_cast<size_t>(cell)];
    }
    return kKeyLabels[static_cast<size_t>(cell)];
}

int cellForPoint(InstrumentDef const& def, float x, float y, float gridX, float gridY, float cellW, float cellH) {
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
    if (col < 0 || col >= def.cols || row < 0 || row >= def.rows) {
        return -1;
    }
    return cellAt(def, col, row);
}

} // namespace gi
