#pragma once

#include "ll/api/network/packet/Packet.h"

#include <string>

// Custom packets of the mod. They ride LeviLamina's RuntimePacket, so any
// LeviLamina client/server understands them and vanilla peers silently drop
// them. The class names are hashed into the runtime id, therefore they must
// never be renamed without bumping the protocol version and they must not live
// in an anonymous namespace.
namespace gi::net {

using ll::network::PacketBase;

// c2s, sent once per join / world load so the server knows this client can be
// sent custom packets.
class HelloPacket final : public PacketBase<HelloPacket> {
public:
    using PacketBase::PacketBase;

    int mProtocol = 1;

    void write(::BinaryStream& stream) const override;
    [[nodiscard]] ::Bedrock::Result<void> read(::ReadOnlyBinaryStream& stream) override;
};

// s2c, asks the client to open the instrument screen.
class OpenInstrumentPacket final : public PacketBase<OpenInstrumentPacket> {
public:
    using PacketBase::PacketBase;

    std::string mInstrument{"windsong_lyre"};
    int         mTransposeOctaves = 0;

    void write(::BinaryStream& stream) const override;
    [[nodiscard]] ::Bedrock::Result<void> read(::ReadOnlyBinaryStream& stream) override;
};

// c2s, the screen was closed (by the player or because the item was dropped).
class CloseInstrumentPacket final : public PacketBase<CloseInstrumentPacket> {
public:
    using PacketBase::PacketBase;

    void write(::BinaryStream& stream) const override;
    [[nodiscard]] ::Bedrock::Result<void> read(::ReadOnlyBinaryStream& stream) override;
};

// c2s, one note was played.
class PlayNotePacket final : public PacketBase<PlayNotePacket> {
public:
    using PacketBase::PacketBase;

    std::string mInstrument{"windsong_lyre"};
    int         mCell             = 0;
    int         mTransposeOctaves = 0;
    float       mVolume           = 1.0f;
    // False when the stream was truncated; handlers ignore such packets.
    bool        mValid            = true;

    void write(::BinaryStream& stream) const override;
    [[nodiscard]] ::Bedrock::Result<void> read(::ReadOnlyBinaryStream& stream) override;
};

} // namespace gi::net
