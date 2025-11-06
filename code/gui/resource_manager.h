#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>
#include <cstdint>

// Forward declarations
namespace catacurses {
    class window;
}

namespace ImGui {
    struct ImDrawList;
    struct ImDrawCmd;
    struct ImFont;
    struct ImTextureID;
}

namespace sdl {
    class renderer;
    class texture;
    class font;
}

namespace gui {

// Resource management system for efficient GUI memory usage
namespace resource_manager {

// Resource types supported
enum class ResourceType : uint8_t {
    Texture,
    Font,
    Shader,
    VertexBuffer,
    IndexBuffer,
    UniformBuffer,
    FrameBuffer,
    Sampler,
    Material
};

// Memory usage statistics
struct MemoryStats {
    std::atomic<uint64_t> total_allocated{0};
    std::atomic<uint64_t> total_freed{0};
    std::atomic<uint32_t> active_textures{0};
    std::atomic<uint32_t> active_fonts{0};
    std::atomic<uint32_t> active_shaders{0};
    std::atomic<uint32_t> active_buffers{0};
    std::atomic<uint32_t> pool_hits{0};
    std::atomic<uint32_t> pool_misses{0};
    
    // Get current allocated size
    uint64_t current_usage() const { 
        return total_allocated.load() - total_freed.load();
    }
    
    // Get pool efficiency percentage
    double pool_efficiency() const {
        uint32_t total = pool_hits.load() + pool_misses.load();
        return total > 0 ? (pool_hits.load() * 100.0) / total : 0.0;
    }
};

// Resource identifier using RAII
class Resource {
public:
    Resource() = default;
    explicit Resource(const std::string& id, ResourceType type);
    virtual ~Resource() = default;
    
    // Move semantics
    Resource(Resource&& other) noexcept;
    Resource& operator=(Resource&& other) noexcept;
    
    // Non-copyable
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    
    const std::string& get_id() const { return id_; }
    ResourceType get_type() const { return type_; }
    uint64_t get_size() const { return size_; }
    std::chrono::steady_clock::time_point get_last_access() const { return last_access_; }
    
    void update_last_access() { last_access_ = std::chrono::steady_clock::now(); }
    void set_size(uint64_t size) { size_ = size; }
    
    // Check if resource is stale (unused for too long)
    bool is_stale(std::chrono::seconds timeout = std::chrono::seconds(300)) const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - last_access_) > timeout;
    }

private:
    std::string id_;
    ResourceType type_{ResourceType::Texture};
    uint64_t size_{0};
    std::chrono::steady_clock::time_point last_access_{std::chrono::steady_clock::now()};
};

// Texture resource with automatic cleanup
class TextureResource : public Resource {
public:
    TextureResource(const std::string& id, ImTextureID texture, uint32_t width, uint32_t height);
    ~TextureResource() override;
    
    ImTextureID get_texture() const { return texture_; }
    uint32_t get_width() const { return width_; }
    uint32_t get_height() const { return height_; }
    
    void set_texture(ImTextureID texture) { texture_ = texture; }
    void update_dimensions(uint32_t width, uint32_t height) {
        width_ = width; 
        height_ = height;
    }

private:
    ImTextureID texture_{nullptr};
    uint32_t width_{0};
    uint32_t height_{0};
};

// Font resource with caching
class FontResource : public Resource {
public:
    FontResource(const std::string& id, ImFont* font);
    ~FontResource() override = default;
    
    ImFont* get_font() const { return font_; }
    void set_font(ImFont* font) { font_ = font; }
    
    // Check if font supports specific characters
    bool supports_text(const std::string& text) const;

private:
    ImFont* font_{nullptr};
};

// Shader resource for GPU programs
class ShaderResource : public Resource {
public:
    ShaderResource(const std::string& id, const std::string& vertex_source, const std::string& fragment_source);
    ~ShaderResource() override;
    
