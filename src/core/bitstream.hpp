#pragma once

// The bit stream has no implementation detail that the public header does
// not already expose, so this header only forwards to it.
//
// The public class definitions must not be repeated here: doing so would
// redefine the same classes in the same namespace and break the build.
#include "blockcodec/bitstream.hpp"
