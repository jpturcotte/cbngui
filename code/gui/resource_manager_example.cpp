#include "resource_manager.h"
#include <iostream>
#include <vector>
#include <string>

// Example usage of the GUI Resource Manager system
// This demonstrates the RAII patterns, resource pooling, and memory tracking

namespace example {

// Example texture creation function
std::shared_ptr<gui::resource_manager::TextureResource> create_default_texture(const std::string& id) {
    // In a real implementation, this would create an actual ImGui texture
    // For this example, we'll simulate texture creation
    void* mock_texture = reinterpret_cast<void*>(0x12345678); // Mock texture pointer
    uint32_t width = 64;
    uint32_t height = 64;
    
    return std::make_shared<gui::resource_manager::TextureResource>(id, mock_texture, width, height);
}

// Example font creation function
std::shared_ptr<gui::resource_manager::FontResource> create_default_font(const std::string& id) {
    // In a real implementation, this would create an actual ImGui font
    // For this example, we'll simulate font creation
    ImGui::ImFont* mock_font = nullptr; // In real code, this would be a valid ImFont pointer
    
    return std::make_shared<gui::resource_manager::FontResource>(id, mock_font);
}

// Example shader creation function
std::shared_ptr<gui::resource_manager::ShaderResource> create_default_shader(const std::string& id) {
    std::string vertex_shader = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";
    
    std::string fragment_shader = R"(
        #version 330 core
        out vec4 FragColor;
        void main() {
            FragColor = vec4(1.0, 0.5, 0.2, 1.0);
        }
    )";
    
    return std::make_shared<gui::resource_manager::ShaderResource>(id, vertex_shader, fragment_shader);
}

// Example buffer creation function
std::shared_ptr<gui::resource_manager::BufferResource> create_default_buffer(const std::string& id) {
    void* mock_data = malloc(1024); // Mock buffer data
    size_t size = 1024;
    auto usage = gui::resource_manager::BufferResource::BufferUsage::Dynamic;
    
    return std::make_shared<gui::resource_manager::BufferResource>(id, 
        gui::resource_manager::ResourceType::VertexBuffer, mock_data, size, usage);
}

void demonstrate_basic_usage() {
    std::cout << "\n=== Basic Resource Management Demo ===\n";
    
    auto& manager = gui::resource_manager::Manager::instance();
    
    // Get a texture using the manager
    auto texture = manager.get_texture("player_sprite", create_default_texture);
    if (texture) {
        std::cout << "Created texture: " << texture->get_id() 
                  << " (" << texture->get_width() << "x" << texture->get_height() << ")\n";
    }
    
    // Get a font
    auto font = manager.get_font("default_font", create_default_font);
    if (font) {
        std::cout << "Created font: " << font->get_id() 
                  << " (size: " << font->get_size() << " bytes)\n";
    }
    
    // Get a shader
    auto shader = manager.get_shader("ui_shader", create_default_shader);
    if (shader) {
        std::cout << "Created shader: " << shader->get_id() 
                  << " (compiled: " << (shader->is_compiled() ? "yes" : "no") << ")\n";
    }
    
    // Check if resource exists
    std::cout << "Has 'player_sprite': " << (manager.has_resource("player_sprite") ? "yes" : "no") << "\n";
    std::cout << "Has 'nonexistent': " << (manager.has_resource("nonexistent") ? "yes" : "no") << "\n";
    
    // Release resources
    manager.release_resource("player_sprite");
    manager.release_resource("default_font");
    manager.release_resource("ui_shader");
    
    std::cout << "Resources released.\n";
}

void demonstrate_raii_patterns() {
    std::cout << "\n=== RAII Resource Management Demo ===\n";
    
    auto& manager = gui::resource_manager::Manager::instance();
    
    // Use ScopedTexture for automatic resource management
    {
        std::cout << "Creating scoped texture...\n";
        gui::resource_manager::ScopedTexture scoped_texture("ui_button", manager);
        
        if (scoped_texture) {
            std::cout << "Scoped texture created: " << scoped_texture.get_id() << "\n";
        }
        
        // Texture will be automatically released when scoped_texture goes out of scope
    }
    
    std::cout << "Scoped texture automatically released.\n";
    
    // Use ScopedFont for automatic font management
    {
        std::cout << "Creating scoped font...\n";
        gui::resource_manager::ScopedFont scoped_font("title_font", manager);
        
        if (scoped_font) {
            std::cout << "Scoped font created: " << scoped_font.get_id() << "\n";
        }
    }
    
    std::cout << "Scoped font automatically released.\n";
}

