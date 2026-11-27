// Client-only instrument screen.
//
// Borrows a vanilla CommonDialogInfoScreen for focus and mouse capture (no form
// id, no server round trip), never lets that dialog paint itself, and draws the
// 3x7 note grid with 2D primitives from BeforeUIRenderEvent. The scene recipe is
// copied from the reference mod's SettingsScreen.cpp (proven on this version of
// LeviLamina); the world keeps rendering behind the panel.
#ifdef LL_PLAT_C

#include "client/InstrumentScreen.h"

#include "client/NotePlayer.h"
#include "instrument/Instruments.h"
#include "mod/Config.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/thread/ClientThreadExecutor.h"

#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/IMinecraftGame.h"
#include "mc/client/gui/CaretMeasureData.h"
#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/FontRepository.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/TextAlignment.h"
#include "mc/client/gui/TextMeasureData.h"
#include "mc/client/gui/screens/AbstractScene.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/input/MouseAction.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/minecraft_renderer/objects/FrameRenderObject.h"

#include <bitset>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace gi::client {
namespace {

using namespace ll::event;

// --- Palette and geometry (GUI units) ---------------------------------------

constexpr mce::Color kWhite{1.f, 1.f, 1.f, 1.f};

struct Rgb {
    float r, g, b;
};
constexpr Rgb kPanel{0.10f, 0.11f, 0.13f}, kFrame{0.36f, 0.37f, 0.40f};
constexpr Rgb kText{0.93f, 0.94f, 0.95f}, kDim{0.60f, 0.62f, 0.65f};
constexpr Rgb kAccent{0.42f, 0.76f, 0.30f}, kCellIdle{0.16f, 0.17f, 0.19f};
constexpr Rgb kCellHover{0.25f, 0.27f, 0.31f}, kCellActive{0.30f, 0.56f, 0.23f};

mce::Color color(Rgb c, float a = 1.f) { return {c.r, c.g, c.b, a}; }

constexpr float kCellW = 34.f, kCellH = 26.f, kGap = 4.f;
constexpr float kStrideX = kCellW + kGap, kStrideY = kCellH + kGap;
constexpr float kGridW = static_cast<float>(gi::kCols) * kCellW + static_cast<float>(gi::kCols - 1) * kGap;
constexpr float kGridH = static_cast<float>(gi::kRows) * kCellH + static_cast<float>(gi::kRows - 1) * kGap;
constexpr float kPad = 16.f, kHeaderH = 48.f, kFooterH = 20.f;
constexpr float kPanelW = kGridW + kPad * 2.f;
constexpr float kPanelH = kHeaderH + kGridH + kFooterH;

struct Geometry {
    float left, top;
    float gridX() const { return left + kPad; }
    float gridY() const { return top + kHeaderH; }
    float cellX(int col) const { return gridX() + static_cast<float>(col) * kStrideX; }
    float cellY(int row) const { return gridY() + static_cast<float>(row) * kStrideY; }
};

Geometry computeGeometry(ScreenView& view) {
    auto& size = view.mSize;
    return {(size->x - kPanelW) / 2.f, (size->y - kPanelH) / 2.f};
}

// --- Shared state ----------------------------------------------------------
// Everything below is guarded by gMutex, like the reference screen: the input
// thread only records, the render thread acts.

std::mutex gMutex;

IClientInstance*               gClient{nullptr};
std::shared_ptr<AbstractScene> gScene;   // borrowed dialog
std::shared_ptr<AbstractScene> gRetired; // stays cancelled while it leaves
thread_local ScreenView*       tRenderView{nullptr};

bool                                  gSeen{false};
bool                                  gClosing{false};
bool                                  gRetiredRendered{false};
std::chrono::steady_clock::time_point gOpenedAt;
std::chrono::steady_clock::time_point gRetiredHardDeadline;

std::string             gInstrumentId{};
int                     gOctave{0}; // session transpose given to open()
std::bitset<gi::kCells> gPressed{}; // notes held down
int                     gHover{-1}; // cell under the cursor, -1 outside

// Every press swallowed while open is remembered so its release is swallowed
// too; a key already held when the screen opened still gets its release through
// and cannot stay stuck after the screen closes.
std::bitset<256> gSwallowedDown{};

bool  gLeftDown{false};
bool  gCursorValid{false};
float gCursorX{0.f}, gCursorY{0.f};
int   gLastMouseCell{-1}; // suppresses repeats while dragging
float gInvScale{1.f};     // window pixels -> GUI units

bool gShiftHeld{false}, gAltHeld{false};

// Notes queued by the input thread; the sound engine runs on the render thread.
struct NoteEvent {
    int  cell;
    int  transpose;
    bool down;
};
std::vector<NoteEvent> gNotes;
constexpr size_t       kMaxQueuedNotes = 64;

std::optional<std::pair<float, float>> gPendingClick;

std::thread::id          gClientThreadId{};
std::vector<ListenerPtr> gListeners;

// --- Drawing primitives -----------------------------------------------------

Font& defaultFont(MinecraftUIRenderContext& ctx) {
    return ctx.mClient.getMinecraftGame_DEPRECATED()
        .getFontRepository()
        ->getFontFromFontType("default")
        .getFont();
}

void fillRect(MinecraftUIRenderContext& ctx, float x, float y, float w, float h, Rgb rgb, float alpha = 1.f) {
    if (w <= 0 || h <= 0) {
        return;
    }
    ctx.fillRectangle(RectangleArea{x, x + w, y, y + h}, color(rgb, 1.f), alpha);
    ctx.flushImages(kWhite, alpha, HashedString{"ui_fillColor"});
}

void frameRect(MinecraftUIRenderContext& ctx, float x, float y, float w, float h, Rgb rgb, float alpha = 1.f) {
    fillRect(ctx, x, y, w, 1, rgb, alpha);
    fillRect(ctx, x, y + h - 1, w, 1, rgb, alpha);
    fillRect(ctx, x, y + 1, 1, h - 2, rgb, alpha);
    fillRect(ctx, x + w - 1, y + 1, 1, h - 2, rgb, alpha);
}

void drawLabel(
    MinecraftUIRenderContext& ctx,
    float                     x,
    float                     y,
    float                     w,
    std::string               text,
    Rgb                       rgb,
    ::ui::TextAlignment       align = ::ui::TextAlignment::Left,
    float                     size  = 1.f
) {
    auto&                   font = defaultFont(ctx);
    TextMeasureData const   measure{size, 0.f, false, false, false, align};
    CaretMeasureData const  caret{-1, false};
    ctx.drawText(font, RectangleArea{x, x + w, y, y + 14.f * size}, std::move(text), color(rgb), 1.f, align, measure, caret);
    ctx.flushText(0.f, std::nullopt);
}

// --- State helpers (the *Locked ones require gMutex) ------------------------

bool ownsTopLocked() {
    return gClient && gScene && gClient->getSceneFactory().getCurrentSceneStack()->getTopScene() == gScene.get();
}

// Drops everything the input path owns.
void resetInputLocked() {
    gPressed.reset();
    gHover         = -1;
    gLastMouseCell = -1;
    gLeftDown      = false;
    gCursorValid   = false;
    gShiftHeld     = false;
    gAltHeld       = false;
    gSwallowedDown.reset();
    gNotes.clear();
    gPendingClick.reset();
}

void clearLocked() {
    gRetired.reset();
    gScene.reset();
    gClient  = nullptr;
    gSeen    = false;
    gClosing = false;
    resetInputLocked();
}

InstrumentDef const& currentDefLocked() {
    if (auto* def = gi::findInstrument(gInstrumentId)) {
        return *def;
    }
    return gi::instrumentAt(0); // unknown id: first instrument is the fallback
}

// Session transpose plus the held modifier (+1 Shift, -1 Alt, both cancels out).
int liveTransposeLocked() {
    int delta = 0;
    if (gShiftHeld != gAltHeld) {
        delta = gShiftHeld ? 1 : -1;
    }
    return gOctave + delta;
}

std::string transposeTextLocked() {
    int const octaves = liveTransposeLocked();
    if (octaves == 0) {
        return "0";
    }
    return (octaves > 0 ? "+" : "") + std::to_string(octaves) + " oct";
}

void playCellLocked(int cell, int transpose) {
    if (auto* def = gi::findInstrument(gInstrumentId)) {
        playNote(*def, cell, transpose, 1.0f);
    } else {
        playNote(gi::instrumentAt(0), cell, transpose, 1.0f);
    }
}

void cycleInstrumentLocked() {
    auto const instruments = gi::allInstruments();
    int const  count       = static_cast<int>(instruments.size());
    if (count <= 0) {
        return;
    }
    int const current = gi::instrumentIndex(gInstrumentId);
    int const next    = current < 0 ? 0 : (current + 1) % count;
    gInstrumentId     = std::string{instruments[static_cast<size_t>(next)].id};
    gPressed.reset(); // the held notes belonged to the previous instrument
    gNotes.clear();
}

void closeLocked() {
    if (!gScene || gClosing) {
        return; // idempotent, or a pop is already in flight
    }
    bool const onTop = ownsTopLocked();
    if (onTop) {
        gClient->getSceneFactory().getCurrentSceneStack()->schedulePopScreen(1);
        gClosing = true;
    }
    notifyScreenClosed();
    resetInputLocked();
    if (!onTop) {
        clearLocked(); // nobody to pop: the screen was already covered
    }
}

// cellForPoint divides by the sizes it is given, so the gap belongs to the cell
// before it, matching the rects that are drawn.
int cellAtPoint(ScreenView& view, float x, float y) {
    auto const geo = computeGeometry(view);
    return gi::cellForPoint(x, y, geo.gridX(), geo.gridY(), kStrideX, kStrideY);
}

std::pair<float, float> toGuiLocked(float x, float y) {
    float const scale = (gInvScale > 0.f && std::isfinite(gInvScale)) ? gInvScale : 1.f;
    return {x * scale, y * scale};
}

// Applies what the input thread queued: notes play, clicks play, hover follows.
void consumeInputLocked(ScreenView& view) {
    for (auto const& note : gNotes) {
        if (note.down) {
            playCellLocked(note.cell, note.transpose);
            gPressed.set(static_cast<size_t>(note.cell));
        } else {
            gPressed.reset(static_cast<size_t>(note.cell));
        }
    }
    gNotes.clear();

    if (auto click = std::exchange(gPendingClick, std::nullopt)) {
        int const cell = cellAtPoint(view, click->first, click->second);
        if (cell >= 0 && cell != gLastMouseCell) {
            playCellLocked(cell, liveTransposeLocked());
        }
        gLastMouseCell = cell; // leaving the grid (-1) re-arms the same cell
    }

    gHover = gCursorValid ? cellAtPoint(view, gCursorX, gCursorY) : -1;
}

// --- Panel ------------------------------------------------------------------

void renderPanel(MinecraftUIRenderContext& ctx, ScreenView& view) {
    auto const  geo  = computeGeometry(view);
    auto const& cfg  = modConfig();
    auto const& def  = currentDefLocked();

    if (cfg.showBackground) {
        fillRect(ctx, geo.left, geo.top, kPanelW, kPanelH, kPanel, 0.97f);
    }
    frameRect(ctx, geo.left, geo.top, kPanelW, kPanelH, kFrame);

    // Header: instrument name, cycle hint, live transpose on the right.
    float const nameSize = def.displayName.size() > 18 ? 0.95f : 1.3f;
    drawLabel(ctx, geo.left + kPad, geo.top + 8.f, kPanelW - kPad * 2.f, std::string{def.displayName}, kText, ::ui::TextAlignment::Left, nameSize);
    drawLabel(ctx, geo.left + kPad, geo.top + 29.f, kPanelW * 0.55f, "Tab: next instrument", kDim, ::ui::TextAlignment::Left, 0.7f);
    drawLabel(ctx, geo.left + kPanelW - kPad - 54.f, geo.top + 27.f, 54.f, transposeTextLocked(), kAccent, ::ui::TextAlignment::Right, 0.9f);

    // Grid: col 0 is the left (lowest) octave; row 0 is the top row, so pitch
    // ascends upward inside a column and the bottom row is C.
    for (int col = 0; col < gi::kCols; ++col) {
        for (int row = 0; row < gi::kRows; ++row) {
            int const  cell    = gi::cellAt(col, row);
            bool const active  = gPressed.test(static_cast<size_t>(cell));
            bool const hovered = (cell == gHover);

            float const x = geo.cellX(col);
            float const y = geo.cellY(row);
            fillRect(ctx, x, y, kCellW, kCellH, active ? kCellActive : (hovered ? kCellHover : kCellIdle), 0.92f);
            frameRect(ctx, x, y, kCellW, kCellH, active ? kAccent : kFrame, active ? 1.f : 0.65f);
            if (cfg.showKeyLabels) {
                drawLabel(ctx, x + 4.f, y + 2.f, kCellW - 8.f, std::string{gi::keyLabelFor(cell)}, active ? kText : kDim, ::ui::TextAlignment::Left, 0.65f);
            }
        }
    }

    drawLabel(
        ctx,
        geo.left + kPad,
        geo.top + kHeaderH + kGridH + 5.f,
        kPanelW - kPad * 2.f,
        "Esc close   Shift/Alt transpose   Tab next",
        kDim,
        ::ui::TextAlignment::Center,
        0.8f
    );
}

// --- Thread dispatch --------------------------------------------------------
// Rendering runs on the client thread, so the render hook records its id; calls
// from anywhere else go through the executor.

void runOnClientThread(std::function<void()> task) {
    std::thread::id clientThread;
    {
        std::lock_guard lock(gMutex);
        clientThread = gClientThreadId;
    }
    if (clientThread != std::thread::id{} && clientThread == std::this_thread::get_id()) {
        task();
        return;
    }
    ll::thread::ClientThreadExecutor::getDefault().execute(std::move(task));
}

// --- Hooks: mark the borrowed scene, keep the world behind it ---------------

LL_TYPE_INSTANCE_HOOK(
    SceneRenderHook,
    HookPriority::Normal,
    UIScene,
    &UIScene::$render,
    void,
    ::ScreenContext& screenContext,
    ::FrameRenderObject const& renderObj
) {
    struct Restore {
        ScreenView* previous;
        ~Restore() { tRenderView = previous; }
    } restore{tRenderView};
    {
        std::lock_guard lock(gMutex);
        gClientThreadId = std::this_thread::get_id(); // this is the client thread
        if (gRetired.get() == this) {
            gRetiredRendered = true;
        }
        // A scene on its way out must stay cancelled as well, otherwise the
        // borrowed vanilla dialog shows up for a frame while it leaves.
        tRenderView = (gScene.get() == this || gRetired.get() == this) ? mScreenView.get() : nullptr;
    }
    origin(screenContext, renderObj);
}

LL_TYPE_INSTANCE_HOOK(
    SceneExitHook,
    HookPriority::Normal,
    UIScene,
    &UIScene::$onScreenExit,
    void,
    bool                             isPopping,
    bool                             doTransitions,
    std::shared_ptr<::AbstractScene> pushedScene
) {
    bool owned;
    {
        std::lock_guard lock(gMutex);
        owned = gScene.get() == this;
    }
    origin(isPopping, owned ? false : doTransitions, std::move(pushedScene));
    if (owned && isPopping) {
        std::lock_guard lock(gMutex);
        // Ownership is given up at once (input and panel stop), but the scene is
        // kept a little longer so its exit still stays unrendered.
        gRetired = std::move(gScene);
        gClient  = nullptr;
        gSeen    = false;
        gClosing = false;
        resetInputLocked();
        gRetiredRendered     = false;
        gRetiredHardDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    }
}

LL_TYPE_INSTANCE_HOOK(SceneBackgroundHook, HookPriority::Normal, UIScene, &UIScene::$renderGameBehind, bool) {
    std::lock_guard lock(gMutex);
    if (gScene.get() == this) {
        return true; // keep drawing the world behind the panel
    }
    return origin();
}

// --- Opening the borrowed scene (client thread) -----------------------------

void openSceneOnClientThread(std::string instrumentId, int transposeOctaves) {
    auto clientInstance = ll::service::bedrock::getClientInstance();
    if (!clientInstance) {
        return;
    }

    // Pushing the scene can run the previous screen's exit hook synchronously;
    // that hook takes gMutex, so the push itself has to happen unlocked.
    std::shared_ptr<::AbstractScene> toPush;

    {
        std::lock_guard lock(gMutex);

        if (!gi::findInstrument(instrumentId)) {
            instrumentId = std::string{gi::instrumentAt(0).id};
        }
        if (gScene) {
            if (gClosing) {
                return; // a pop is already in flight, do not revive that scene
            }
            // Already open: behave as an instrument switch and keep the scene.
            gInstrumentId = std::move(instrumentId);
            gOctave       = transposeOctaves;
            gPressed.reset();
            gNotes.clear();
            return;
        }

        // Only stack on the HUD or the chat screen, never on another screen.
        std::string screenName = clientInstance->getScreenName();
        if (screenName.rfind("hud_screen", 0) != 0 && screenName != "chat_screen") {
            return;
        }

        auto scene = clientInstance->getSceneFactory().createCommonDialogInfoScreen("Instrument", "");
        if (!scene) {
            return;
        }

        gClient       = &*clientInstance;
        gScene        = std::move(scene);
        gInstrumentId = std::move(instrumentId);
        gOctave       = transposeOctaves;
        gSeen         = false;
        gClosing      = false;
        gOpenedAt     = std::chrono::steady_clock::now();
        resetInputLocked();
        toPush = gScene;
    }

    clientInstance->getSceneFactory().getCurrentSceneStack()->pushScreen(std::move(toPush), false);
}

} // namespace