    const std::string& get_vertex_source() const { return vertex_source_; }
    const std::string& get_fragment_source() const { return fragment_source_; }
    bool is_compiled() const { return compiled_; }
    void set_compiled(bool compiled) { compiled_ = compiled; }

private:
    std::string vertex_source_;
    std::string fragment_source_;
    bool compiled_{false};
};

// Generic buffer resource
class BufferResource : public Resource {
public:
    enum class BufferUsage : uint8_t {
        Static,
        Dynamic,
        Stream
    };
    
    BufferResource(const std::string& id, ResourceType type, void* data, size_t size, BufferUsage usage);
    ~BufferResource() override;
    
    void* get_data() const { return data_; }
    size_t get_buffer_size() const { return buffer_size_; }
    BufferUsage get_usage() const { return usage_; }
    
    void update_data(void* new_data, size_t new_size);
    
private:
    void* data_{nullptr};
    size_t buffer_size_{0};
    BufferUsage usage_{BufferUsage::Static};
};

// Resource pool for object reuse
template<typename T>
class ResourcePool {
public:
    ResourcePool(size_t max_size = 1000) : max_size_(max_size) {}
    
    std::shared_ptr<T> get(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = available_.find(id);
        if (it != available_.end() && !it->second->expired()) {
            pool_hits_++;
            return it->second.lock();
        }
        
        pool_misses_++;
        return nullptr;
    }
    
    void release(const std::string& id, std::shared_ptr<T> resource) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (available_.size() >= max_size_) {
            // Clean up oldest entries
            cleanup();
        }
        
        available_[id] = resource;
    }
    
    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto it = available_.begin(); it != available_.end();) {
            if (it->second.expired()) {
                it = available_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void set_max_size(size_t max_size) { 
        std::lock_guard<std::mutex> lock(mutex_);
        max_size_ = max_size; 
    }
    
    size_t get_available_count() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return available_.size(); 
    }
    
    uint32_t get_hits() const { return pool_hits_.load(); }
    uint32_t get_misses() const { return pool_misses_.load(); }

private:
    std::unordered_map<std::string, std::weak_ptr<T>> available_;
    mutable std::mutex mutex_;
    size_t max_size_;
    std::atomic<uint32_t> pool_hits_{0};
    std::atomic<uint32_t> pool_misses_{0};
};

// Resource creation callbacks
using TextureCreator = std::function<std::shared_ptr<TextureResource>(const std::string&)>;
using FontCreator = std::function<std::shared_ptr<FontResource>(const std::string&)>;
using ShaderCreator = std::function<std::shared_ptr<ShaderResource>(const std::string&)>;
using BufferCreator = std::function<std::shared_ptr<BufferResource>(const std::string&)>;

// Main resource manager class
class Manager {
public:
    static Manager& instance();
    
    // Resource lifecycle management
    std::shared_ptr<TextureResource> get_texture(const std::string& id, const TextureCreator& creator);
    std::shared_ptr<FontResource> get_font(const std::string& id, const FontCreator& creator);
    std::shared_ptr<ShaderResource> get_shader(const std::string& id, const ShaderCreator& creator);
    std::shared_ptr<BufferResource> get_buffer(const std::string& id, ResourceType type, const BufferCreator& creator);
    
    // Direct resource access
    std::shared_ptr<Resource> get_resource(const std::string& id);
    bool has_resource(const std::string& id) const;
    void release_resource(const std::string& id);
    
    // Resource validation and cleanup
    void validate_resources();
    void cleanup_stale_resources(std::chrono::seconds timeout = std::chrono::seconds(300));
    void clear_all_resources();
    
    // Memory management
    const MemoryStats& get_stats() const { return stats_; }
    uint64_t get_total_memory_usage() const { return stats_.current_usage(); }
    void set_memory_limit(uint64_t limit) { memory_limit_ = limit; }
    uint64_t get_memory_limit() const { return memory_limit_; }
    
    // Pool management
    void set_pool_size(size_t texture_pool, size_t font_pool, size_t shader_pool, size_t buffer_pool);
    void cleanup_pools();
    
