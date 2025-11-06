#include "resource_manager.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

namespace gui {

namespace resource_manager {

// Resource implementation
Resource::Resource(const std::string& id, ResourceType type) 
    : id_(id), type_(type) {
    last_access_ = std::chrono::steady_clock::now();
}

Resource::Resource(Resource&& other) noexcept
    : id_(std::move(other.id_))
    , type_(other.type_)
    , size_(other.size_)
    , last_access_(other.last_access_) {
    other.size_ = 0;
}

Resource& Resource::operator=(Resource&& other) noexcept {
    if (this != &other) {
        id_ = std::move(other.id_);
        type_ = other.type_;
        size_ = other.size_;
        last_access_ = other.last_access_;
        other.size_ = 0;
    }
    return *this;
}

// TextureResource implementation
TextureResource::TextureResource(const std::string& id, ImTextureID texture, uint32_t width, uint32_t height)
    : Resource(id, ResourceType::Texture)
    , texture_(texture)
    , width_(width)
    , height_(height) {
    
    set_size(static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 4); // Assume RGBA
    update_last_access();
}

TextureResource::~TextureResource() {
    // Texture cleanup is handled by the underlying graphics system
    // This destructor ensures proper RAII behavior
}

// FontResource implementation
FontResource::FontResource(const std::string& id, ImFont* font)
    : Resource(id, ResourceType::Font)
    , font_(font) {
    
    if (font_) {
        // Approximate font memory usage
        set_size(font_->MetricsTotalSize);
    }
    update_last_access();
}

bool FontResource::supports_text(const std::string& text) const {
    if (!font_) return false;
    
    ImGuiIO& io = ImGui::GetIO();
    for (unsigned char c : text) {
        if (c == 0) break;
        if (font_->FindGlyph(c) == nullptr) {
            return false;
        }
    }
    return true;
}

// ShaderResource implementation
ShaderResource::ShaderResource(const std::string& id, const std::string& vertex_source, const std::string& fragment_source)
    : Resource(id, ResourceType::Shader)
    , vertex_source_(vertex_source)
    , fragment_source_(fragment_source) {
    
    // Approximate shader memory usage
    set_size(vertex_source_.size() + fragment_source_.size());
    update_last_access();
}

ShaderResource::~ShaderResource() {
    // Shader cleanup is handled by the graphics API
}

// BufferResource implementation
BufferResource::BufferResource(const std::string& id, ResourceType type, void* data, size_t size, BufferUsage usage)
    : Resource(id, type)
    , data_(data)
    , buffer_size_(size)
    , usage_(usage) {
    
    set_size(size);
    update_last_access();
}

BufferResource::~BufferResource() {
    if (data_ && usage_ == BufferUsage::Static) {
        // Free static buffer data
        free(data_);
    }
}

void BufferResource::update_data(void* new_data, size_t new_size) {
    if (usage_ == BufferUsage::Static && data_) {
        free(data_);
    }
    
    data_ = new_data;
    buffer_size_ = new_size;
    set_size(new_size);
    update_last_access();
}

// Manager implementation
Manager& Manager::instance() {
    static Manager instance_;
    return instance_;
}

std::shared_ptr<TextureResource> Manager::get_texture(const std::string& id, const TextureCreator& creator) {
    if (id.empty() || !creator) return nullptr;
    
    std::shared_ptr<TextureResource> resource;
    
    // Try pool first
    resource = texture_pool_.get(id);
    if (resource) {
        resource->update_last_access();
        return resource;
    }
    
    // Create new resource
    resource = creator(id);
    if (resource) {
        std::unique_lock<std::shared_mutex> lock(resources_mutex_);
        
        // Check if resource already exists
        auto it = resources_.find(id);
        if (it != resources_.end()) {
            // Resource exists, use it
            return std::static_pointer_cast<TextureResource>(it->second);
        }
        
        // Store new resource
        resources_[id] = resource;
        update_memory_stats(ResourceType::Texture, resource->get_size(), true);
    }
    
    return resource;
}

std::shared_ptr<FontResource> Manager::get_font(const std::string& id, const FontCreator& creator) {
    if (id.empty() || !creator) return nullptr;
    
    std::shared_ptr<FontResource> resource;
    
    // Try pool first
    resource = font_pool_.get(id);
    if (resource) {
        resource->update_last_access();
        return resource;
    }
    
    // Create new resource
    resource = creator(id);
    if (resource) {
        std::unique_lock<std::shared_mutex> lock(resources_mutex_);
        
        auto it = resources_.find(id);
        if (it != resources_.end()) {
            return std::static_pointer_cast<FontResource>(it->second);
        }
        
        resources_[id] = resource;
        update_memory_stats(ResourceType::Font, resource->get_size(), true);
    }
    
    return resource;
}

std::shared_ptr<ShaderResource> Manager::get_shader(const std::string& id, const ShaderCreator& creator) {
    if (id.empty() || !creator) return nullptr;
    
    std::shared_ptr<ShaderResource> resource;
    
    // Try pool first
    resource = shader_pool_.get(id);
    if (resource) {
        resource->update_last_access();
        return resource;
    }
    
    // Create new resource
    resource = creator(id);
    if (resource) {
        std::unique_lock<std::shared_mutex> lock(resources_mutex_);
        
        auto it = resources_.find(id);
        if (it != resources_.end()) {
            return std::static_pointer_cast<ShaderResource>(it->second);
        }
        
        resources_[id] = resource;
        update_memory_stats(ResourceType::Shader, resource->get_size(), true);
    }
    
    return resource;
}

std::shared_ptr<BufferResource> Manager::get_buffer(const std::string& id, ResourceType type, const BufferCreator& creator) {
    if (id.empty() || !creator) return nullptr;
    
    std::shared_ptr<BufferResource> resource;
    
    // Try pool first
    resource = buffer_pool_.get(id);
    if (resource) {
        resource->update_last_access();
        return resource;
    }
    
    // Create new resource
    resource = creator(id);
    if (resource) {
        std::unique_lock<std::shared_mutex> lock(resources_mutex_);
        
        auto it = resources_.find(id);
        if (it != resources_.end()) {
            return std::static_pointer_cast<BufferResource>(it->second);
        }
        
        resources_[id] = resource;
        update_memory_stats(type, resource->get_size(), true);
    }
    
    return resource;
}

std::shared_ptr<Resource> Manager::get_resource(const std::string& id) {
    if (id.empty()) return nullptr;
    
    std::shared_lock<std::shared_mutex> lock(resources_mutex_);
    auto it = resources_.find(id);
    
    if (it != resources_.end()) {
        it->second->update_last_access();
        return it->second;
    }
    
    return nullptr;
}

bool Manager::has_resource(const std::string& id) const {
    if (id.empty()) return false;
    
    std::shared_lock<std::shared_mutex> lock(resources_mutex_);
    return resources_.find(id) != resources_.end();
}

void Manager::release_resource(const std::string& id) {
    if (id.empty()) return;
    
    std::unique_lock<std::shared_mutex> lock(resources_mutex_);
    auto it = resources_.find(id);
    
    if (it != resources_.end()) {
        auto resource = it->second;
        
        // Update statistics
        update_memory_stats(resource->get_type(), resource->get_size(), false);
        
        // Release to appropriate pool
        switch (resource->get_type()) {
            case ResourceType::Texture:
                if (auto tex = std::dynamic_pointer_cast<TextureResource>(resource)) {
                    texture_pool_.release(id, tex);
                }
                break;
            case ResourceType::Font:
                if (auto font = std::dynamic_pointer_cast<FontResource>(resource)) {
                    font_pool_.release(id, font);
                }
                break;
            case ResourceType::Shader:
                if (auto shader = std::dynamic_pointer_cast<ShaderResource>(resource)) {
                    shader_pool_.release(id, shader);
                }
                break;
            default:
                if (auto buffer = std::dynamic_pointer_cast<BufferResource>(resource)) {
                    buffer_pool_.release(id, buffer);
                }
                break;
        }
        
        resources_.erase(it);
    }
}

void Manager::validate_resources() {
    std::unique_lock<std::shared_mutex> lock(resources_mutex_);
    
    for (auto it = resources_.begin(); it != resources_.end();) {
        auto resource = it->second;
        
        // Check if resource is still valid
        bool is_valid = true;
        
        switch (resource->get_type()) {
            case ResourceType::Texture:
                if (auto tex = std::dynamic_pointer_cast<TextureResource>(resource)) {
                    is_valid = tex->get_texture() != nullptr;
                }
                break;
            case ResourceType::Font:
                if (auto font = std::dynamic_pointer_cast<FontResource>(resource)) {
                    is_valid = font->get_font() != nullptr;
                }
                break;
            default:
                // For other resource types, assume they're valid if they exist
                break;
        }
        
        if (!is_valid) {
            update_memory_stats(resource->get_type(), resource->get_size(), false);
            it = resources_.erase(it);
        } else {
            ++it;
        }
    }
}

void Manager::cleanup_stale_resources(std::chrono::seconds timeout) {
    auto now = std::chrono::steady_clock::now();
    
    std::unique_lock<std::shared_mutex> lock(resources_mutex_);
    
    for (auto it = resources_.begin(); it != resources_.end();) {
        auto resource = it->second;
        
        if (resource->is_stale(timeout)) {
            // Release to pool instead of deleting
            switch (resource->get_type()) {
                case ResourceType::Texture:
                    if (auto tex = std::dynamic_pointer_cast<TextureResource>(resource)) {
                        texture_pool_.release(it->first, tex);
                    }
                    break;
                case ResourceType::Font:
                    if (auto font = std::dynamic_pointer_cast<FontResource>(resource)) {
                        font_pool_.release(it->first, font);
                    }
                    break;
                case ResourceType::Shader:
                    if (auto shader = std::dynamic_pointer_cast<ShaderResource>(resource)) {
                        shader_pool_.release(it->first, shader);
                    }
                    break;
                default:
                    if (auto buffer = std::dynamic_pointer_cast<BufferResource>(resource)) {
                        buffer_pool_.release(it->first, buffer);
                    }
                    break;
            }
            
            update_memory_stats(resource->get_type(), resource->get_size(), false);
            it = resources_.erase(it);
        } else {
            ++it;
        }
    }
}

void Manager::clear_all_resources() {
    std::unique_lock<std::shared_mutex> lock(resources_mutex_);
    
    // Clear all resources
    for (auto& pair : resources_) {
        update_memory_stats(pair.second->get_type(), pair.second->get_size(), false);
    }
    
    resources_.clear();
    
    // Clean up pools
    texture_pool_.cleanup();
    font_pool_.cleanup();
    shader_pool_.cleanup();
    buffer_pool_.cleanup();
}

void Manager::set_pool_size(size_t texture_pool, size_t font_pool, size_t shader_pool, size_t buffer_pool) {
    texture_pool_.set_max_size(texture_pool);
    font_pool_.set_max_size(font_pool);
    shader_pool_.set_max_size(shader_pool);
    buffer_pool_.set_max_size(buffer_pool);
}

void Manager::cleanup_pools() {
    texture_pool_.cleanup();
    font_pool_.cleanup();
    shader_pool_.cleanup();
    buffer_pool_.cleanup();
}

void Manager::log_resource_usage() const {
    auto stats = get_stats();
    
    std::cout << "=== GUI Resource Manager Statistics ===\n";
    std::cout << "Total Allocated: " << utilities::format_memory_size(stats.total_allocated) << "\n";
    std::cout << "Total Freed: " << utilities::format_memory_size(stats.total_freed) << "\n";
    std::cout << "Current Usage: " << utilities::format_memory_size(stats.current_usage()) << "\n";
    std::cout << "Active Textures: " << stats.active_textures << "\n";
    std::cout << "Active Fonts: " << stats.active_fonts << "\n";
    std::cout << "Active Shaders: " << stats.active_shaders << "\n";
    std::cout << "Active Buffers: " << stats.active_buffers << "\n";
    std::cout << "Pool Efficiency: " << std::fixed << std::setprecision(1) << stats.pool_efficiency() << "%\n";
    std::cout << "Pool Hits: " << stats.pool_hits << "\n";
    std::cout << "Pool Misses: " << stats.pool_misses << "\n";
    
    std::cout << "\n=== Pool Statistics ===\n";
    std::cout << "Texture Pool: " << texture_pool_.get_available_count() << " available\n";
    std::cout << "Font Pool: " << font_pool_.get_available_count() << " available\n";
    std::cout << "Shader Pool: " << shader_pool_.get_available_count() << " available\n";
    std::cout << "Buffer Pool: " << buffer_pool_.get_available_count() << " available\n";
}

void Manager::generate_memory_report(std::ostream& report) const {
    auto stats = get_stats();
    
    report << "GUI Resource Manager Memory Report\n";
    report << "==================================\n\n";
    
    report << "Memory Statistics:\n";
    report << "------------------\n";
    report << "Total Allocated: " << utilities::format_memory_size(stats.total_allocated) << "\n";
    report << "Total Freed: " << utilities::format_memory_size(stats.total_freed) << "\n";
    report << "Current Usage: " << utilities::format_memory_size(stats.current_usage()) << "\n";
    report << "Memory Limit: " << utilities::format_memory_size(memory_limit_) << "\n";
    report << "Usage vs Limit: " << std::fixed << std::setprecision(1) 
           << (stats.current_usage() * 100.0) / memory_limit_ << "%\n\n";
    
    report << "Resource Counts:\n";
    report << "----------------\n";
    report << "Active Textures: " << stats.active_textures << "\n";
    report << "Active Fonts: " << stats.active_fonts << "\n";
    report << "Active Shaders: " << stats.active_shaders << "\n";
    report << "Active Buffers: " << stats.active_buffers << "\n\n";
    
    report << "Pool Performance:\n";
    report << "-----------------\n";
    report << "Pool Efficiency: " << std::fixed << std::setprecision(1) << stats.pool_efficiency() << "%\n";
    report << "Pool Hits: " << stats.pool_hits << "\n";
    report << "Pool Misses: " << stats.pool_misses << "\n\n";
    
    report << "Pool Statistics:\n";
    report << "---------------\n";
    report << "Texture Pool: " << texture_pool_.get_available_count() << " available (" 
           << texture_pool_.get_hits() << " hits, " << texture_pool_.get_misses() << " misses)\n";
    report << "Font Pool: " << font_pool_.get_available_count() << " available (" 
           << font_pool_.get_hits() << " hits, " << font_pool_.get_misses() << " misses)\n";
    report << "Shader Pool: " << shader_pool_.get_available_count() << " available (" 
           << shader_pool_.get_hits() << " hits, " << shader_pool_.get_misses() << " misses)\n";
    report << "Buffer Pool: " << buffer_pool_.get_available_count() << " available (" 
           << buffer_pool_.get_hits() << " hits, " << buffer_pool_.get_misses() << " misses)\n\n";
    
    // Memory usage by resource type
    std::unordered_map<ResourceType, uint64_t> type_usage;
    {
        std::shared_lock<std::shared_mutex> lock(resources_mutex_);
        for (const auto& pair : resources_) {
            type_usage[pair.second->get_type()] += pair.second->get_size();
        }
    }
    
    report << "Memory by Resource Type:\n";
    report << "-----------------------\n";
    for (const auto& pair : type_usage) {
        report << utilities::resource_type_to_string(pair.first) << ": " 
               << utilities::format_memory_size(pair.second) << "\n";
    }
}

void Manager::integrate_with_ui_manager() {
    // This would integrate with Cataclysm-BN's ui_manager
    // to hook into redraw and resize events
}

void Manager::integrate_with_sdl_renderer(void* renderer) {
    // This would integrate with SDL2 renderer for texture management
}

void Manager::integrate_with_input_system() {
    // This would integrate with input system for resource hotkeys
}

void Manager::update_memory_stats(ResourceType type, uint64_t size, bool allocating) {
    if (allocating) {
        stats_.total_allocated += size;
        
        switch (type) {
            case ResourceType::Texture:
                stats_.active_textures++;
                break;
            case ResourceType::Font:
                stats_.active_fonts++;
                break;
            case ResourceType::Shader:
                stats_.active_shaders++;
                break;
            default:
                stats_.active_buffers++;
                break;
        }
    } else {
        stats_.total_freed += size;
        
        switch (type) {
            case ResourceType::Texture:
                stats_.active_textures--;
                break;
            case ResourceType::Font:
                stats_.active_fonts--;
                break;
            case ResourceType::Shader:
                stats_.active_shaders--;
                break;
            default:
                stats_.active_buffers--;
                break;
        }
    }
}

bool Manager::check_memory_limit(uint64_t additional_size) const {
    uint64_t current_usage = stats_.current_usage();
    return (current_usage + additional_size) <= memory_limit_;
}

void Manager::enforce_memory_limit() {
    uint64_t current_usage = stats_.current_usage();
    
    if (current_usage > memory_limit_) {
        // Clean up stale resources first
        cleanup_stale_resources(std::chrono::seconds(60)); // Shorter timeout for memory pressure
        
        current_usage = stats_.current_usage();
        
        if (current_usage > memory_limit_) {
            // If still over limit, clean all pools
            cleanup_pools();
            
            current_usage = stats_.current_usage();
            
            if (current_usage > memory_limit_) {
                // Last resort: clear least recently used resources
                std::vector<std::pair<std::string, std::shared_ptr<Resource>>> resources_copy;
                {
                    std::shared_lock<std::shared_mutex> lock(resources_mutex_);
                    resources_copy.reserve(resources_.size());
                    for (const auto& pair : resources_) {
                        resources_copy.emplace_back(pair.first, pair.second);
                    }
                }
                
                // Sort by last access time (oldest first)
                std::sort(resources_copy.begin(), resources_copy.end(),
                    [](const auto& a, const auto& b) {
                        return a.second->get_last_access() < b.second->get_last_access();
                    });
                
                // Remove oldest resources until under limit
                for (const auto& pair : resources_copy) {
                    release_resource(pair.first);
                    current_usage = stats_.current_usage();
                    if (current_usage <= memory_limit_) {
                        break;
                    }
                }
            }
        }
    }
}

template<typename T>
std::shared_ptr<T> Manager::get_pooled_resource(
    const std::string& id,
    ResourceType type,
    ResourcePool<T>& pool,
    const std::function<std::shared_ptr<T>(const std::string&)>& creator) {
    
    if (id.empty() || !creator) return nullptr;
    
    // Try pool first
    auto resource = pool.get(id);
    if (resource) {
        resource->update_last_access();
        return resource;
    }
    
    // Create new resource
    resource = creator(id);
    if (resource) {
        std::unique_lock<std::shared_mutex> lock(resources_mutex_);
        
        // Check if resource already exists
        auto it = resources_.find(id);
        if (it != resources_.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        
        // Store new resource
        resources_[id] = resource;
        update_memory_stats(type, resource->get_size(), true);
    }
    
    return resource;
}

// ScopedTexture implementation
ScopedTexture::ScopedTexture(const std::string& id, Manager& manager) : manager_(manager) {
    if (!id.empty()) {
        resource_ = manager_.get_resource(id);
        if (resource_ && resource_->get_type() == ResourceType::Texture) {
            id_ = id;
        }
    }
}

ScopedTexture::ScopedTexture(ScopedTexture&& other) noexcept 
    : manager_(other.manager_)
    , id_(std::move(other.id_))
    , resource_(std::move(other.resource_)) {
    other.id_.clear();
    other.resource_.reset();
}

ScopedTexture& ScopedTexture::operator=(ScopedTexture&& other) noexcept {
    if (this != &other) {
        release();
        manager_ = other.manager_;
        id_ = std::move(other.id_);
        resource_ = std::move(other.resource_);
        other.id_.clear();
        other.resource_.reset();
    }
    return *this;
}

// ScopedFont implementation
ScopedFont::ScopedFont(const std::string& id, Manager& manager) : manager_(manager) {
    if (!id.empty()) {
        resource_ = manager_.get_resource(id);
        if (resource_ && resource_->get_type() == ResourceType::Font) {
            id_ = id;
        }
    }
}

ScopedFont::ScopedFont(ScopedFont&& other) noexcept 
    : manager_(other.manager_)
    , id_(std::move(other.id_))
    , resource_(std::move(other.resource_)) {
    other.id_.clear();
    other.resource_.reset();
}

ScopedFont& ScopedFont::operator=(ScopedFont&& other) noexcept {
    if (this != &other) {
        release();
        manager_ = other.manager_;
        id_ = std::move(other.id_);
        resource_ = std::move(other.resource_);
        other.id_.clear();
        other.resource_.reset();
    }
    return *this;
}

// ScopedShader implementation
ScopedShader::ScopedShader(const std::string& id, Manager& manager) : manager_(manager) {
    if (!id.empty()) {
        resource_ = manager_.get_resource(id);
        if (resource_ && resource_->get_type() == ResourceType::Shader) {
            id_ = id;
        }
    }
}

ScopedShader::ScopedShader(ScopedShader&& other) noexcept 
    : manager_(other.manager_)
    , id_(std::move(other.id_))
    , resource_(std::move(other.resource_)) {
    other.id_.clear();
    other.resource_.reset();
}

ScopedShader& ScopedShader::operator=(ScopedShader&& other) noexcept {
    if (this != &other) {
        release();
        manager_ = other.manager_;
        id_ = std::move(other.id_);
        resource_ = std::move(other.resource_);
        other.id_.clear();
        other.resource_.reset();
    }
    return *this;
}

// ScopedBuffer implementation
ScopedBuffer::ScopedBuffer(const std::string& id, ResourceType type, Manager& manager) 
    : manager_(manager), type_(type) {
    if (!id.empty()) {
        resource_ = manager_.get_resource(id);
        if (resource_ && resource_->get_type() == type) {
            id_ = id;
        }
    }
}

ScopedBuffer::ScopedBuffer(ScopedBuffer&& other) noexcept 
    : manager_(other.manager_)
    , id_(std::move(other.id_))
    , type_(other.type_)
    , resource_(std::move(other.resource_)) {
    other.id_.clear();
    other.type_ = ResourceType::Texture;
    other.resource_.reset();
}

ScopedBuffer& ScopedBuffer::operator=(ScopedBuffer&& other) noexcept {
    if (this != &other) {
        release();
        manager_ = other.manager_;
        id_ = std::move(other.id_);
        type_ = other.type_;
        resource_ = std::move(other.resource_);
        other.id_.clear();
        other.type_ = ResourceType::Texture;
        other.resource_.reset();
    }
    return *this;
}

// Utility functions implementation
namespace utilities {

std::string format_memory_size(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
    return oss.str();
}

std::string resource_type_to_string(ResourceType type) {
    switch (type) {
        case ResourceType::Texture: return "Texture";
        case ResourceType::Font: return "Font";
        case ResourceType::Shader: return "Shader";
        case ResourceType::VertexBuffer: return "Vertex Buffer";
        case ResourceType::IndexBuffer: return "Index Buffer";
        case ResourceType::UniformBuffer: return "Uniform Buffer";
        case ResourceType::FrameBuffer: return "Frame Buffer";
        case ResourceType::Sampler: return "Sampler";
        case ResourceType::Material: return "Material";
        default: return "Unknown";
    }
}

bool is_valid_resource_id(const std::string& id) {
    if (id.empty()) return false;
    
    // Check for valid characters: alphanumeric, underscore, dash, dot
    for (char c : id) {
        if (!std::isalnum(c) && c != '_' && c != '-' && c != '.') {
            return false;
        }
    }
    
    // Check length limits
    if (id.length() > 256) return false;
    
    // Check for reasonable naming patterns
    // Should not start with number or special character
    if (std::isdigit(id[0])) return false;
    
    return true;
}

std::string generate_unique_id(const std::string& prefix) {
    static std::atomic<uint64_t> counter{0};
    uint64_t id = counter.fetch_add(1);
    
    std::ostringstream oss;
    if (!prefix.empty()) {
        oss << prefix << "_";
    }
    oss << std::hex << std::uppercase << id;
    return oss.str();
}

} // namespace utilities

} // namespace resource_manager

} // namespace gui
