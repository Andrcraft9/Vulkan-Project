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

// TODO: Abstract descriptor set and uniform buffers somehow...?
class VertexShaderImpl : public VertexShader {
public:
  struct Vertex final {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
  };

  struct Options final {
    const std::string vertexShaderPath{};
    VkCommandPool commandPool{};
    std::vector<Vertex> vertices{};
    std::vector<std::uint16_t> indices{};
  };

  VertexShaderImpl(render::Context &context, const Options &options) {
    LOG(INFO) << "VS: Loading a shader:" << options.vertexShaderPath;
    const auto vertexShaderCode = ReadFile(options.vertexShaderPath);
    shaderModule_ = context.CreateShaderModule(vertexShaderCode);

    LOG(INFO) << "VS: Input binding/attribute descriptions...";
    vertexInputBinding_ = GetBindingDescription();
    vertexInputAttributes_ = GetAttributeDescriptions();

    LOG(INFO) << "VS: Creating a vertex buffer...";
    render::VertexBufferOptions vertexBufferOptions{};
    vertexBufferOptions.commandPool = options.commandPool;
    vertexBufferOptions.bufferSize =
        sizeof(options.vertices[0]) * options.vertices.size();
    vertexBufferOptions.bufferData = options.vertices.data();
    vertexBuffer_ = context.CreateVertexBuffer(vertexBufferOptions);

    LOG(INFO) << "VS: Creating a index buffer...";
    render::IndexBufferOptions indexBufferOptions{};
    indexBufferOptions.commandPool = options.commandPool;
    indexBufferOptions.bufferSize =
        sizeof(options.indices[0]) * options.indices.size();
    indexBufferOptions.bufferData = options.indices.data();
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

  static VkVertexInputBindingDescription GetBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
  }

  static std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    VkVertexInputAttributeDescription vertexInputAttribute0{};
    vertexInputAttribute0.binding = 0;
    vertexInputAttribute0.location = 0;
    vertexInputAttribute0.format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttribute0.offset = offsetof(Vertex, pos);
    attributeDescriptions.push_back(vertexInputAttribute0);

    VkVertexInputAttributeDescription vertexInputAttribute1{};
    vertexInputAttribute1.binding = 0;
    vertexInputAttribute1.location = 1;
    vertexInputAttribute1.format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttribute1.offset = offsetof(Vertex, color);
    attributeDescriptions.push_back(vertexInputAttribute1);

    VkVertexInputAttributeDescription vertexInputAttribute2{};
    vertexInputAttribute2.binding = 0;
    vertexInputAttribute2.location = 2;
    vertexInputAttribute2.format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttribute2.offset = offsetof(Vertex, texCoord);
    attributeDescriptions.push_back(vertexInputAttribute2);

    return attributeDescriptions;
  }
};

} // namespace graphics