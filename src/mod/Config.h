#pragma once

#include <string>

namespace gi {

// Persisted as <mod>/config/config.json. Keep the field set flat and primitive:
// it is serialised by ll::config, which reflects plain aggregates.
struct Config {
    int version = 1;

    // --- how the instrument screen is opened ---------------------------------
    bool enableKeybind      = true;  // open with a keyboard shortcut
    int  openKey            = 0x56;  // 'V'
    bool enableCycleKey     = true;  // cycle instruments without opening the UI
    int  cycleKey           = 0x42;  // 'B'
    bool enableItemTrigger  = true;  // right-click an instrument item (needs the BP)
    std::string defaultInstrument = "windsong_lyre";

    // --- playback ------------------------------------------------------------
    bool  playLocally       = false; // client plays its own notes for zero latency
    bool  relayToNearby     = true;  // server echoes notes to nearby players
    bool  relayToSelf       = true;  // ...including the player who played it
    float relayVolumeScale  = 1.0f;
    float localVolumeScale  = 1.0f;
    int   relayRadius       = 32;   // blocks
    bool  useStereo         = false;

    // --- screen --------------------------------------------------------------
    bool showKeyLabels      = true;
    bool showBackground     = true;
    int  transposeOctaves   = 0;    // -2..2, added on top of the 3 octave grid

    bool enableDebugLog     = false;
};

Config& modConfig();

} // namespace gi
