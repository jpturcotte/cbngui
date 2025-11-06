#ifndef GUI_MANAGER_H
#define GUI_MANAGER_H

#include <memory>
#include <string>
#include <functional>
#include <SDL.h>

/**
 * @brief Core GUI manager for Cataclysm-BN Dear ImGui overlay integration
 * 
 * This class manages the lifecycle of the GUI overlay system, including:
 * - Dear ImGui integration with SDL2
 * - UI manager registration and lifecycle coordination
 * - Window management and context handling
 * - Proper initialization and shutdown procedures
 * 
 * Follows PIMPL idiom to minimize header pollution and compilation coupling.
 */
class GUIManager {
public:
    // Forward declarations for PIMPL pattern
    struct Impl;
    
    /**
     * @brief Configuration structure for GUI manager initialization
     */
    struct Config {
        bool enabled = true;                    /**< Enable/disable GUI overlay */
        bool pass_through_input = true;         /**< Pass unconsumed events to game */
        float dpi_scale = 1.0f;                 /**< DPI scaling factor */
        bool minimize_pause = true;             /**< Pause on window minimize */
        std::string ini_filename;               /**< Custom ImGui ini filename (null for default) */
    };
    
    /**
     * @brief Initialize the GUI manager
     * 
     * @param window SDL2 window handle
     * @param renderer SDL2 renderer handle
     * @param config Configuration parameters
     * @return true if initialization successful, false otherwise
     */
    bool Initialize(SDL_Window* window, SDL_Renderer* renderer, const Config& config = Config());
    
    /**
     * @brief Shutdown the GUI manager and clean up resources
     */
    void Shutdown();
    
    /**
     * @brief Update GUI state (called per frame)
     */
    void Update();
    
    /**
     * @brief Render the GUI overlay (called after base scene)
     */
    void Render();
    
    /**
     * @brief Handle SDL events for input routing
     * 
     * @param event SDL event to process
     * @return true if event was consumed by GUI, false if should pass to game
     */
    bool HandleEvent(const SDL_Event& event);
    
    /**
     * @brief Open the GUI overlay
     */
    void Open();
    
    /**
     * @brief Close the GUI overlay
     */
    void Close();
    
    /**
     * @brief Check if GUI overlay is currently open
     */
    bool IsOpen() const;
    
    /**
     * @brief Check if GUI overlay is enabled
     */
    bool IsEnabled() const;
    
    /**
     * @brief Enable or disable the GUI overlay
     */
    void SetEnabled(bool enabled);
    
    /**
     * @brief Check if window is minimized
     */
    bool IsMinimized() const;
    
    /**
     * @brief Set window focus state
     */
    void SetFocused(bool focused);
    
    /**
     * @brief Handle window resize
     */
    void OnWindowResized(int width, int height);
    
    // Callbacks for external systems
    using RedrawCallback = std::function<void()>;
    using ResizeCallback = std::function<void(int, int)>;
    
    /**
     * @brief Register redraw callback (called when UI needs redraw)
     */
    void RegisterRedrawCallback(RedrawCallback callback);
    
    /**
     * @brief Register resize callback (called on window resize)
     */
    void RegisterResizeCallback(ResizeCallback callback);
    
    /**
     * @brief Get error message from last failed operation
     */
    const std::string& GetLastError() const;
    
    // Rule of five - proper lifecycle management
    GUIManager();
    ~GUIManager();
    GUIManager(const GUIManager&) = delete;
    GUIManager& operator=(const GUIManager&) = delete;
    GUIManager(GUIManager&&) = delete;
    GUIManager& operator=(GUIManager&&) = delete;

private:
    std::unique_ptr<Impl> pImpl_;  /**< PIMPL pointer for implementation details */
    
    /**
     * @brief Internal initialization that can fail
     */
    bool InitializeInternal(const Config& config);
    
    /**
     * @brief Validate configuration parameters
     */
    bool ValidateConfig(const Config& config) const;
    
    /**
     * @brief Log error and set last error message
     */
    void LogError(const std::string& error);
    
    /**
     * @brief Check if system is in graphical build mode
     */
    bool IsGraphicalBuild() const;
};

#endif // GUI_MANAGER_H