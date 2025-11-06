#include "gui_settings.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace CataclysmBN {
namespace GUI {

// Helper namespace for file operations
namespace FileOps {
    bool ensureDirectoryExists(const std::string& path) {
        try {
            if (!std::filesystem::exists(path)) {
                return std::filesystem::create_directories(path);
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error creating directory " << path << ": " << e.what() << std::endl;
            return false;
        }
    }

    bool fileExists(const std::string& path) {
        try {
            return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
        } catch (const std::exception& e) {
            std::cerr << "Error checking file existence " << path << ": " << e.what() << std::endl;
            return false;
        }
    }
}

GUISettings::GUISettings() {
    setDefaultValues();
    m_configPath = getDefaultConfigPath();
}

GUISettings::~GUISettings() = default;

GUISettings& GUISettings::getInstance() {
    static GUISettings instance;
    return instance;
}

void GUISettings::setDefaultValues() {
    m_uiDensity = UIDensity::COMFORTABLE;
    m_uiTheme = UITheme::DEFAULT;
    m_fontSize = 14;
    m_fontFamily = "Arial";
    m_windowScale = 100;
    m_sidebarWidth = 300;
    m_buttonHeight = 32;
    m_animationsEnabled = true;
    m_animationSpeed = 1;
    m_highContrast = false;
    m_reducedMotion = false;
}

bool GUISettings::loadFromFile(const std::string& configPath) {
    std::string path = configPath.empty() ? m_configPath : configPath;
    
    if (path.empty()) {
        path = getDefaultConfigPath();
    }

    try {
        Json::Value root = loadJSONFromFile(path);
        if (root.isNull()) {
            // File doesn't exist or is invalid, keep defaults
            std::cout << "GUI settings file not found or invalid, using defaults: " << path << std::endl;
            return false;
        }

        if (!deserialize(root)) {
            std::cerr << "Failed to parse GUI settings from file: " << path << std::endl;
            return false;
        }

        std::cout << "GUI settings loaded successfully from: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading GUI settings: " << e.what() << std::endl;
        return false;
    }
}

bool GUISettings::saveToFile(const std::string& configPath) {
    std::string path = configPath.empty() ? m_configPath : configPath;
    
    if (path.empty()) {
        path = getDefaultConfigPath();
    }

    try {
        if (!ensureConfigDirectory()) {
            std::cerr << "Failed to ensure config directory for GUI settings" << std::endl;
            return false;
        }

        Json::Value data = serialize();
        if (!saveJSONToFile(data, path)) {
            std::cerr << "Failed to save GUI settings to file: " << path << std::endl;
            return false;
        }

        std::cout << "GUI settings saved successfully to: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving GUI settings: " << e.what() << std::endl;
        return false;
    }
}

void GUISettings::resetToDefaults() {
    std::cout << "Resetting GUI settings to defaults" << std::endl;
    setDefaultValues();
    applySettings();
}

std::string GUISettings::getDefaultConfigPath() {
    // Use platform-appropriate config directory
    std::string configDir;
    
    #ifdef _WIN32
        const char* appData = std::getenv("APPDATA");
        if (appData) {
            configDir = std::string(appData) + "\\CataclysmBN";
        } else {
            configDir = ".";
        }
    #elif defined(__linux__) || defined(__unix__)
        const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
        if (xdgConfig) {
            configDir = std::string(xdgConfig) + "/cataclysm-bn";
        } else {
            configDir = std::string(std::getenv("HOME")) + "/.config/cataclysm-bn";
        }
    #elif defined(__APPLE__)
        std::string home = std::string(std::getenv("HOME"));
        configDir = home + "/Library/Application Support/CataclysmBN";
    #else
        configDir = ".";
    #endif

    return configDir + "/gui_settings.json";
}

void GUISettings::onUISettingsChanged() {
    std::cout << "UI settings changed, applying changes..." << std::endl;
    applySettings();
}

void GUISettings::applySettings() {
    // Apply settings to the UI system
    // This would integrate with the actual Cataclysm-BN UI system
    
    // For demonstration, just output what would be applied
    std::cout << "Applying GUI settings:" << std::endl;
    std::cout << "  UI Density: " << static_cast<int>(m_uiDensity) << std::endl;
    std::cout << "  UI Theme: " << static_cast<int>(m_uiTheme) << std::endl;
    std::cout << "  Font Size: " << m_fontSize << std::endl;
    std::cout << "  Font Family: " << m_fontFamily << std::endl;
    std::cout << "  Window Scale: " << m_windowScale << "%" << std::endl;
    std::cout << "  Sidebar Width: " << m_sidebarWidth << std::endl;
    std::cout << "  Button Height: " << m_buttonHeight << std::endl;
    std::cout << "  Animations: " << (m_animationsEnabled ? "Enabled" : "Disabled") << std::endl;
    std::cout << "  Animation Speed: " << m_animationSpeed << std::endl;
    std::cout << "  High Contrast: " << (m_highContrast ? "Enabled" : "Disabled") << std::endl;
    std::cout << "  Reduced Motion: " << (m_reducedMotion ? "Enabled" : "Disabled") << std::endl;
}

Json::Value GUISettings::serialize() const {
    Json::Value root;
    
    root["version"] = "1.0";
    root["ui_density"] = static_cast<int>(m_uiDensity);
    root["ui_theme"] = static_cast<int>(m_uiTheme);
    root["font_size"] = m_fontSize;
    root["font_family"] = m_fontFamily;
    root["window_scale"] = m_windowScale;
    root["sidebar_width"] = m_sidebarWidth;
    root["button_height"] = m_buttonHeight;
    root["animations_enabled"] = m_animationsEnabled;
    root["animation_speed"] = m_animationSpeed;
    root["high_contrast"] = m_highContrast;
    root["reduced_motion"] = m_reducedMotion;
    
    return root;
}

bool GUISettings::deserialize(const Json::Value& data) {
    if (!data.isObject()) {
        return false;
    }
    
    try {
        if (data.isMember("ui_density") && data["ui_density"].isInt()) {
            int density = data["ui_density"].asInt();
            if (isValidUIDensity(density)) {
                m_uiDensity = static_cast<UIDensity>(density);
            }
        }
        
        if (data.isMember("ui_theme") && data["ui_theme"].isInt()) {
            int theme = data["ui_theme"].asInt();
            if (isValidUITheme(theme)) {
                m_uiTheme = static_cast<UITheme>(theme);
            }
        }
        
        if (data.isMember("font_size") && data["font_size"].isInt()) {
            int size = data["font_size"].asInt();
            if (isValidFontSize(size)) {
                m_fontSize = size;
            }
        }
        
        if (data.isMember("font_family") && data["font_family"].isString()) {
            m_fontFamily = data["font_family"].asString();
        }
        
        if (data.isMember("window_scale") && data["window_scale"].isInt()) {
            int scale = data["window_scale"].asInt();
            if (isValidWindowScale(scale)) {
                m_windowScale = scale;
            }
        }
        
        if (data.isMember("sidebar_width") && data["sidebar_width"].isInt()) {
            m_sidebarWidth = data["sidebar_width"].asInt();
        }
        
        if (data.isMember("button_height") && data["button_height"].isInt()) {
            m_buttonHeight = data["button_height"].asInt();
        }
        
        if (data.isMember("animations_enabled")) {
            m_animationsEnabled = data["animations_enabled"].asBool();
        }
        
        if (data.isMember("animation_speed") && data["animation_speed"].isInt()) {
            m_animationSpeed = data["animation_speed"].asInt();
        }
        
        if (data.isMember("high_contrast")) {
            m_highContrast = data["high_contrast"].asBool();
        }
        
        if (data.isMember("reduced_motion")) {
            m_reducedMotion = data["reduced_motion"].asBool();
        }
        
        return validateSettings();
    } catch (const std::exception& e) {
        std::cerr << "Error deserializing GUI settings: " << e.what() << std::endl;
        return false;
    }
}

std::string GUISettings::serializeToString() const {
    Json::Value data = serialize();
    Json::StreamWriterBuilder writer;
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> jsonWriter(writer.newStreamWriter());
    jsonWriter->write(data, &oss);
    jsonWriter->releaseStream();
    return oss.str();
}

bool GUISettings::deserializeFromString(const std::string& data) {
    try {
        Json::Value root;
        Json::CharReaderBuilder reader;
        std::istringstream iss(data);
        std::string errs;
        
        if (!Json::parseFromStream(reader, iss, &root, &errs)) {
            std::cerr << "Failed to parse JSON: " << errs << std::endl;
            return false;
        }
        
        return deserialize(root);
    } catch (const std::exception& e) {
        std::cerr << "Error deserializing from string: " << e.what() << std::endl;
        return false;
    }
}

bool GUISettings::validateSettings() const {
    if (!isValidUIDensity(static_cast<int>(m_uiDensity))) {
        std::cerr << "Invalid UI density setting" << std::endl;
        return false;
    }
    
    if (!isValidUITheme(static_cast<int>(m_uiTheme))) {
        std::cerr << "Invalid UI theme setting" << std::endl;
        return false;
    }
    
    if (!isValidFontSize(m_fontSize)) {
        std::cerr << "Invalid font size: " << m_fontSize << std::endl;
        return false;
    }
    
    if (!isValidWindowScale(m_windowScale)) {
        std::cerr << "Invalid window scale: " << m_windowScale << std::endl;
        return false;
    }
    
    if (m_animationSpeed < 0 || m_animationSpeed > 5) {
        std::cerr << "Invalid animation speed: " << m_animationSpeed << std::endl;
        return false;
    }
    
    if (m_buttonHeight < 16 || m_buttonHeight > 64) {
        std::cerr << "Invalid button height: " << m_buttonHeight << std::endl;
        return false;
    }
    
    if (m_sidebarWidth < 200 || m_sidebarWidth > 600) {
        std::cerr << "Invalid sidebar width: " << m_sidebarWidth << std::endl;
        return false;
    }
    
    return true;
}

bool GUISettings::isValidUIDensity(int value) {
    return value >= 0 && value <= 2;
}

bool GUISettings::isValidUITheme(int value) {
    return value >= 0 && value <= 3;
}

bool GUISettings::isValidFontSize(int size) {
    return size >= 8 && size <= 32;
}

bool GUISettings::isValidWindowScale(int scale) {
    return scale >= 50 && scale <= 200;
}

bool GUISettings::ensureConfigDirectory() {
    size_t lastSlash = m_configPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        std::string dir = m_configPath.substr(0, lastSlash);
        return FileOps::ensureDirectoryExists(dir);
    }
    return true;
}

Json::Value GUISettings::loadJSONFromFile(const std::string& filePath) {
    if (!FileOps::fileExists(filePath)) {
        return Json::Value::null;
    }
    
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return Json::Value::null;
        }
        
