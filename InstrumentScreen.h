#pragma once

#include <string>
#include <string_view>

namespace gi::client {

// Native 2D instrument screen. It borrows a vanilla dialog scene and draws the
// note grid itself; see SettingsScreen.cpp in the reference mod for the scene
// ownership recipe this implementation is based on.
class InstrumentScreen {
public:
    static InstrumentScreen& getInstance();

    bool install();
    void uninstall();

    // Opens the screen, or switches instrument when it is already open.
    // Safe to call from the client thread, from an input listener and from a
    // packet handler (the work is forwarded to the client thread).
    void open(std::string_view instrumentId, int transposeOctaves);
    void close();

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] std::string instrumentId() const;

private:
    InstrumentScreen() = default;
};

InstrumentScreen& instrumentScreen();

} // namespace gi::client
