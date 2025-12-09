#include <cstdint>
#include <string>

#include <stb/stb_image.h>

namespace render {

class ImageData final {
public:
  ImageData();
  ImageData(std::string path);
  ImageData(const unsigned char *data, std::size_t size);

  ImageData(ImageData &&other);

  ~ImageData();

  int Width() const { return width_; }

  int Height() const { return height_; }

  int Components() const { return components_; }

  stbi_uc *Data() const { return data_; }

  ImageData(const ImageData &) = delete;
  ImageData &operator=(const ImageData &) = delete;
  ImageData &operator=(ImageData &&) = delete;

private:
  int width_{};
  int height_{};
  int components_{};
  stbi_uc *data_{};
};

} // namespace render
