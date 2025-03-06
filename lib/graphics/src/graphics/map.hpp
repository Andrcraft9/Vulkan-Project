#pragma once

#include <render/context.hpp>

#include <cpr/cpr.h>

namespace graphics {

class TileParser {
  public:

    TileParser() = default;
    ~TileParser() = default;

    TileParser(const TileParser&) = delete;
    TileParser(TileParser&&) = delete;
    TileParser& operator=(const TileParser&) = delete;
    TileParser& operator=(TileParser&&) = delete;

    render::ImageData Parse(std::uint32_t zoom, std::uint32_t x, std::uint32_t y) {
      const std::string tile_server{"https://tile.openstreetmap.org"};

      std::string request{tile_server};
      request.append("/");
      request.append(std::to_string(zoom));
      request.append("/");
      request.append(std::to_string(x));
      request.append("/");
      request.append(std::to_string(y));
      request.append(".png");
      LOG(INFO) << "Requesting" << request.c_str();
      cpr::Response r = cpr::Get(cpr::Url{std::move(request)});

      render::ImageData tile{reinterpret_cast<unsigned char *>(r.text.data()), r.text.size()};
      LOG(INFO) << "Image" << tile.Width() << tile.Height() << tile.Components();
      return tile;
    }
  };

}  // namespace graphics
