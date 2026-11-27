#pragma once

namespace gi::pack {

// Registers <mod>/packs/{RP,BP} with the client resource pack repository and
// pushes both packs on top of every composed pack stack.
bool install();
void uninstall();

} // namespace gi::pack
