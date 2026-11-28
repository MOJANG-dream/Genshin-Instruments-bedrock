#include "pack/ResourcePacks.h"

#ifdef LL_PLAT_C

#include "ll/api/io/Logger.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/resource/PackIdVersion.h"
#include "mc/deps/core/resource/PackType.h"
#include "mc/resources/PackInstance.h"
#include "mc/resources/ResourcePackManager.h"
#include "mc/resources/ResourcePackRepository.h"
#include "mc/resources/ResourcePackSelectionPolicy.h"
#include "mc/resources/ResourcePackStack.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace gi::pack {
namespace {

// header uuid + version of data/packs/*/manifest.json. Bump the version
// whenever the content of a pack changes: the game caches loaded packs by
// uuid+version and would otherwise keep serving stale sounds/textures.
constexpr char kResourcePackIdVersion[] = "7d3f1a2b-9c4e-4b7a-8f1d-2e6c5a9b0d41_0.1.0";
// Behaviour pack id; kept here so the two manifests and this file cannot drift
// apart. The pack itself is loaded per world (see docs/BUILD.md).
[[maybe_unused]] constexpr char kBehaviorPackIdVersion[] = "2b6c8e4f-1a3d-4f5b-9c7e-6d2a0b8f4c53_0.1.0";

void diag(std::string_view what) {
    if (auto mod = ll::mod::NativeMod::current()) {
        mod->getLogger().info("[gi] {}", what);
    }
}

// Development packs are scanned at start-up and are enabled for every world
// without touching the world settings, which is exactly what a mod-bundled pack
// needs: the composed stack alone leaves the icons and the sound events unknown
// to the client until a world is reloaded with the pack enabled by hand.
bool copyIntoDevelopmentFolder(std::filesystem::path const& from, std::string const& folder) {
    if (folder.empty() || !std::filesystem::is_directory(from)) {
        return false;
    }

    std::error_code ec;
    std::filesystem::path const target = std::filesystem::path{folder} / "genshin_instruments";
    std::filesystem::remove_all(target, ec);
    std::filesystem::create_directories(target.parent_path(), ec);
    std::filesystem::copy(from, target, std::filesystem::copy_options::recursive, ec);
    if (ec) {
        diag("packs: could not copy into " + folder + ": " + ec.message());
        return false;
    }
    diag("packs: copied " + from.string() + " -> " + target.string());
    return true;
}

LL_TYPE_INSTANCE_HOOK(
    ComposeFullStackHook,
    HookPriority::Normal,
    ResourcePackManager,
    &ResourcePackManager::_composeFullStack,
    void
) {
    origin();

    auto repo = ll::service::bedrock::getResourcePackRepository(true);
    if (!repo) {
        return;
    }

    ResourcePackStack* fullStack = mFullStack.get();
    if (!fullStack) {
        return;
    }

    // NonOwnerPointer<IResourcePackRepository const> cannot be built from the
    // derived type directly; cast to the base reference first.
    auto const& baseRepo = static_cast<::IResourcePackRepository const&>(*repo);

    // Only the resource pack goes onto the composed stack: that stack is the
    // client's resource stack, and behaviour packs are resolved per world.
    auto pack = repo->getResourcePackSatisfiesPackId(
        PackIdVersion::fromString(kResourcePackIdVersion),
        false,
        ::ResourcePackSelectionPolicy::PreferDevelopmentIfSameOrNewer
    );
    if (!pack) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            diag("packs: resource pack not found while composing the stack");
        }
        return;
    }
    fullStack->add(PackInstance{pack, 0, false, nullptr}, baseRepo, false);
}

} // namespace

bool install() {
    auto repo = ll::service::bedrock::getResourcePackRepository(true);
    auto mod  = ll::mod::NativeMod::current();
    if (!repo || !mod) {
        return false;
    }

    std::filesystem::path const packs = mod->getModDir() / "packs";

    // addCustomResourcePackPath only registers a directory to scan; the resource
    // pack itself is pushed onto the stack by the hook above. The behaviour pack
    // is registered so it can be picked up by a world, but it is not stacked.
    repo->addCustomResourcePackPath(packs / "RP", PackType::Resources);
    repo->addCustomResourcePackPath(packs / "BP", PackType::Behavior);

    // Belt and braces: a copy in the development folder is loaded for every world
    // by the game itself, so the textures and the sound events exist even when the
    // composed stack is built before this mod registered its packs.
    bool changed = false;
    // getDevelopment...Path() returns a const PathBuffer; PathBuffer::get() is not
    // const, and its explicit conversion operator is the const-friendly way in.
    auto const& devResources = repo->getDevelopmentResourcePacksPath();
    auto const& devBehavior  = repo->getDevelopmentBehaviorPacksPath();
    changed |= copyIntoDevelopmentFolder(
        packs / "RP" / "genshin_instruments",
        std::string{static_cast<std::string const&>(devResources)}
    );
    changed |= copyIntoDevelopmentFolder(
        packs / "BP" / "genshin_instruments",
        std::string{static_cast<std::string const&>(devBehavior)}
    );
    if (changed) {
        repo->refreshPacks();
        diag("packs: refreshed the pack list");
    }

    ComposeFullStackHook::hook();
    return true;
}

void uninstall() { ComposeFullStackHook::unhook(); }

} // namespace gi::pack

#else // LL_PLAT_C

namespace gi::pack {

bool install() { return true; }
void uninstall() {}

} // namespace gi::pack

#endif
