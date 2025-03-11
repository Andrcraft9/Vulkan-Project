#pragma once

#include <graphics/utils.hpp>
#include <render/context.hpp>

namespace graphics {

class Vertex {};

class VertexShader {
public:
  virtual VkShaderModule ShaderModule() const = 0;
  virtual VkVertexInputBindingDescription VertexInputBinding() const = 0;
  virtual std::vector<VkVertexInputAttributeDescription>
  VertexInputAttributes() const = 0;
  virtual VkBuffer VertexBuffer() const = 0;
  virtual VkBuffer IndexBuffer() const = 0;
};

class VertexShaderImpl : public VertexShader {
public:
  struct Options final {
    const std::string vertexShaderPath{};
    VkCommandPool commandPool{};
    // TODO: Make implementation specific, don't use render::Vertex.
    std::vector<render::Vertex> vertices{};
    std::vector<std::uint16_t> indices{};
  };

  VertexShaderImpl(render::Context &context, const Options &options) {
    LOG(INFO) << "VS: Loading a shader:" << options.vertexShaderPath;
    const auto vertexShaderCode = ReadFile(options.vertexShaderPath);
    shaderModule_ = context.CreateShaderModule(vertexShaderCode);

    // TODO: Make implementation specific, don't use render::Get*().
    LOG(INFO) << "VS: Input binding/attribute descriptions...";
    vertexInputBinding_ = render::GetBindingDescription();
    vertexInputAttributes_ = render::GetAttributeDescriptions();

    LOG(INFO) << "VS: Creating a vertex buffer...";
    render::VertexBufferOptions vertexBufferOptions{};
    vertexBufferOptions.commandPool = options.commandPool;
    vertexBufferOptions.vertices = options.vertices;
    vertexBuffer_ = context.CreateVertexBuffer(vertexBufferOptions);

    LOG(INFO) << "VS: Creating a index buffer...";
    render::IndexBufferOptions indexBufferOptions{};
    indexBufferOptions.commandPool = options.commandPool;
    indexBufferOptions.indices = options.indices;
    indexBuffer_ = context.CreateIndexBuffer(indexBufferOptions);
  }

  VkShaderModule ShaderModule() const final { return shaderModule_; }

  VkVertexInputBindingDescription VertexInputBinding() const final {
    return vertexInputBinding_;
  }

  std::vector<VkVertexInputAttributeDescription>
  VertexInputAttributes() const final {
    return vertexInputAttributes_;
  }

  VkBuffer VertexBuffer() const final { return vertexBuffer_; }

  VkBuffer IndexBuffer() const final { return indexBuffer_; }

private:
  VkShaderModule shaderModule_;
  VkVertexInputBindingDescription vertexInputBinding_;
  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes_;
  VkBuffer vertexBuffer_;
  VkBuffer indexBuffer_;
};

} // namespace graphics