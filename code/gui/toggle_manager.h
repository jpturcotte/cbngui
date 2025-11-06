#ifndef TOGGLE_MANAGER_H
#define TOGGLE_MANAGER_H

#include <string>
#include <map>
#include <set>
#include <vector>
#include <functional>
#include <memory>
#include "json.h"

namespace CataclysmBN {
namespace GUI {

// Forward declarations
class GUISettings;

/**
 * ToggleManager handles enabling/disabling individual GUI elements
 * and provides notification system for UI state changes
 */
class ToggleManager {
public:
    // Constructor and Destructor
    ToggleManager();
    ~ToggleManager();

    // Copy and assignment operators
    ToggleManager(const ToggleManager&) = delete;
    ToggleManager& operator=(const ToggleManager&) = delete;

    // Singleton access
    static ToggleManager& getInstance();

    // Component management
    bool registerComponent(const std::string& componentId, 
                          const std::string& displayName,
                          bool defaultVisible = true,
                          const std::string& category = "General");
    
    bool unregisterComponent(const std::string& componentId);
    
    bool componentExists(const std::string& componentId) const;
    std::vector<std::string> getAllComponentIds() const;
    std::vector<std::string> getComponentIdsByCategory(const std::string& category) const;
    std::vector<std::string> getAllCategories() const;

    // Component state management
    bool setComponentVisible(const std::string& componentId, bool visible);
    bool isComponentVisible(const std::string& componentId) const;
    
    bool setComponentEnabled(const std::string& componentId, bool enabled);
    bool isComponentEnabled(const std::string& componentId) const;

    // Component display information
    std::string getComponentDisplayName(const std::string& componentId) const;
    std::string getComponentCategory(const std::string& componentId) const;
    
    // Batch operations
    void setCategoryVisible(const std::string& category, bool visible);
    void setCategoryEnabled(const std::string& category, bool enabled);
    
    void showAll();
    void hideAll();
    void enableAll();
    void disableAll();

    // Filter and query
    std::vector<std::string> getVisibleComponents() const;
    std::vector<std::string> getEnabledComponents() const;
    std::vector<std::string> getDisabledComponents() const;
    std::vector<std::string> getInvisibleComponents() const;

    // Persistence
    bool loadFromFile(const std::string& configPath = "");
    bool saveToFile(const std::string& configPath = "");
    void resetToDefaults();
    void setConfigPath(const std::string& path) { m_configPath = path; }
    std::string getConfigPath() const { return m_configPath; }

    // Serialization
    Json::Value serialize() const;
    bool deserialize(const Json::Value& data);
    std::string serializeToString() const;
    bool deserializeFromString(const std::string& data);

    // Event system
    using ComponentStateChangeCallback = std::function<void(const std::string& componentId, bool visible, bool enabled)>;
    using BulkStateChangeCallback = std::function<void(const std::string& category, bool visible, bool enabled)>;

    int addComponentStateChangeCallback(ComponentStateChangeCallback callback);
    int addBulkStateChangeCallback(BulkStateChangeCallback callback);
    bool removeCallback(int callbackId);

    // Backward compatibility
    void preserveKeybindings();
    void restoreKeybindings();
    bool isKeybindingPreserved(const std::string& action) const;

    // Keyboard shortcuts for toggles
    bool processKeyboardToggle(int key, bool ctrlPressed = false, bool altPressed = false);
    void registerToggleShortcut(const std::string& componentId, int key, bool ctrl = false, bool alt = false);
    void unregisterToggleShortcut(const std::string& componentId);
    std::string getShortcutForComponent(const std::string& componentId) const;

    // Integration with GUISettings
    void setSettingsManager(GUISettings* settings);
    GUISettings* getSettingsManager() const { return m_settings; }

    // Statistics and debugging
    int getComponentCount() const;
    int getVisibleComponentCount() const;
    int getEnabledComponentCount() const;
    std::map<std::string, int> getComponentStats() const;

    // Validation
    bool validateComponentData() const;
    
    // Default configuration path
    static std::string getDefaultConfigPath();

private:
    // Configuration data
    std::string m_configPath;
    GUISettings* m_settings;

    // Component data structure
    struct ComponentData {
        std::string displayName;
        std::string category;
        bool visible;
        bool enabled;
        int zIndex;
        
        // Keyboard shortcut data
        int shortcutKey;
        bool shortcutCtrl;
        bool shortcutAlt;

        ComponentData() : 
            displayName(""), 
            category("General"), 
            visible(true), 
            enabled(true), 
            zIndex(0),
            shortcutKey(0),
            shortcutCtrl(false),
            shortcutAlt(false) {}
    };

    std::map<std::string, ComponentData> m_components;
    std::map<std::string, std::set<std::string>> m_categories; // category -> componentIds

    // Preserved keybindings for backward compatibility
    std::map<std::string, std::string> m_preservedKeybindings;

    // Event callbacks
    std::vector<ComponentStateChangeCallback> m_componentCallbacks;
    std::vector<BulkStateChangeCallback> m_bulkCallbacks;
    int m_nextCallbackId;

    // Default component definitions
    void initializeDefaultComponents();

    // File I/O helpers
    bool ensureConfigDirectory();
    Json::Value loadJSONFromFile(const std::string& filePath);
    bool saveJSONToFile(const Json::Value& json, const std::string& filePath);

    // Event system helpers
    void notifyComponentStateChange(const std::string& componentId, bool visible, bool enabled);
    void notifyBulkStateChange(const std::string& category, bool visible, bool enabled);

    // Utility functions
    static bool isValidComponentId(const std::string& id);
    static bool isValidCategory(const std::string& category);
    static std::string getCurrentTimestamp();

    // Keyboard event processing
    std::string getComponentIdFromShortcut(int key, bool ctrl, bool alt) const;
};

} // namespace GUI
} // namespace CataclysmBN

#endif // TOGGLE_MANAGER_H