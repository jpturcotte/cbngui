#ifndef GUI_SETTINGS_H
#define GUI_SETTINGS_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include "json.h"

namespace CataclysmBN {
namespace GUI {

// Forward declarations
class ToggleManager;

/**
 * GUI Settings class that manages all GUI configuration options
 * Provides persistent storage and runtime adjustments for UI appearance
 */
class GUISettings {
public:
    // Constructor and Destructor
    GUISettings();
    ~GUISettings();

    // Copy and assignment operators
    GUISettings(const GUISettings&) = delete;
    GUISettings& operator=(const GUISettings&) = delete;

    // Singleton access
    static GUISettings& getInstance();

    // Initialization and management
    bool loadFromFile(const std::string& configPath = "");
    bool saveToFile(const std::string& configPath = "");
    void resetToDefaults();
    void setConfigPath(const std::string& path) { m_configPath = path; }
    std::string getConfigPath() const { return m_configPath; }

    // GUI Density and Appearance Settings
    enum class UIDensity {
        COMPACT = 0,
        COMFORTABLE = 1,
        SPACIOUS = 2
    };

    enum class UITheme {
        DEFAULT = 0,
        DARK = 1,
        HIGH_CONTRAST = 2,
        ACCESSIBILITY = 3
    };

    // Getters and Setters for UI properties
    UIDensity getUIDensity() const { return m_uiDensity; }
    void setUIDensity(UIDensity density) { m_uiDensity = density; onUISettingsChanged(); }

    UITheme getUITheme() const { return m_uiTheme; }
    void setUITheme(UITheme theme) { m_uiTheme = theme; onUISettingsChanged(); }

    // Font and text settings
    int getFontSize() const { return m_fontSize; }
    void setFontSize(int size) { m_fontSize = size; onUISettingsChanged(); }

    std::string getFontFamily() const { return m_fontFamily; }
    void setFontFamily(const std::string& family) { m_fontFamily = family; onUISettingsChanged(); }

    // Window and layout settings
    int getWindowScale() const { return m_windowScale; }
    void setWindowScale(int scale) { m_windowScale = scale; onUISettingsChanged(); }

    int getSidebarWidth() const { return m_sidebarWidth; }
    void setSidebarWidth(int width) { m_sidebarWidth = width; onUISettingsChanged(); }

    int getButtonHeight() const { return m_buttonHeight; }
    void setButtonHeight(int height) { m_buttonHeight = height; onUISettingsChanged(); }

    // Animation and visual effects
    bool getAnimationsEnabled() const { return m_animationsEnabled; }
    void setAnimationsEnabled(bool enabled) { m_animationsEnabled = enabled; onUISettingsChanged(); }

    int getAnimationSpeed() const { return m_animationSpeed; }
    void setAnimationSpeed(int speed) { m_animationSpeed = speed; onUISettingsChanged(); }

    // Accessibility settings
    bool getHighContrast() const { return m_highContrast; }
    void setHighContrast(bool enabled) { m_highContrast = enabled; onUISettingsChanged(); }

    bool getReducedMotion() const { return m_reducedMotion; }
    void setReducedMotion(bool enabled) { m_reducedMotion = enabled; onUISettingsChanged(); }

    // Event handling
    void onUISettingsChanged(); // Called when any UI setting changes

    // Serialization
    Json::Value serialize() const;
    bool deserialize(const Json::Value& data);
    std::string serializeToString() const;
    bool deserializeFromString(const std::string& data);

    // Default configuration path
    static std::string getDefaultConfigPath();

    // Validation
    bool validateSettings() const;
    void applySettings(); // Apply settings to the current UI system

private:
    // Configuration data
    std::string m_configPath;
    UIDensity m_uiDensity;
    UITheme m_uiTheme;
    int m_fontSize;
    std::string m_fontFamily;
    int m_windowScale;
    int m_sidebarWidth;
    int m_buttonHeight;
    bool m_animationsEnabled;
    int m_animationSpeed;
    bool m_highContrast;
    bool m_reducedMotion;

    // Default values
    void setDefaultValues();
    
    // File I/O helpers
    bool ensureConfigDirectory();
    Json::Value loadJSONFromFile(const std::string& filePath);
    bool saveJSONToFile(const Json::Value& json, const std::string& filePath);

    // Validation helpers
    static bool isValidUIDensity(int value);
    static bool isValidUITheme(int value);
    static bool isValidFontSize(int size);
    static bool isValidWindowScale(int scale);
};

/**
 * Structure to hold individual GUI component settings
 */
struct GUIComponentSettings {
    std::string componentId;
    bool visible;
    bool enabled;
    int zIndex;
    std::string position;
    std::map<std::string, std::string> customProperties;

    Json::Value serialize() const;
    bool deserialize(const Json::Value& data);
};

} // namespace GUI
} // namespace CataclysmBN

#endif // GUI_SETTINGS_H