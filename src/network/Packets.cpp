#include "network/Packets.h"

#include "instrument/Instruments.h"

#include "mc/deps/core/utility/BinaryStream.h"
#include "mc/deps/core/utility/ReadOnlyBinaryStream.h"

#include <algorithm>
#include <utility>

namespace gi::net {

namespace {

constexpr uint64 kMaxStringLength = 64;

} // namespace

void HelloPacket::write(::BinaryStream& stream) const {
    stream.writeVarInt(mProtocol, nullptr, nullptr);
}

::Bedrock::Result<void> HelloPacket::read(::ReadOnlyBinaryStream& stream) {
    auto protocol = stream.getVarInt();
    if (!protocol) {
        return {};
    }
    mProtocol = *protocol;
    return {};
}

void OpenInstrumentPacket::write(::BinaryStream& stream) const {
    stream.writeString(std::string_view{mInstrument}, nullptr, nullptr);
    stream.writeVarInt(mTransposeOctaves, nullptr, nullptr);
}

::Bedrock::Result<void> OpenInstrumentPacket::read(::ReadOnlyBinaryStream& stream) {
    auto instrument = stream.getString(kMaxStringLength);
    if (!instrument) {
        return {};
    }
    mInstrument = std::move(*instrument);

    auto transpose = stream.getVarInt();
    if (!transpose) {
        return {};
    }
    mTransposeOctaves = std::clamp(*transpose, -2, 2);
    return {};
}

void CloseInstrumentPacket::write(::BinaryStream&) const {}

::Bedrock::Result<void> CloseInstrumentPacket::read(::ReadOnlyBinaryStream&) { return {}; }

void PlayNotePacket::write(::BinaryStream& stream) const {
    stream.writeString(std::string_view{mInstrument}, nullptr, nullptr);
    stream.writeVarInt(mCell, nullptr, nullptr);
    stream.writeVarInt(mTransposeOctaves, nullptr, nullptr);
    stream.writeFloat(mVolume, nullptr, nullptr);
}

::Bedrock::Result<void> PlayNotePacket::read(::ReadOnlyBinaryStream& stream) {
    mValid = false;

    auto instrument = stream.getString(kMaxStringLength);
    if (!instrument) {
        return {};
    }
    auto cell = stream.getVarInt();
    if (!cell) {
        return {};
    }
    auto transpose = stream.getVarInt();
    if (!transpose) {
        return {};
    }
    auto volume = stream.getFloat();
    if (!volume) {
        return {};
    }

    mInstrument       = std::move(*instrument);
    mCell             = *cell;
    mTransposeOctaves = std::clamp(*transpose, -2, 2);
    mVolume           = std::clamp(*volume, 0.0f, 2.0f);
    auto const* def   = gi::findInstrument(mInstrument);
    mValid            = def != nullptr && mCell >= 0 && mCell < gi::cellsOf(*def);
    return {};
}

} // namespace gi::net
