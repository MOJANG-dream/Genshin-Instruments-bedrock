#pragma once

#include <span>
#include <string>
#include <string_view>

namespace gi {

// The three-octave instruments use a 3 column x 7 row note grid (three
// octaves, C..B). Column 0 is the lowest octave, column 2 the highest.
inline constexpr int kRows  = 7;
inline constexpr int kCols  = 3;
inline constexpr int kCells = kRows * kCols;

struct InstrumentDef {
    std::string_view id;          // item id suffix, e.g. "windsong_lyre"
    std::string_view displayName; // English display name
    std::string_view soundGroup;  // sound definition group, e.g. "windsong_lyre"
    int              sampleCount; // number of recorded samples
    bool             percussion;  // drums: no octave wrapping, no transposition
    bool             named;       // samples are named ("don"/"ka") instead of numbered
    bool             hasStereo;   // a *_stereo take of every sample exists
    // Grid shape. rowMajor grids number their cells left to right, top to
    // bottom (the djem djem drum: its eight samples are recorded in that
    // order); the three-octave grids number them bottom up inside a column.
    int  rows;
    int  cols;
    bool rowMajor;
};

std::span<InstrumentDef const> allInstruments();

InstrumentDef const* findInstrument(std::string_view id);
InstrumentDef const& instrumentAt(int index);
int                  instrumentIndex(std::string_view id);

inline int cellsOf(InstrumentDef const& def) { return def.rows * def.cols; }
inline int rowsOf(InstrumentDef const& def) { return def.rows; }
inline int colsOf(InstrumentDef const& def) { return def.cols; }

// The cell a grid position maps to. Row 0 is always the top row on screen.
inline int cellAt(InstrumentDef const& def, int col, int row) {
    return def.rowMajor ? row * def.cols + col : col * def.rows + (def.rows - 1 - row);
}
inline int colForCell(InstrumentDef const& def, int cell) {
    return def.rowMajor ? cell % def.cols : cell / def.rows;
}
inline int rowForCell(InstrumentDef const& def, int cell) {
    return def.rowMajor ? cell / def.cols : def.rows - 1 - (cell % def.rows);
}

// Sound definition key for a grid cell, e.g. "genshin.windsong_lyre.14".
// Percussion instruments with named samples use "genshin.glorious_drum.don".
std::string soundNameFor(InstrumentDef const& def, int cell, bool stereo = false);

// Playback pitch multiplier for a cell: short sample sets are wrapped by
// octaves (cell 8 of an 8 sample instrument is sample 0 one octave up).
float pitchFor(InstrumentDef const& def, int cell, int transposeOctaves);

// Keyboard / mouse mapping. Returns the cell for a Win32 virtual key code,
// or -1 when the key is not part of this instrument's keyboard.
int cellForKeyCode(InstrumentDef const& def, int vkCode);

// One character label per cell, following the physical layout:
// Z X C V B N M / A S D F G H J / Q W E R T Y U for the three-octave grid,
// A S D F / Q W E R for the djem djem drum.
char const* keyLabelFor(InstrumentDef const& def, int cell);

// The note name printed on the key, as the Java mod prints it: C D E F G A B for
// the three-octave grid (row 0 is the top row, so it starts at B), and the
// recorded names ("C#", "Bb", ...) for the djem djem drum.
char const* noteNameFor(InstrumentDef const& def, int cell);

// Mouse hit test on the rendered grid (GUI units).
int cellForPoint(InstrumentDef const& def, float x, float y, float gridX, float gridY, float cellW, float cellH);

} // namespace gi
