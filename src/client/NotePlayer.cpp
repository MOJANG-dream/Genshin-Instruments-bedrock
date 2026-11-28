#include "client/NotePlayer.h"

#ifdef LL_PLAT_C

#include "mod/Config.h"
#include "network/Packets.h"

#include "ll/api/io/Logger.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/sound/SoundEngine.h"

#include <cstdio>
#include <string>
#include <string_view>


namespace gi::client {
namespace {

// Temporary diagnostics (config.enableDebugLog): a note that never starts has to
// show which event name was asked for and what the engine answered.
void diag(std::string_view what) {
    if (auto mod = ll::mod::NativeMod::current()) {
        mod->getLogger().info("[gi] {}", what);
    }
}

std::string noteLine(std::string const& name, float volume, float pitch, uint64 handle) {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), " (vol %.2f, pitch %.3f, handle %llu)",
                  static_cast<double>(volume), static_cast<double>(pitch),
                  static_cast<unsigned long long>(handle));
    return "note " + name + buffer;
}

float noteVolume(float volumeScale) {
    float volume = modConfig().localVolumeScale * volumeScale;
    if (volume < 0.0f) {
        volume = 0.0f;
    }
    if (volume > 2.0f) {
        volume = 2.0f;
    }
    return volume;
}

} // namespace

void playNote(InstrumentDef const& def, int cell, int transposeOctaves, float volumeScale) {
    auto const& config = modConfig();
    float const volume = noteVolume(volumeScale);

    if (config.playLocally) {
        auto client = ll::service::bedrock::getClientInstance();
        if (!client) {
            diag("note: no client instance");
        } else {
            // playUI takes the three values directly; PlaySoundOptions cannot be
            // copied across the module boundary (its ServerSoundHandle member is
            // not exported), so the struct overload is unusable from a mod.
            auto const name   = gi::soundNameFor(def, cell, config.useStereo);
            auto const pitch  = gi::pitchFor(def, cell, transposeOctaves);
            auto       engine = client->getSoundEngine();
            uint64 const handle = engine->playUI(name, volume, pitch);
            diag(noteLine(name, volume, pitch, handle));
        }
    }

    if (config.relayToNearby) {
        net::PlayNotePacket packet;
        packet.mInstrument       = std::string{def.id};
        packet.mCell             = cell;
        packet.mTransposeOctaves = transposeOctaves;
        packet.mVolume           = volume;
        packet.sendToServer();
    }
}

void notifyScreenClosed() {
    net::CloseInstrumentPacket packet;
    packet.sendToServer();
}

} // namespace gi::client

#endif // LL_PLAT_C
