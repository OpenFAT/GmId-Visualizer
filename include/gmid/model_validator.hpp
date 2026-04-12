#pragma once

#include "types.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace gmid {

// ---------------------------------------------------------------------------
// Model-file validation
//
// Reads up to 2 MB of a SPICE model file and classifies it as MOSFET, BJT,
// or unknown based on keyword heuristics.  Returns std::unexpected on I/O
// failure.
// ---------------------------------------------------------------------------

std::expected<ModelKind, std::string>
validate_model_file(const std::filesystem::path& path);

// Scan the file for the first ".model <name>" statement and return the name.
std::expected<std::string, std::string>
extract_device_name(const std::filesystem::path& path);

} // namespace gmid
