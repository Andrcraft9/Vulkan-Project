#pragma once

#include <graphics/utils.hpp>
#include <render/context.hpp>

namespace graphics {

struct VertexShaderCode final {
  const void *data{};
  VkDeviceSize size{};
};

struct VertexBufferData final {
  const void *data{};
  VkDeviceSize size{};
};

struct IndexBufferData final {
  const std::uint16_t *data{};
  VkDeviceSize size{};
};

struct VertexDescriptorSetDescription final {
  std::uint32_t binding{};
  VkDescriptorType type{};
};

class VertexShader {
public:
  virtual VertexShaderCode ShaderModule() const = 0;
  virtual VertexBufferData VertexBuffer() const = 0;
  virtual IndexBufferData IndexBuffer() const = 0;

  virtual VkVertexInputBindingDescription VertexInputBinding() const = 0;
  virtual std::vector<VkVertexInputAttributeDescription>
  VertexInputAttributes() const = 0;

  virtual VertexDescriptorSetDescription DescriptorSetLayout() const = 0;
};

class VertexShaderImpl : public VertexShader {
public:
  struct Vertex final {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
  };

  // TODO: Abstract descriptor set and uniform buffers somehow...?
  struct UniformBuffer final {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
  };

  struct Options final {
    const std::string vertexShaderPath{};
    std::vector<Vertex> vertices{};
    std::vector<std::uint16_t> indices{};
  };

  VertexShaderImpl(const Options &options) {
    LOG(INFO) << "VS: Loading a shader:" << options.vertexShaderPath;
    shaderCode_ = ReadFile(options.vertexShaderPath);

    LOG(INFO) << "VS: Copying vertices and indices...";
    vertices_ = options.vertices;
    indices_ = options.indices;

    LOG(INFO) << "VS: Caching input binding/attribute and descriptor set "
                 "descriptions...";
    vertexInputBinding_ = GetBindingDescription();
    vertexInputAttributes_ = GetAttributeDescriptions();
    vertexDescriptorSetDescription_ = GetVertexDescriptorSetDescription();
  }

  VertexShaderCode ShaderModule() const final {
    return VertexShaderCode{shaderCode_.data(), shaderCode_.size()};
  }

  VertexBufferData VertexBuffer() const final {
    return VertexBufferData{vertices_.data(),
                            sizeof(Vertex) * vertices_.size()};
  }

  IndexBufferData IndexBuffer() const final {
    return IndexBufferData{indices_.data(),
                           sizeof(std::uint16_t) * indices_.size()};
  }

  VkVertexInputBindingDescription VertexInputBinding() const final {
    return vertexInputBinding_;
  }

  std::vector<VkVertexInputAttributeDescription>
  VertexInputAttributes() const final {
    return vertexInputAttributes_;
  }

  VertexDescriptorSetDescription DescriptorSetLayout() const final {
    return vertexDescriptorSetDescription_;
  }

private:
  std::vector<char> shaderCode_;
  std::vector<Vertex> vertices_{};
  std::vector<std::uint16_t> indices_{};
  VkVertexInputBindingDescription vertexInputBinding_;
  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes_;
  VertexDescriptorSetDescription vertexDescriptorSetDescription_;

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

  static VertexDescriptorSetDescription GetVertexDescriptorSetDescription() {
    VertexDescriptorSetDescription descriptorSetDescription{};
    descriptorSetDescription.binding = 0;
    descriptorSetDescription.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    return descriptorSetDescription;
  }
};

} // namespace graphics