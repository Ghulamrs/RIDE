#pragma once

// **The product's name, once.** Every name the editor shows or leaves on a
// disk is made from these: the window's title and About, the per-user state
// directory, the temporary files a build writes, the marker a debugger echoes.
// The programs' own file names come from the build - PRODUCT in the Makefiles,
// RideProduct in product.props, Product in the .iss - and are spelled the same.
// A rename is these lines and those three, and a search of the prose.

namespace editor {
namespace product {

// As people read it: "RIDE 4.0", the Start menu, a message box's title.
constexpr const char* kName = "RIDE";
// As a file name wants it: ride-run.s, ride-parts, .ride.
constexpr const char* kLower = "ride";
// The per-user state - the recent projects and files, the frame, Debug or
// Release. Not configuration: that is settings.json beside the programs, and a
// project's own is its .pro. Under the home directory.
constexpr const char* kStateDirectory = ".ride";
constexpr const char* kStateFile = "state.json";

}
}