// --- Public interface -------------------------------------------------------

InstrumentScreen& InstrumentScreen::getInstance() {
    static InstrumentScreen instance;
    return instance;
}

bool InstrumentScreen::install() {
    SceneRenderHook::hook();
    SceneExitHook::hook();
    SceneBackgroundHook::hook();

    auto& bus = EventBus::getInstance();
    gListeners.clear();

    // Draw the grid instead of the borrowed dialog.
    gListeners.emplace_back(bus.emplaceListener<BeforeUIRenderEvent>([](BeforeUIRenderEvent& event) {
        std::lock_guard lock(gMutex);
        if (!gScene || tRenderView != &event.screenView()) {
            return;
        }
        event.cancel();
        gSeen = true;
        if (gClient) {
            gInvScale = gClient->getGuiData()->mInvGuiScale;
        }
        consumeInputLocked(event.screenView());
        if (gScene) {
            renderPanel(event.uiRenderContext(), event.screenView());
        }
    }));

    gListeners.emplace_back(bus.emplaceListener<AfterUIRenderEvent>([](AfterUIRenderEvent& event) {
        std::lock_guard lock(gMutex);
        // Drop the retired scene once it stopped rendering, with a hard cap in
        // case it never renders again.
        if (gRetired) {
            if (!gRetiredRendered || std::chrono::steady_clock::now() > gRetiredHardDeadline) {
                gRetired.reset();
            } else {
                gRetiredRendered = false;
            }
        }
        // Alive but no longer on top: it can never become usable again.
        if (gScene && &event.uiRenderContext().mClient == gClient && !ownsTopLocked()) {
            if (gSeen || std::chrono::steady_clock::now() - gOpenedAt > std::chrono::seconds(3)) {
                clearLocked();
            }
        }
    }));

    gListeners.emplace_back(bus.emplaceListener<input::MouseInputEvent>([](input::MouseInputEvent& event) {
        std::lock_guard lock(gMutex);
        if (!ownsTopLocked()) {
            return;
        }
        int const button   = event.actionButtonId();
        auto const [x, y] = toGuiLocked(static_cast<float>(event.x()), static_cast<float>(event.y()));

        if (button == MouseAction::ActionMove || button == MouseAction::ActionMoveRelative) {
            gCursorX     = x;
            gCursorY     = y;
            gCursorValid = true;
            if (gLeftDown) {
                gPendingClick = std::make_pair(x, y); // glissando
            }
            return; // never cancel: the game still needs the move to move the cursor
        }
        if (button == MouseAction::ActionWheel) {
            event.cancel(); // no hotbar scrolling through the panel
            return;
        }
        if (event.buttonData() == MouseAction::DataUp) {
            if (button == MouseAction::ActionLeft) {
                gLeftDown      = false;
                gLastMouseCell = -1; // pressing the same cell again must replay it
            }
            return; // let the release through so nothing sticks
        }
        event.cancel();
        if (button == MouseAction::ActionLeft) {
            gCursorX       = x;
            gCursorY       = y;
            gCursorValid   = true;
            gLeftDown      = true;
            gLastMouseCell = -1;
            gPendingClick  = std::make_pair(x, y);
        }
    }));

    gListeners.emplace_back(bus.emplaceListener<input::KeyInputEvent>([](input::KeyInputEvent& event) {
        std::lock_guard lock(gMutex);
        if (!ownsTopLocked()) {
            return;
        }
        int const  key     = event.keyCode();
        bool const down    = event.isDown();
        bool const tracked = key >= 0 && key < 256;
        auto const slot    = tracked ? static_cast<size_t>(key) : size_t{0};

        // Modifiers are only tracked, the transpose is applied when a note plays.
        switch (key) {
        case 0x10:
        case 0xA0:
        case 0xA1:
            gShiftHeld = down;
            break;
        case 0x12:
        case 0xA3:
        case 0xA4:
        case 0xA5:
            gAltHeld = down;
            break;
        default:
            break;
        }

        int const cell = gi::cellForKeyCode(key);
        if (cell >= 0 && gNotes.size() < kMaxQueuedNotes) {
            gNotes.push_back({cell, liveTransposeLocked(), down});
        }

        if (!down) {
            if (tracked && gSwallowedDown.test(slot)) {
                gSwallowedDown.reset(slot);
                event.cancel(); // release of a press this screen ate
            }
            return;
        }

        event.cancel(); // the screen owns every press while it is open
        if (tracked) {
            gSwallowedDown.set(slot);
        }
        if (key == 0x1b) {        // Esc
            closeLocked();
        } else if (key == 0x09) { // Tab: next instrument
            cycleInstrumentLocked();
        }
    }));

    gListeners.emplace_back(bus.emplaceListener<ClientExitLevelEvent>([](ClientExitLevelEvent&) {
        std::lock_guard lock(gMutex);
        clearLocked();
    }));

    return true;
}

