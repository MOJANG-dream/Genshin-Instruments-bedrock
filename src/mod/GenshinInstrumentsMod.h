#pragma once

#include "ll/api/mod/NativeMod.h"

namespace gi {

class GenshinInstrumentsMod {
public:
    static GenshinInstrumentsMod& getInstance();

    GenshinInstrumentsMod();

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace gi
