#include "client/NotePlayer.h"

#ifdef LL_PLAT_C

#include "mod/Config.h"
#include "network/Packets.h"

#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/sound/SoundEngine.h"


namespace gi::client {
namespace {

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
        if (client) {
            // playUI takes the three values directly; PlaySoundOptions cannot be
            // copied across the module boundary (its ServerSoundHandle member is
            // not exported), so the struct overload is unusable from a mod.
            auto engine = client->getSoundEngine();
            engine->playUI(
                gi::soundNameFor(def, cell, config.useStereo),
                volume,
                gi::pitchFor(def, cell, transposeOctaves)
            );
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