void demonstrate_resource_pooling() {
    std::cout << "\n=== Resource Pooling Demo ===\n";
    
    auto& manager = gui::resource_manager::Manager::instance();
    
    // Get the same texture multiple times to demonstrate pooling
    std::vector<std::string> texture_ids = {"pool_texture_1", "pool_texture_1", "pool_texture_1"};
    
    for (size_t i = 0; i < texture_ids.size(); ++i) {
        std::cout << "Getting texture " << (i+1) << " time...\n";
        auto texture = manager.get_texture(texture_ids[i], create_default_texture);
        if (texture) {
            std::cout << "Got texture: " << texture->get_id() 
                      << " at address: " << texture.get() << "\n";
        }
    }
    
    // Print pool statistics
    auto stats = manager.get_stats();
    std::cout << "Pool efficiency: " << std::fixed << std::setprecision(1) 
              << stats.pool_efficiency() << "%\n";
    std::cout << "Pool hits: " << stats.pool_hits << "\n";
    std::cout << "Pool misses: " << stats.pool_misses << "\n";
    
    // Clean up
    manager.release_resource("pool_texture_1");
}

void demonstrate_memory_tracking() {
    std::cout << "\n=== Memory Tracking Demo ===\n";
    
    auto& manager = gui::resource_manager::Manager::instance();
    
    // Create several resources to track memory usage
    std::vector<std::string> resource_ids = {
        "memory_texture_1", "memory_texture_2", "memory_font_1", 
        "memory_shader_1", "memory_buffer_1"
    };
    
    std::cout << "Creating resources...\n";
    
    // Create textures
    for (int i = 0; i < 2; ++i) {
        auto id = "memory_texture_" + std::to_string(i + 1);
        manager.get_texture(id, create_default_texture);
    }
    
    // Create font
    manager.get_font("memory_font_1", create_default_font);
    
    // Create shader
    manager.get_shader("memory_shader_1", create_default_shader);
    
    // Create buffer
    manager.get_buffer("memory_buffer_1", 
        gui::resource_manager::ResourceType::VertexBuffer, create_default_buffer);
    
    // Print memory statistics
    auto stats = manager.get_stats();
    std::cout << "Current memory usage: " << gui::resource_manager::utilities::format_memory_size(stats.current_usage()) << "\n";
    std::cout << "Total allocated: " << gui::resource_manager::utilities::format_memory_size(stats.total_allocated) << "\n";
    std::cout << "Active textures: " << stats.active_textures << "\n";
    std::cout << "Active fonts: " << stats.active_fonts << "\n";
    std::cout << "Active shaders: " << stats.active_shaders << "\n";
    std::cout << "Active buffers: " << stats.active_buffers << "\n";
    
    // Set memory limit and check
    manager.set_memory_limit(50 * 1024 * 1024); // 50MB
    std::cout << "Memory limit set to: " << gui::resource_manager::utilities::format_memory_size(50 * 1024 * 1024) << "\n";
    
    // Clean up all resources
    for (const auto& id : resource_ids) {
        manager.release_resource(id);
    }
    
    std::cout << "After cleanup - Current memory usage: " 
              << gui::resource_manager::utilities::format_memory_size(stats.current_usage()) << "\n";
}

