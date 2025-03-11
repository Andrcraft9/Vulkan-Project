#pragma once

#include <filesystem>
#include <fstream>
#include <vector>

namespace graphics {

namespace fs = std::filesystem;

/// Helper function to load the binary data from the files.
///
/// @param path  Path to file.
///
/// @return Loaded binary data.
std::vector<char> ReadFile(fs::path path);

} // namespace graphics
