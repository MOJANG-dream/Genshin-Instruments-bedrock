#include "client/ClientSide.h"

#ifdef LL_PLAT_C

#include "client/InstrumentScreen.h"
#include "instrument/Instruments.h"
#include "mod/Config.h"
#include "network/Packets.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/io/Logger.h"
#include "ll/api/mod/NativeMod.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace gi::client {
namespace {

std::vector<ll::event::ListenerPtr> gListeners;
int                                 gCycleIndex = 0;

// Temporary diagnostics (config.enableDebugLog): the key path is the one that
// silently does nothing on a real client, so it reports every press it sees.
void diagKey(std::string_view what) {
    if (auto mod = ll::mod::NativeMod::current()) {
        mod->getLogger().info("[gi] {}", what);
    }
}

std::string hexKey(int key) {
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "0x%02X", key);
    return buffer;
}

class OpenInstrumentHandler final
: public ll::network::PacketHandlerBase<OpenInstrumentHandler, net::OpenInstrumentPacket> {
public:
    void handlePacket(::NetworkIdentifier const&, ::NetEventCallback&, ::ll::network::PacketSide, net::OpenInstrumentPacket const& packet) const {
        diagKey("open packet: " + packet.mInstrument);
        if (gi::findInstrument(packet.mInstrument) == nullptr) {
            return;
        }
        // InstrumentScreen::open forwards to the client thread by itself.
        instrumentScreen().open(packet.mInstrument, packet.mTransposeOctaves);
    }
};

void onClientJoinLevel(ll::event::client::ClientJoinLevelEvent&) {
    // Announce that this client understands the mod's custom packets. Servers
    // only send instrument packets to clients that answered.
    net::HelloPacket hello;
    hello.sendToServer();
}

void openWith(std::string_view id) {
    // Re-announce before opening: the handshake sent on join can be lost if the
    // client was still connecting, and the server only opens the UI for clients
    // it knows understand the mod's packets. The server side is idempotent.
    net::HelloPacket hello;
    hello.sendToServer();

    instrumentScreen().open(id, modConfig().transposeOctaves);
}

void onKey(ll::event::input::KeyInputEvent& event) {
    if (!event.isDown()) {
        return;
    }
    if (instrumentScreen().isOpen()) {
        // The screen's own listener consumes keys while it is open.
        return;
    }

    auto const& config = modConfig();
    int const   key    = event.keyCode();
    diagKey("key down " + hexKey(key));

    if (config.enableKeybind && key == config.openKey) {
        event.cancel();
        auto const id = gi::findInstrument(config.defaultInstrument) != nullptr
                          ? std::string_view{config.defaultInstrument}
                          : gi::instrumentAt(0).id;
        diagKey("key: open " + std::string{id});
        openWith(id);
        return;
    }

    if (config.enableCycleKey && key == config.cycleKey) {
        event.cancel();
        auto const instruments = gi::allInstruments();
        gCycleIndex            = (gCycleIndex + 1) % static_cast<int>(instruments.size());
        diagKey("key: cycle to " + std::string{instruments[static_cast<size_t>(gCycleIndex)].id});
        openWith(instruments[static_cast<size_t>(gCycleIndex)].id);
    }
}

[[maybe_unused]] OpenInstrumentHandler gOpenInstrumentHandler;

} // namespace

bool installClientSide() {
    auto& bus = ll::event::EventBus::getInstance();

    gListeners.emplace_back(bus.emplaceListener<ll::event::client::ClientJoinLevelEvent>(onClientJoinLevel));
    gListeners.emplace_back(bus.emplaceListener<ll::event::input::KeyInputEvent>(onKey));

    gCycleIndex = gi::instrumentIndex(modConfig().defaultInstrument);
    if (gCycleIndex < 0) {
        gCycleIndex = 0;
    }

    return instrumentScreen().install();
}

void uninstallClientSide() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto& listener : gListeners) {
        bus.removeListener(listener);
    }
    gListeners.clear();
    instrumentScreen().uninstall();
}

} // namespace gi::client

#endif // LL_PLAT_C

