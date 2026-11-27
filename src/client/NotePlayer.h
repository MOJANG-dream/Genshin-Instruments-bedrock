#pragma once

#include "instrument/Instruments.h"

namespace gi::client {

// Plays one note on the local sound engine (zero latency) and, when relay is
// enabled, tells the server so nearby players hear it too.
void playNote(InstrumentDef const& def, int cell, int transposeOctaves, float volumeScale);

// Tells the server the screen was closed.
void notifyScreenClosed();

} // namespace gi::client