void InstrumentScreen::uninstall() {
    {
        std::lock_guard lock(gMutex);
        // The borrowed dialog has to go before the scene hooks come off,
        // otherwise nothing cancels its render and it stays on screen.
        if (gScene && gClient && ownsTopLocked() && !gClosing) {
            gClient->getSceneFactory().getCurrentSceneStack()->schedulePopScreen(1);
        }
        clearLocked();
    }

    auto& bus = EventBus::getInstance();
    for (auto& listener : gListeners) {
        bus.removeListener(listener);
    }
    gListeners.clear();

    SceneBackgroundHook::unhook();
    SceneExitHook::unhook();
    SceneRenderHook::unhook();

    std::lock_guard lock(gMutex);
    gClientThreadId = std::thread::id{};
}

void InstrumentScreen::open(std::string_view instrumentId, int transposeOctaves) {
    // Copy the view here: the caller's buffer does not outlive this call.
    runOnClientThread([id = std::string{instrumentId}, transposeOctaves] {
        openSceneOnClientThread(std::move(id), transposeOctaves);
    });
}

void InstrumentScreen::close() {
    runOnClientThread([] {
        std::lock_guard lock(gMutex);
        closeLocked();
    });
}

bool InstrumentScreen::isOpen() const {
    std::lock_guard lock(gMutex);
    return static_cast<bool>(gScene);
}

std::string InstrumentScreen::instrumentId() const {
    std::lock_guard lock(gMutex);
    return gInstrumentId;
}

InstrumentScreen& instrumentScreen() { return InstrumentScreen::getInstance(); }

} // namespace gi::client

#endif // LL_PLAT_C