    // Resource profiling and debugging
    void enable_profiling(bool enable) { profiling_enabled_ = enable; }
    bool is_profiling_enabled() const { return profiling_enabled_; }
    
    void log_resource_usage() const;
    void generate_memory_report(std::ostream& report) const;
    
    // Integration with existing Cataclysm systems
    void integrate_with_ui_manager();
    void integrate_with_sdl_renderer(void* renderer);
    void integrate_with_input_system();
    
    // Thread safety
    void set_thread_safe(bool safe) { thread_safe_ = safe; }
    bool is_thread_safe() const { return thread_safe_; }

private:
    Manager() = default;
    ~Manager() = default;
    
    // Non-copyable
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    
    // Internal resource storage
    std::unordered_map<std::string, std::shared_ptr<Resource>> resources_;
    mutable std::shared_mutex resources_mutex_;
    
    // Resource pools
    ResourcePool<TextureResource> texture_pool_{1000};
    ResourcePool<FontResource> font_pool_{100};
    ResourcePool<ShaderResource> shader_pool_{50};
    ResourcePool<BufferResource> buffer_pool_{2000};
    
    // Memory statistics and limits
    MemoryStats stats_;
    uint64_t memory_limit_{100 * 1024 * 1024}; // 100MB default
    
    // Configuration
    bool profiling_enabled_{false};
    bool thread_safe_{true};
    std::chrono::seconds cleanup_interval_{std::chrono::seconds(60)};
    
    // Cleanup timer
    std::chrono::steady_clock::time_point last_cleanup_{std::chrono::steady_clock::now()};
    
    // Internal helpers
    template<typename T>
    std::shared_ptr<T> get_pooled_resource(
        const std::string& id,
        ResourceType type,
        ResourcePool<T>& pool,
        const std::function<std::shared_ptr<T>(const std::string&)>& creator);
    
    void update_memory_stats(ResourceType type, uint64_t size, bool allocating);
    bool check_memory_limit(uint64_t additional_size) const;
    void enforce_memory_limit();
    
    // Integration hooks
    static void ui_manager_redraw_hook();
    static void sdl_renderer_cleanup_hook();
    void periodic_cleanup();
};

// RAII resource handles for automatic resource management
class ScopedTexture {
public:
    ScopedTexture(const std::string& id, Manager& manager = Manager::instance());
    ~ScopedTexture() { release(); }
    
    // Move semantics
    ScopedTexture(ScopedTexture&& other) noexcept : manager_(other.manager_), id_(other.id_), resource_(other.resource_) {
        other.id_.clear();
        other.resource_.reset();
    }
    ScopedTexture& operator=(ScopedTexture&& other) noexcept {
        if (this != &other) {
            release();
            manager_ = other.manager_;
            id_ = other.id_;
            resource_ = other.resource_;
            other.id_.clear();
            other.resource_.reset();
        }
        return *this;
    }
    
    // Non-copyable
    ScopedTexture(const ScopedTexture&) = delete;
    ScopedTexture& operator=(const ScopedTexture&) = delete;
    
    const std::string& get_id() const { return id_; }
    std::shared_ptr<TextureResource> get() const { return resource_; }
    explicit operator bool() const { return resource_ != nullptr; }
    
    void release() {
        if (!id_.empty()) {
            manager_.release_resource(id_);
            id_.clear();
            resource_.reset();
        }
    }

private:
    Manager& manager_;
    std::string id_;
    std::shared_ptr<TextureResource> resource_;
};

class ScopedFont {
public:
    ScopedFont(const std::string& id, Manager& manager = Manager::instance());
    ~ScopedFont() { release(); }
    
    // Move semantics
    ScopedFont(ScopedFont&& other) noexcept : manager_(other.manager_), id_(other.id_), resource_(other.resource_) {
        other.id_.clear();
        other.resource_.reset();
    }
    ScopedFont& operator=(ScopedFont&& other) noexcept {
        if (this != &other) {
            release();
            manager_ = other.manager_;
            id_ = other.id_;
            resource_ = other.resource_;
            other.id_.clear();
            other.resource_.reset();
        }
        return *this;
    }
    