        Json::Value root;
        Json::CharReaderBuilder reader;
        std::string errs;
        
        if (!Json::parseFromStream(reader, file, &root, &errs)) {
            std::cerr << "Error parsing JSON: " << errs << std::endl;
            return Json::Value::null;
        }
        
        file.close();
        return root;
    } catch (const std::exception& e) {
        std::cerr << "Error loading JSON file: " << e.what() << std::endl;
        return Json::Value::null;
    }
}

bool GUISettings::saveJSONToFile(const Json::Value& json, const std::string& filePath) {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        
        Json::StreamWriterBuilder writer;
        std::unique_ptr<Json::StreamWriter> jsonWriter(writer.newStreamWriter());
        jsonWriter->write(json, &file);
        jsonWriter->releaseStream();
        
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving JSON file: " << e.what() << std::endl;
        return false;
    }
}

// GUIComponentSettings implementation
Json::Value GUIComponentSettings::serialize() const {
    Json::Value root;
    root["component_id"] = componentId;
    root["visible"] = visible;
    root["enabled"] = enabled;
    root["z_index"] = zIndex;
    root["position"] = position;
    
    Json::Value props(Json::objectValue);
    for (const auto& prop : customProperties) {
        props[prop.first] = prop.second;
    }
    root["custom_properties"] = props;
    
    return root;
}

bool GUIComponentSettings::deserialize(const Json::Value& data) {
    if (!data.isObject()) {
        return false;
    }
    
    try {
        if (data.isMember("component_id") && data["component_id"].isString()) {
            componentId = data["component_id"].asString();
        }
        
        if (data.isMember("visible")) {
            visible = data["visible"].asBool();
        }
        
        if (data.isMember("enabled")) {
            enabled = data["enabled"].asBool();
        }
        
        if (data.isMember("z_index") && data["z_index"].isInt()) {
            zIndex = data["z_index"].asInt();
        }
        
        if (data.isMember("position") && data["position"].isString()) {
            position = data["position"].asString();
        }
        
        if (data.isMember("custom_properties") && data["custom_properties"].isObject()) {
            customProperties.clear();
            for (const auto& key : data["custom_properties"].getMemberNames()) {
                if (data["custom_properties"][key].isString()) {
                    customProperties[key] = data["custom_properties"][key].asString();
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error deserializing GUI component settings: " << e.what() << std::endl;
        return false;
    }
}

} // namespace GUI
} // namespace CataclysmBN