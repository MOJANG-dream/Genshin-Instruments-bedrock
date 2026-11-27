#include "mod/GenshinInstrumentsMod.h"

#include "instrument/Instruments.h"
#include "mod/Config.h"
#include "network/ServerSide.h"

#include "ll/api/Config.h"
#include "ll/api/io/Logger.h"
#include "ll/api/mod/RegisterHelper.h"

#ifdef LL_PLAT_C
#include "client/ClientSide.h"
#include "pack/ResourcePacks.h"
#endif

#include <algorithm>
#include <string>

namespace gi {

namespace {
Config gConfig;
} // namespace

Config& modConfig() { return gConfig; }

GenshinInstrumentsMod& GenshinInstrumentsMod::getInstance() {
    static GenshinInstrumentsMod instance;
    return instance;
}

GenshinInstrumentsMod::GenshinInstrumentsMod() : mSelf(*ll::mod::NativeMod::current()) {}

bool GenshinInstrumentsMod::load() {
    auto& logger = getSelf().getLogger();
    auto  path   = getSelf().getConfigDir() / "config.json";

    bool const loaded = ll::config::loadConfig(gConfig, path);
    if (!loaded) {
        ll::config::saveConfig(gConfig, path);
        logger.info("created default config at {}", path.string());
    }

    if (gi::findInstrument(gConfig.defaultInstrument) == nullptr) {
        gConfig.defaultInstrument = std::string{gi::instrumentAt(0).id};
        ll::config::saveConfig(gConfig, path);
        logger.warn("unknown defaultInstrument in config, reset to {}", gConfig.defaultInstrument);
    }

    gConfig.transposeOctaves = std::clamp(gConfig.transposeOctaves, -2, 2);

    logger.info(
        "Genshin Instruments loaded: {} instruments, open key 0x{:X}, cycle key 0x{:X}",
        gi::allInstruments().size(),
        gConfig.openKey,
        gConfig.cycleKey
    );
    return true;
}

bool GenshinInstrumentsMod::enable() {
    auto& logger = getSelf().getLogger();

    if (!network::installServerSide()) {
        logger.error("failed to install the server side");
        return false;
    }

#ifdef LL_PLAT_C
    if (!pack::install()) {
        logger.warn("resource packs could not be registered; install data/packs manually");
    }
    if (!client::installClientSide()) {
        logger.error("failed to install the client side");
        return false;
    }
#endif

    logger.info("Genshin Instruments enabled");
    return true;
}

bool GenshinInstrumentsMod::disable() {
#ifdef LL_PLAT_C
    client::uninstallClientSide();
    pack::uninstall();
#endif
    network::uninstallServerSide();
    return true;
}

} // namespace gi

LL_REGISTER_MOD(gi::GenshinInstrumentsMod, gi::GenshinInstrumentsMod::getInstance());