void demonstrate_memory_reports() {
    std::cout << "\n=== Memory Reports Demo ===\n";
    
    auto& manager = gui::resource_manager::Manager::instance();
    
    // Create some resources first
    manager.get_texture("report_texture_1", create_default_texture);
    manager.get_texture("report_texture_2", create_default_texture);
    manager.get_font("report_font_1", create_default_font);
    manager.get_shader("report_shader_1", create_default_shader);
    
    // Generate and print memory report
    std::cout << "\n--- Detailed Memory Report ---\n";
    std::ostringstream report;
    manager.generate_memory_report(report);
    std::cout << report.str();
    
    // Print usage statistics
    std::cout << "\n--- Usage Statistics ---\n";
    manager.log_resource_usage();
    
    // Clean up
    manager.release_resource("report_texture_1");
    manager.release_resource("report_texture_2");
    manager.release_resource("report_font_1");
    manager.release_resource("report_shader_1");
}

void demonstrate_resource_validation() {
    std::cout << "\n=== Resource Validation Demo ===\n";
    
    auto& manager = gui::resource_manager::Manager::instance();
    
    // Create resources
    auto texture = manager.get_texture("validation_texture", create_default_texture);
    auto font = manager.get_font("validation_font", create_default_font);
    
    std::cout << "Created resources for validation test.\n";
    
    // Check initial state
    std::cout << "Has 'validation_texture': " << (manager.has_resource("validation_texture") ? "yes" : "no") << "\n";
    std::cout << "Has 'validation_font': " << (manager.has_resource("validation_font") ? "yes" : "no") << "\n";
    
    // Validate resources
    std::cout << "Validating resources...\n";
    manager.validate_resources();
    
    // Check state after validation
    std::cout << "After validation - Has 'validation_texture': " << (manager.has_resource("validation_texture") ? "yes" : "no") << "\n";
    std::cout << "After validation - Has 'validation_font': " << (manager.has_resource("validation_font") ? "yes" : "no") << "\n";
    
    // Clean up
    manager.release_resource("validation_texture");
    manager.release_resource("validation_font");
}

void demonstrate_utility_functions() {
    std::cout << "\n=== Utility Functions Demo ===\n";
    
    // Test memory size formatting
    std::vector<uint64_t> sizes = {1024, 1024 * 1024, 1024 * 1024 * 1024};
    std::vector<std::string> size_names = {"1KB", "1MB", "1GB"};
    
    for (size_t i = 0; i < sizes.size(); ++i) {
        std::string formatted = gui::resource_manager::utilities::format_memory_size(sizes[i]);
        std::cout << "Size " << size_names[i] << " formatted as: " << formatted << "\n";
    }
    
    // Test resource type string conversion
    auto types = {
        gui::resource_manager::ResourceType::Texture,
        gui::resource_manager::ResourceType::Font,
        gui::resource_manager::ResourceType::Shader,
        gui::resource_manager::ResourceType::VertexBuffer
    };
    
    for (auto type : types) {
        std::string type_str = gui::resource_manager::utilities::resource_type_to_string(type);
        std::cout << "Resource type " << static_cast<int>(type) << " as string: " << type_str << "\n";
    }
    
    // Test resource ID validation
    std::vector<std::string> test_ids = {
        "valid_id_123", "invalid-id!", "123invalid", "", "very_long_valid_id_that_exceeds_reasonable_limits_and_should_fail_validation"
    };
    
    for (const auto& id : test_ids) {
        bool valid = gui::resource_manager::utilities::is_valid_resource_id(id);
        std::cout << "ID '" << id << "' is " << (valid ? "valid" : "invalid") << "\n";
    }
    
    // Test unique ID generation
    for (int i = 0; i < 3; ++i) {
        std::string unique_id = gui::resource_manager::utilities::generate_unique_id("ui");
        std::cout << "Generated unique ID: " << unique_id << "\n";
    }
}

void run_all_examples() {
    std::cout << "GUI Resource Manager Examples\n";
    std::cout << "=============================\n";
    
    demonstrate_basic_usage();
    demonstrate_raii_patterns();
    demonstrate_resource_pooling();
    demonstrate_memory_tracking();
    demonstrate_memory_reports();
    demonstrate_resource_validation();
    demonstrate_utility_functions();
    
    std::cout << "\n=== Examples Complete ===\n";
}

} // namespace example

// Main function to run examples
int main() {
    try {
        example::run_all_examples();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error running examples: " << e.what() << "\n";
        return 1;
    }
}
