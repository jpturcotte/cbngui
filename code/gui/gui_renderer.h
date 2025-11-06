#ifndef GUI_RENDERER_H
#define GUI_RENDERER_H

#include <SDL.h>
#include <string>
#include <memory>
#include <functional>

// Forward declarations for Dear ImGui
struct ImGuiContext;
struct ImGuiIO;

/**
 * @brief GUI Renderer class for Dear ImGui integration with SDL2
 * 
 * This class handles:
 * - Dear ImGui context creation and management
 * - SDL2 backend initialization for ImGui
 * - Render pipeline integration
 * - Font and texture management
 * - DPI scaling and high-DPI support
 */
class GUIRenderer {
public:
    /**
     * @brief Initialize Dear ImGui with SDL2 backends
     * 
     * @param window SDL2 window handle
     * @param renderer SDL2 renderer handle
     * @param dpi_scale DPI scaling factor
     * @return true if initialization successful, false otherwise
     */
    bool Initialize(SDL_Window* window, SDL_Renderer* renderer, float dpi_scale = 1.0f);
    
    /**
     * @brief Shutdown Dear ImGui and clean up resources
     */
    void Shutdown();
    
    /**
     * @brief Update Dear ImGui state (called per frame)
     */
    void Update();
    
    /**
     * @brief Render the GUI overlay using ImDrawLists
     */
    void Render();
    
    /**
     * @brief Process SDL events through ImGui input system
     * 
     * @param event SDL event to process
     * @return true if event was consumed by ImGui, false otherwise
     */
    bool HandleEvent(const SDL_Event& event);
    
    /**
     * @brief Handle window resize events
     * 
     * @param width New window width
     * @param height New window height
     */
    void OnWindowResized(int width, int height);
    
    /**
     * @brief Set custom ImGui ini filename
     * 
     * @param filename Custom ini filename (empty for default behavior)
     */
    void SetIniFilename(const std::string& filename);
    
    /**
     * @brief Set ImGui log filename
     * 
     * @param filename Log filename (empty to disable logging)
     */
    void SetLogFilename(const std::string& filename);
    
    /**
     * @brief Enable/disable ImGui docking
     * 
     * @param enabled Enable docking feature
     */
    void SetDockingEnabled(bool enabled);
    
    /**
     * @brief Enable/disable ImGui viewports
     * 
     * @param enabled Enable viewports feature
     */
    void SetViewportsEnabled(bool enabled);
    
    /**
     * @brief Get Dear ImGui context
     * 
     * @return ImGui context pointer, or nullptr if not initialized
     */
    ImGuiContext* GetContext() const;
    
    /**
     * @brief Get ImGui I/O interface
     * 
     * @return ImGui IO reference, or nullptr if not initialized
     */
    ImGuiIO* GetIO() const;
    
    /**
     * @brief Check if renderer is initialized
     * 
     * @return true if initialized, false otherwise
     */
    bool IsInitialized() const;
    
    /**
     * @brief Check if Dear ImGui context is valid
     * 
     * @return true if context exists, false otherwise
     */
    bool HasContext() const;
    
    /**
     * @brief Force rebuild of font atlas
     */
    void RebuildFontAtlas();
    
    // Rule of five - proper lifecycle management
    GUIRenderer();
    ~GUIRenderer();
    GUIRenderer(const GUIRenderer&) = delete;
    GUIRenderer& operator=(const GUIRenderer&) = delete;
    GUIRenderer(GUIRenderer&&) = delete;
    GUIRenderer& operator=(GUIRenderer&&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
    
    /**
     * @brief Check SDL version compatibility
     */
    bool CheckSDLVersion() const;
    
    /**
     * @brief Setup ImGui configuration
     */
    void SetupImGuiConfig();
    
    /**
     * @brief Apply DPI scaling settings
     */
    void ApplyDPISettings(float dpi_scale);
    
    /**
     * @brief Log initialization error
     */
    void LogError(const std::string& error);
};

#endif // GUI_RENDERER_H