#pragma once

#include <span>
#include <string>
#include <string_view>

namespace gi {

// The Java mod uses a 3 column x 7 row note grid (three octaves, C..B).
// Column 0 is the lowest octave, column 2 the highest.
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
};

std::span<InstrumentDef const> allInstruments();

InstrumentDef const* findInstrument(std::string_view id);
InstrumentDef const& instrumentAt(int index);
int                  instrumentIndex(std::string_view id);

// Sound definition key for a grid cell, e.g. "genshin.windsong_lyre.14".
// Percussion instruments with named samples use "genshin.glorious_drum.don".
std::string soundNameFor(InstrumentDef const& def, int cell, bool stereo = false);

// Playback pitch multiplier for a cell: short sample sets are wrapped by
// octaves (cell 8 of an 8 sample instrument is sample 0 one octave up).
float pitchFor(InstrumentDef const& def, int cell, int transposeOctaves);

// Grid geometry. A column is one octave and a row one note; columns run left to
// right from the lowest octave to the highest, and inside a column pitch
// ascends upward (bottom row = C, top row = B), exactly like the three staves
// of the Java mod.
//
//   row 0   M  J  U
//   ...
//   row 6   Z  A  Q
inline int cellAt(int col, int row) { return col * kRows + (kRows - 1 - row); }
inline int colForCell(int cell) { return cell / kRows; }
inline int rowForCell(int cell) { return kRows - 1 - (cell % kRows); }

// Keyboard / mouse mapping. Returns the cell for a Win32 virtual key code,
// or -1 when the key is not part of the instrument keyboard.
int cellForKeyCode(int vkCode);

// One character label per cell, following the physical layout:
// Z X C V B N M / A S D F G H J / Q W E R T Y U.
char const* keyLabelFor(int cell);

// Mouse hit test on the rendered grid (GUI units).
int cellForPoint(float x, float y, float gridX, float gridY, float cellW, float cellH);

} // namespace gi
