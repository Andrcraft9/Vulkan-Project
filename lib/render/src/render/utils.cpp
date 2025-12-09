#include "render/utils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace render {

ImageData::ImageData() = default;

ImageData::ImageData(const std::string path) {
  data_ =
      stbi_load(path.c_str(), &width_, &height_, &components_, STBI_rgb_alpha);
  components_ = STBI_rgb_alpha;
}

ImageData::ImageData(const unsigned char *data, const std::size_t size) {
  data_ = stbi_load_from_memory(data, size, &width_, &height_, &components_,
                                STBI_rgb_alpha);
  components_ = STBI_rgb_alpha;
}

ImageData::ImageData(ImageData &&other)
    : width_{other.width_}, height_{other.height_},
      components_{other.components_}, data_{other.data_} {
  other.data_ = nullptr;
}

ImageData::~ImageData() {
  if (data_ != nullptr) {
    stbi_image_free(data_);
  }
}

} // namespace render
