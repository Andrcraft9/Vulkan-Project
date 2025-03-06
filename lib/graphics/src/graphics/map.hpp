#pragma once

#include <render/context.hpp>

#include <cpr/cpr.h>

class TileParser {
  public:
    struct Tile final {
      std::int32_t size_x{};
      std::int32_t size_y{};
      std::int32_t components{};
      unsigned char* image_data{};
    };

    TileParser() = default;

    ~TileParser() {
      for (auto& tile: tiles_) {
        stbi_image_free(tile.image_data);
      }
    }

    TileParser(const TileParser&) = delete;
    TileParser(TileParser&&) = delete;
    TileParser& operator=(const TileParser&) = delete;
    TileParser& operator=(TileParser&&) = delete;

    Tile Parse(std::uint32_t zoom, std::uint32_t x, std::uint32_t y) {
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

      Tile tile{};
      tile.image_data =
          stbi_load_from_memory(reinterpret_cast<unsigned char *>(r.text.data()),
                                r.text.size(), &tile.size_x, &tile.size_y, &tile.components, 0);
      LOG(INFO) << "Image" << tile.size_x << tile.size_y << tile.components
                << tile.image_data;
      tiles_.push_back(tile);
  
      return tile;
    }
  
  private:
    std::vector<Tile> tiles_{};
  };
