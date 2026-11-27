#include "network/ServerSide.h"

#include "instrument/Instruments.h"
#include "mod/Config.h"
#include "mod/GenshinInstrumentsMod.h"
#include "network/Packets.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerUseItemEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/network/PacketSender.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/network/packet/PlaySoundPacket.h"
#include "mc/world/Minecraft.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/Level.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace gi::network {
namespace {

constexpr int kProtocolVersion = 1;

// Network identifiers of clients that announced themselves with HelloPacket.
// Only those ever receive custom packets, so a vanilla client never sees one.
std::unordered_set<::NetworkIdentifier>& supportedClients() {
    static std::unordered_set<::NetworkIdentifier> clients;
    return clients;
}

::Player* playerFromNetId(::NetworkIdentifier const& netId) {
    auto handler = ll::service::bedrock::getServerNetworkHandler();
    if (!handler) {
        return nullptr;
    }
    return handler->_getServerPlayer(netId, ::SubClientId::PrimaryClient);
}

std::string instrumentItemId(::ItemStack const& stack) {
    for (auto const& def : gi::allInstruments()) {
        std::string id{"genshin:"};
        id += def.id;
        if (stack.isInstance(::HashedString{std::string_view{id}}, false)) {
            return std::string{def.id};
        }
    }
    return {};
}

void relayNote(::Player& player, gi::InstrumentDef const& def, int cell, int transpose, float volume) {
    auto const& config = modConfig();
    if (!config.relayToNearby) {
        return;
    }

    ::PlaySoundPacket sound;
    sound.mName      = gi::soundNameFor(def, cell, config.useStereo);
    sound.mPos       = player.getFeetBlockPos();
    sound.mVolume    = volume * config.relayVolumeScale;
    sound.mPitch     = gi::pitchFor(def, cell, transpose);
    sound.mLoopCount = 0;
    sound.mBypassListenerRangeCheck = false;

    int const  radius   = config.relayRadius;
    float const radius2 = static_cast<float>(radius) * static_cast<float>(radius);
    auto const& origin  = player.getPosition();
    auto const  dim     = player.getDimensionId();

    auto level = ll::service::bedrock::getLevel();
    if (!level) {
        return;
    }
    // Vanilla packets are sent through the server's own sender: ll::network::Packet
    // wraps everything into a RuntimePacket, which only LeviLamina peers understand.
    auto minecraft = ll::service::bedrock::getMinecraft();
    if (!minecraft) {
        return;
    }
    ::PacketSender& sender = minecraft->mPacketSender;

    level->forEachPlayer([&](::Player& other) {
        if (&other == &player) {
            return true;
        }
        if (other.getDimensionId() != dim) {
            return true;
        }
        auto const& pos = other.getPosition();
        float const dx  = pos.x - origin.x;
        float const dy  = pos.y - origin.y;
        float const dz  = pos.z - origin.z;
        if (dx * dx + dy * dy + dz * dz <= radius2) {
            sender.sendToClient(other.getNetworkIdentifier(), sound, ::SubClientId::PrimaryClient);
        }
        return true;
    });
}

// --- packet handlers ---------------------------------------------------------

class HelloHandler final : public ll::network::PacketHandlerBase<HelloHandler, HelloPacket> {
public:
    void handlePacket(::NetworkIdentifier const& netId, ::NetEventCallback&, ::ll::network::PacketSide, HelloPacket const& packet) const {
        if (packet.mProtocol != kProtocolVersion) {
            return;
        }
        if (playerFromNetId(netId) == nullptr) {
            return;
        }
        supportedClients().insert(netId);
    }
};

class PlayNoteHandler final : public ll::network::PacketHandlerBase<PlayNoteHandler, PlayNotePacket> {
public:
    void handlePacket(::NetworkIdentifier const& netId, ::NetEventCallback&, ::ll::network::PacketSide, PlayNotePacket const& packet) const {
        if (!packet.mValid) {
            return;
        }
        auto* player = playerFromNetId(netId);
        if (player == nullptr) {
            return;
        }
        auto const* def = gi::findInstrument(packet.mInstrument);
        if (def == nullptr) {
            return;
        }
        if (packet.mCell < 0 || packet.mCell >= gi::kCells) {
            return;
        }
        relayNote(*player, *def, packet.mCell, packet.mTransposeOctaves, packet.mVolume);
    }
};

class CloseHandler final : public ll::network::PacketHandlerBase<CloseHandler, CloseInstrumentPacket> {
public:
    void handlePacket(::NetworkIdentifier const&, ::NetEventCallback&, ::ll::network::PacketSide, CloseInstrumentPacket const&) const {
        // Nothing to release server side yet; the packet exists so the server can
        // stop tracking a player as "performing" once that is added.
    }
};

std::vector<ll::event::ListenerPtr> gListeners;

void onUseItem(ll::event::player::PlayerUseItemEvent& event) {
    if (!modConfig().enableItemTrigger) {
        return;
    }
    auto const instrument = instrumentItemId(event.item());
    if (instrument.empty()) {
        return;
    }
    auto const& netId = event.self().getNetworkIdentifier();
    if (playerFromNetId(netId) == nullptr) {
        return;
    }
    // A vanilla client would not understand a LeviLamina runtime packet.
    if (!supportedClients().contains(netId)) {
        return;
    }
    // Swallow the use so the item never enters a vanilla use state.
    event.cancel();

    OpenInstrumentPacket packet;
    packet.mInstrument       = instrument;
    packet.mTransposeOctaves = modConfig().transposeOctaves;
    packet.sendTo(event.self());
}

[[maybe_unused]] HelloHandler    gHelloHandler;
[[maybe_unused]] PlayNoteHandler gPlayNoteHandler;
[[maybe_unused]] CloseHandler    gCloseHandler;

} // namespace

bool installServerSide() {
    auto& bus = ll::event::EventBus::getInstance();

    gListeners.emplace_back(bus.emplaceListener<ll::event::player::PlayerUseItemEvent>(onUseItem));

    (void)gHelloHandler;
    (void)gPlayNoteHandler;
    (void)gCloseHandler;

    if (modConfig().enableDebugLog) {
        GenshinInstrumentsMod::getInstance().getSelf().getLogger().info(
            "server side installed ({} instruments)",
            gi::allInstruments().size()
        );
    }
    return true;
}

void uninstallServerSide() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto& listener : gListeners) {
        bus.removeListener(listener);
    }
    gListeners.clear();
    supportedClients().clear();
}

} // namespace gi::network

