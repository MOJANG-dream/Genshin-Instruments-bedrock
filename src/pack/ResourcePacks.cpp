#include "pack/ResourcePacks.h"

#ifdef LL_PLAT_C

#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/resource/PackIdVersion.h"
#include "mc/deps/core/resource/PackType.h"
#include "mc/resources/PackInstance.h"
#include "mc/resources/ResourcePackManager.h"
#include "mc/resources/ResourcePackRepository.h"
#include "mc/resources/ResourcePackStack.h"

#include <optional>

namespace gi::pack {
namespace {

// header uuid + version of data/packs/*/manifest.json. Bump the version
// whenever the content of a pack changes: the game caches loaded packs by
// uuid+version and would otherwise keep serving stale sounds/textures.
constexpr char kResourcePackIdVersion[] = "7d3f1a2b-9c4e-4b7a-8f1d-2e6c5a9b0d41_0.1.0";
// Behaviour pack id; kept here so the two manifests and this file cannot drift
// apart. The pack itself is loaded per world (see docs/BUILD.md).
[[maybe_unused]] constexpr char kBehaviorPackIdVersion[] = "2b6c8e4f-1a3d-4f5b-9c7e-6d2a0b8f4c53_0.1.0";

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
    auto pack = repo->getResourcePackSatisfiesPackId(PackIdVersion::fromString(kResourcePackIdVersion), true);
    if (!pack) {
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

    // addCustomResourcePackPath only registers a directory to scan; the resource
    // pack itself is pushed onto the stack by the hook above. The behaviour pack
    // is registered so it can be picked up by a world, but it is not stacked.
    repo->addCustomResourcePackPath(mod->getModDir() / "packs" / "RP", PackType::Resources);
    repo->addCustomResourcePackPath(mod->getModDir() / "packs" / "BP", PackType::Behavior);

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
