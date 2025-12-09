#include "graphics/utils.hpp"

namespace graphics {

std::vector<char> ReadFile(const fs::path path) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f.is_open()) {
    throw std::runtime_error("failed to open file!");
  }
  const auto sz = fs::file_size(path);
  std::vector<char> result(sz);
  f.read(result.data(), sz);
  return result;
}

} // namespace graphics