    // Non-copyable
    ScopedFont(const ScopedFont&) = delete;
    ScopedFont& operator=(const ScopedFont&) = delete;
    
    const std::string& get_id() const { return id_; }
    std::shared_ptr<FontResource> get() const { return resource_; }
    explicit operator bool() const { return resource_ != nullptr; }
    
    void release() {
        if (!id_.empty()) {
            manager_.release_resource(id_);
            id_.clear();
            resource_.reset();
        }
    }

private:
    Manager& manager_;
    std::string id_;
    std::shared_ptr<FontResource> resource_;
};

class ScopedShader {
public:
    ScopedShader(const std::string& id, Manager& manager = Manager::instance());
    ~ScopedShader() { release(); }
    
    // Move semantics
    ScopedShader(ScopedShader&& other) noexcept : manager_(other.manager_), id_(other.id_), resource_(other.resource_) {
        other.id_.clear();
        other.resource_.reset();
    }
    ScopedShader& operator=(ScopedShader&& other) noexcept {
        if (this != &other) {
            release();
            manager_ = other.manager_;
            id_ = other.id_;
            resource_ = other.resource_;
            other.id_.clear();
            other.resource_.reset();
        }
        return *this;
    }
    
    // Non-copyable
    ScopedShader(const ScopedShader&) = delete;
    ScopedShader& operator=(const ScopedShader&) = delete;
    
    const std::string& get_id() const { return id_; }
    std::shared_ptr<ShaderResource> get() const { return resource_; }
    explicit operator bool() const { return resource_ != nullptr; }
    
    void release() {
        if (!id_.empty()) {
            manager_.release_resource(id_);
            id_.clear();
            resource_.reset();
        }
    }

private:
    Manager& manager_;
    std::string id_;
    std::shared_ptr<ShaderResource> resource_;
};

class ScopedBuffer {
public:
    ScopedBuffer(const std::string& id, ResourceType type, Manager& manager = Manager::instance());
    ~ScopedBuffer() { release(); }
    
    // Move semantics
    ScopedBuffer(ScopedBuffer&& other) noexcept : manager_(other.manager_), id_(other.id_), type_(other.type_), resource_(other.resource_) {
        other.id_.clear();
        other.type_ = ResourceType::Texture;
        other.resource_.reset();
    }
    ScopedBuffer& operator=(ScopedBuffer&& other) noexcept {
        if (this != &other) {
            release();
            manager_ = other.manager_;
            id_ = other.id_;
            type_ = other.type_;
            resource_ = other.resource_;
            other.id_.clear();
            other.type_ = ResourceType::Texture;
            other.resource_.reset();
        }
        return *this;
    }
    
    // Non-copyable
    ScopedBuffer(const ScopedBuffer&) = delete;
    ScopedBuffer& operator=(const ScopedBuffer&) = delete;
    
    const std::string& get_id() const { return id_; }
    ResourceType get_type() const { return type_; }
    std::shared_ptr<BufferResource> get() const { return resource_; }
    explicit operator bool() const { return resource_ != nullptr; }
    
    void release() {
        if (!id_.empty()) {
            manager_.release_resource(id_);
            id_.clear();
            resource_.reset();
        }
    }

private:
    Manager& manager_;
    std::string id_;
    ResourceType type_;
    std::shared_ptr<BufferResource> resource_;
};

// Convenience functions for common operations
namespace utilities {

// Get memory usage as human-readable string
std::string format_memory_size(uint64_t bytes);

// Get resource type as string
std::string resource_type_to_string(ResourceType type);

// Check if a resource ID follows naming conventions
bool is_valid_resource_id(const std::string& id);

// Generate unique resource ID
std::string generate_unique_id(const std::string& prefix = "res");

} // namespace utilities

} // namespace resource_manager

} // namespace gui
