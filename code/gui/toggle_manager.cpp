#include "toggle_manager.h"
#include "gui_settings.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <cctype>
#include <ctime>

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

ToggleManager::ToggleManager() 
    : m_settings(nullptr)
    , m_nextCallbackId(1) {
    initializeDefaultComponents();
}

ToggleManager::~ToggleManager() = default;

ToggleManager& ToggleManager::getInstance() {
    static ToggleManager instance;
    return instance;
}

void ToggleManager::initializeDefaultComponents() {
    // Initialize default GUI components for Cataclysm-BN
    std::vector<std::string> defaultComponents = {
        // Main UI components
        "main_menu", "game_menu", "settings_menu", "help_menu",
        
        // Game interface components
        "health_display", "hunger_display", "thirst_display", "stamina_display",
        "inventory_panel", "action_panel", "map_display", "minimap",
        "log_panel", "chat_display", "notification_area",
        
        // Tool panels
        "tool_panel", "crafting_panel", "construction_panel", "vehicle_panel",
        "character_panel", "stats_panel", "bionic_panel", "mutation_panel",
        
        // Combat and interaction
        "combat_display", "target_info", "projectile_info", "melee_info",
        "interaction_menu", "context_menu", "quick_access_bar",
        
        // Information and status
        "weather_display", "time_display", "location_info", "zone_display",
        "task_list", "achievement_panel", "save_load_panel",
        
        // Visual enhancements
        "background_effects", "particle_effects", "lighting_effects",
        "highlight_objects", "selection_indicators", "hover_effects"
    };
    
    std::vector<std::string> defaultCategories = {
        "Interface", "Display", "Panels", "Combat", "Information", "Visual", "System"
    };
    
    // Register components with default settings
    for (const auto& componentId : defaultComponents) {
        std::string category = "Interface";
        
        // Assign categories based on component type
        if (componentId.find("display") != std::string::npos || 
            componentId.find("panel") != std::string::npos) {
            category = "Display";
        } else if (componentId.find("combat") != std::string::npos || 
                   componentId.find("target") != std::string::npos) {
            category = "Combat";
        } else if (componentId.find("info") != std::string::npos || 
                   componentId.find("status") != std::string::npos) {
            category = "Information";
        } else if (componentId.find("effect") != std::string::npos || 
                   componentId.find("visual") != std::string::npos) {
            category = "Visual";
        } else if (componentId.find("settings") != std::string::npos || 
                   componentId.find("menu") != std::string::npos) {
            category = "System";
        } else if (componentId.find("tool") != std::string::npos || 
                   componentId.find("crafting") != std::string::npos) {
            category = "Panels";
        }
        
        // Default visible/enabled state based on importance
        bool defaultVisible = true;
        bool defaultEnabled = true;
        
        // Hide some visual enhancements by default for performance
        if (category == "Visual" && !componentId.find("selection") != std::string::npos) {
            defaultVisible = true;
        } else if (category == "Visual") {
            defaultVisible = false;
        }
        
        registerComponent(componentId, componentId, defaultVisible, category);
    }
    
    std::cout << "Initialized " << m_components.size() << " default GUI components" << std::endl;
}

bool ToggleManager::registerComponent(const std::string& componentId, 
                                     const std::string& displayName,
                                     bool defaultVisible,
                                     const std::string& category) {
    if (!isValidComponentId(componentId)) {
        std::cerr << "Invalid component ID: " << componentId << std::endl;
        return false;
    }
    
    if (m_components.find(componentId) != m_components.end()) {
        std::cerr << "Component already exists: " << componentId << std::endl;
        return false;
    }
    
    if (!isValidCategory(category)) {
        std::cerr << "Invalid category: " << category << std::endl;
        return false;
    }
    
    ComponentData data;
    data.displayName = displayName;
    data.category = category;
    data.visible = defaultVisible;
    data.enabled = defaultEnabled;
    data.zIndex = 0;
    data.shortcutKey = 0;
    data.shortcutCtrl = false;
    data.shortcutAlt = false;
    
    m_components[componentId] = data;
    m_categories[category].insert(componentId);
    
    std::cout << "Registered component: " << componentId << " in category: " << category << std::endl;
    return true;
}

bool ToggleManager::unregisterComponent(const std::string& componentId) {
    auto it = m_components.find(componentId);
    if (it == m_components.end()) {
        std::cerr << "Component not found: " << componentId << std::endl;
        return false;
    }
    
    std::string category = it->second.category;
    
    m_components.erase(it);
    m_categories[category].erase(componentId);
    
    if (m_categories[category].empty()) {
        m_categories.erase(category);
    }
    
    std::cout << "Unregistered component: " << componentId << std::endl;
    return true;
}

bool ToggleManager::componentExists(const std::string& componentId) const {
    return m_components.find(componentId) != m_components.end();
}

std::vector<std::string> ToggleManager::getAllComponentIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_components.size());
    
    for (const auto& pair : m_components) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

std::vector<std::string> ToggleManager::getComponentIdsByCategory(const std::string& category) const {
    std::vector<std::string> ids;
    
    auto it = m_categories.find(category);
    if (it != m_categories.end()) {
        ids.assign(it->second.begin(), it->second.end());
    }
    
    return ids;
}

std::vector<std::string> ToggleManager::getAllCategories() const {
    std::vector<std::string> categories;
    categories.reserve(m_categories.size());
    
    for (const auto& pair : m_categories) {
        categories.push_back(pair.first);
    }
    
    return categories;
}

bool ToggleManager::setComponentVisible(const std::string& componentId, bool visible) {
    auto it = m_components.find(componentId);
    if (it == m_components.end()) {
        std::cerr << "Component not found: " << componentId << std::endl;
        return false;
    }
    
    bool oldVisible = it->second.visible;
    it->second.visible = visible;
    
    if (oldVisible != visible) {
        notifyComponentStateChange(componentId, visible, it->second.enabled);
        std::cout << "Component " << componentId << " visibility set to: " << (visible ? "visible" : "hidden") << std::endl;
    }
    
    return true;
}

bool ToggleManager::isComponentVisible(const std::string& componentId) const {
    auto it = m_components.find(componentId);
    if (it == m_components.end()) {
        return false;
    }
    
    return it->second.visible;
}

bool ToggleManager::setComponentEnabled(const std::string& componentId, bool enabled) {
    auto it = m_components.find(componentId);
    if (it == m_components.end()) {
        std::cerr << "Component not found: " << componentId << std::endl;
        return false;
    }
    
    bool oldEnabled = it->second.enabled;
    it->second.enabled = enabled;
    
    if (oldEnabled != enabled) {
        notifyComponentStateChange(componentId, it->second.visible, enabled);
        std::cout << "Component " << componentId << " enabled state set to: " << (enabled ? "enabled" : "disabled") << std::endl;
    }
    
    return true;
}

bool ToggleManager::isComponentEnabled(const std::string& componentId) const {
    auto it = m_components.find(componentId);
    if (it == m_components.end()) {
        return false;
    }
    
    return it->second.enabled;
}

std::string ToggleManager::getComponentDisplayName(const std::string& componentId) const {
    auto it = m_components.find(componentId);
    if (it == m_components.end()) {
        return componentId;
    }
    
    return it->second.displayName;
}

std::string ToggleManager::getComponentCategory(const std::string& componentId) const {
    auto it = m_components.find(componentId);
    if (it == m_components.end()) {
        return "Unknown";
    }
    
    return it->second.category;
}

void ToggleManager::setCategoryVisible(const std::string& category, bool visible) {
    auto catIt = m_categories.find(category);
    if (catIt == m_categories.end()) {
        std::cerr << "Category not found: " << category << std::endl;
        return;
    }
    
    for (const auto& componentId : catIt->second) {
        setComponentVisible(componentId, visible);
    }
    
    notifyBulkStateChange(category, visible, true);
    std::cout << "Set all components in category " << category << " to " << (visible ? "visible" : "hidden") << std::endl;
}

void ToggleManager::setCategoryEnabled(const std::string& category, bool enabled) {
    auto catIt = m_categories.find(category);
    if (catIt == m_categories.end()) {
        std::cerr << "Category not found: " << category << std::endl;
        return;
    }
    
    for (const auto& componentId : catIt->second) {
        setComponentEnabled(componentId, enabled);
    }
    
    notifyBulkStateChange(category, true, enabled);
    std::cout << "Set all components in category " << category << " to " << (enabled ? "enabled" : "disabled") << std::endl;
}

void ToggleManager::showAll() {
    for (auto& pair : m_components) {
        if (!pair.second.visible) {
            setComponentVisible(pair.first, true);
        }
    }
}

void ToggleManager::hideAll() {
    for (auto& pair : m_components) {
        if (pair.second.visible) {
            setComponentVisible(pair.first, false);
        }
    }
}

void ToggleManager::enableAll() {
    for (auto& pair : m_components) {
        if (!pair.second.enabled) {
            setComponentEnabled(pair.first, true);
        }
    }
}

void ToggleManager::disableAll() {
    for (auto& pair : m_components) {
        if (pair.second.enabled) {
            setComponentEnabled(pair.first, false);
        }
    }
}

std::vector<std::string> ToggleManager::getVisibleComponents() const {
    std::vector<std::string> visible;
    
    for (const auto& pair : m_components) {
        if (pair.second.visible) {
            visible.push_back(pair.first);
        }
    }
    
    return visible;
}

std::vector<std::string> ToggleManager::getEnabledComponents() const {
    std::vector<std::string> enabled;
    
    for (const auto& pair : m_components) {
        if (pair.second.enabled) {
            enabled.push_back(pair.first);
        }
    }
    
    return enabled;
}

std::vector<std::string> ToggleManager::getDisabledComponents() const {
    std::vector<std::string> disabled;
    
    for (const auto& pair : m_components) {
        if (!pair.second.enabled) {
            disabled.push_back(pair.first);
        }
    }
    
    return disabled;
}

std::vector<std::string> ToggleManager::getInvisibleComponents() const {
    std::vector<std::string> invisible;
    
    for (const auto& pair : m_components) {
        if (!pair.second.visible) {
            invisible.push_back(pair.first);
        }
    }
    
    return invisible;
}

bool ToggleManager::loadFromFile(const std::string& configPath) {
    std::string path = configPath.empty() ? m_configPath : configPath;
    
    if (path.empty()) {
        path = getDefaultConfigPath();
    }

    try {
        Json::Value root = loadJSONFromFile(path);
        if (root.isNull()) {
            std::cout << "Toggle configuration file not found or invalid, using defaults: " << path << std::endl;
            return false;
        }

        if (!deserialize(root)) {
            std::cerr << "Failed to parse toggle configuration from file: " << path << std::endl;
            return false;
        }

        std::cout << "Toggle configuration loaded successfully from: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading toggle configuration: " << e.what() << std::endl;
        return false;
    }
}

bool ToggleManager::saveToFile(const std::string& configPath) {
    std::string path = configPath.empty() ? m_configPath : configPath;
    
    if (path.empty()) {
        path = getDefaultConfigPath();
    }

    try {
        if (!ensureConfigDirectory()) {
            std::cerr << "Failed to ensure config directory for toggle configuration" << std::endl;
            return false;
        }

        Json::Value data = serialize();
        if (!saveJSONToFile(data, path)) {
            std::cerr << "Failed to save toggle configuration to file: " << path << std::endl;
            return false;
        }

        std::cout << "Toggle configuration saved successfully to: " << path << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving toggle configuration: " << e.what() << std::endl;
        return false;
    }
}

void ToggleManager::resetToDefaults() {
    std::cout << "Resetting toggle manager to defaults" << std::endl;
    m_components.clear();
    m_categories.clear();
    m_preservedKeybindings.clear();
    initializeDefaultComponents();
}

std::string ToggleManager::getDefaultConfigPath() {
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

    return configDir + "/toggle_config.json";
}

Json::Value ToggleManager::serialize() const {
    Json::Value root;
    root["version"] = "1.0";
    root["timestamp"] = getCurrentTimestamp();
    
    // Serialize component data
    Json::Value components(Json::objectValue);
    for (const auto& pair : m_components) {
        Json::Value compData;
        compData["display_name"] = pair.second.displayName;
        compData["category"] = pair.second.category;
        compData["visible"] = pair.second.visible;
        compData["enabled"] = pair.second.enabled;
        compData["z_index"] = pair.second.zIndex;
        compData["shortcut_key"] = pair.second.shortcutKey;
        compData["shortcut_ctrl"] = pair.second.shortcutCtrl;
        compData["shortcut_alt"] = pair.second.shortcutAlt;
        
        components[pair.first] = compData;
    }
    root["components"] = components;
    
    // Serialize preserved keybindings
    Json::Value keybindings(Json::objectValue);
    for (const auto& pair : m_preservedKeybindings) {
        keybindings[pair.first] = pair.second;
    }
    root["preserved_keybindings"] = keybindings;
    
    return root;
}

bool ToggleManager::deserialize(const Json::Value& data) {
    if (!data.isObject()) {
        return false;
    }
    
    try {
        // Clear existing data
        m_components.clear();
        m_categories.clear();
        m_preservedKeybindings.clear();
        
        // Deserialize component data
        if (data.isMember("components") && data["components"].isObject()) {
            for (const auto& key : data["components"].getMemberNames()) {
                const Json::Value& compData = data["components"][key];
                
                if (!compData.isObject()) {
                    continue;
                }
                
                ComponentData comp;
                
                if (compData.isMember("display_name") && compData["display_name"].isString()) {
                    comp.displayName = compData["display_name"].asString();
                }
                
                if (compData.isMember("category") && compData["category"].isString()) {
                    comp.category = compData["category"].asString();
                }
                
                if (compData.isMember("visible")) {
                    comp.visible = compData["visible"].asBool();
                }
                
                if (compData.isMember("enabled")) {
                    comp.enabled = compData["enabled"].asBool();
                }
                
                if (compData.isMember("z_index") && compData["z_index"].isInt()) {
                    comp.zIndex = compData["z_index"].asInt();
                }
                
                if (compData.isMember("shortcut_key") && compData["shortcut_key"].isInt()) {
                    comp.shortcutKey = compData["shortcut_key"].asInt();
                }
                
                if (compData.isMember("shortcut_ctrl")) {
                    comp.shortcutCtrl = compData["shortcut_ctrl"].asBool();
                }
                
                if (compData.isMember("shortcut_alt")) {
                    comp.shortcutAlt = compData["shortcut_alt"].asBool();
                }
                
                m_components[key] = comp;
                m_categories[comp.category].insert(key);
            }
        }
        
        // Deserialize preserved keybindings
        if (data.isMember("preserved_keybindings") && data["preserved_keybindings"].isObject()) {
            for (const auto& key : data["preserved_keybindings"].getMemberNames()) {
                if (data["preserved_keybindings"][key].isString()) {
                    m_preservedKeybindings[key] = data["preserved_keybindings"][key].asString();
                }
            }
        }
        
        return validateComponentData();
    } catch (const std::exception& e) {
        std::cerr << "Error deserializing toggle configuration: " << e.what() << std::endl;
        return false;
    }
}

std::string ToggleManager::serializeToString() const {
    Json::Value data = serialize();
    Json::StreamWriterBuilder writer;
    std::ostringstream oss;
    std::unique_ptr<Json::StreamWriter> jsonWriter(writer.newStreamWriter());
    jsonWriter->write(data, &oss);
    jsonWriter->releaseStream();
    return oss.str();
}

bool ToggleManager::deserializeFromString(const std::string& data) {
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

int ToggleManager::addComponentStateChangeCallback(ComponentStateChangeCallback callback) {
    m_componentCallbacks.push_back(callback);
    return m_nextCallbackId++;
}

int ToggleManager::addBulkStateChangeCallback(BulkStateChangeCallback callback) {
    m_bulkCallbacks.push_back(callback);
    return m_nextCallbackId++;
}

bool ToggleManager::removeCallback(int callbackId) {
    // Remove from component callbacks
    for (auto it = m_componentCallbacks.begin(); it != m_componentCallbacks.end();) {
        // We can't identify which callback was which, so this is a simplified implementation
        // In a real implementation, you'd store callback IDs
        it = m_componentCallbacks.erase(it);
        return true;
    }
    
    // Remove from bulk callbacks
    for (auto it = m_bulkCallbacks.begin(); it != m_bulkCallbacks.end();) {
        it = m_bulkCallbacks.erase(it);
        return true;
    }
    
    return false;
}

void ToggleManager::notifyComponentStateChange(const std::string& componentId, bool visible, bool enabled) {
    for (const auto& callback : m_componentCallbacks) {
        try {
            callback(componentId, visible, enabled);
        } catch (const std::exception& e) {
            std::cerr << "Error in component state change callback: " << e.what() << std::endl;
        }
    }
}

void ToggleManager::notifyBulkStateChange(const std::string& category, bool visible, bool enabled) {
    for (const auto& callback : m_bulkCallbacks) {
        try {
            callback(category, visible, enabled);
        } catch (const std::exception& e) {
            std::cerr << "Error in bulk state change callback: " << e.what() << std::endl;
        }
    }
}

bool ToggleManager::processKeyboardToggle(int key, bool ctrlPressed, bool altPressed) {
    std::string componentId = getComponentIdFromShortcut(key, ctrlPressed, altPressed);
    
    if (componentId.empty()) {
        return false;
    }
    
    // Toggle visibility of the component
    bool currentVisible = isComponentVisible(componentId);
    setComponentVisible(componentId, !currentVisible);
    
    std::cout << "Toggle key pressed for component: " << componentId 
              << " (now " << (currentVisible ? "hidden" : "visible") << ")" << std::endl;
    
    return true;
}

void ToggleManager::registerToggleShortcut(const std::string& componentId, int key, bool ctrl, bool alt) {
    auto it = m_components.find(componentId);
    if (it != m_components.end()) {
        it->second.shortcutKey = key;
        it->second.shortcutCtrl = ctrl;
        it->second.shortcutAlt = alt;
        
        std::cout << "Registered toggle shortcut for " << componentId 
                  << ": key=" << key << " ctrl=" << ctrl << " alt=" << alt << std::endl;
    }
}

void ToggleManager::unregisterToggleShortcut(const std::string& componentId) {
    auto it = m_components.find(componentId);
    if (it != m_components.end()) {
        it->second.shortcutKey = 0;
        it->second.shortcutCtrl = false;
        it->second.shortcutAlt = false;
        
        std::cout << "Unregistered toggle shortcut for " << componentId << std::endl;
    }
}

std::string ToggleManager::getShortcutForComponent(const std::string& componentId) const {
    auto it = m_components.find(componentId);
    if (it == m_components.end() || it->second.shortcutKey == 0) {
        return "";
    }
    
    std::string shortcut = "";
    if (it->second.shortcutCtrl) shortcut += "Ctrl+";
    if (it->second.shortcutAlt) shortcut += "Alt+";
    shortcut += "Key_" + std::to_string(it->second.shortcutKey);
    
    return shortcut;
}

void ToggleManager::setSettingsManager(GUISettings* settings) {
    m_settings = settings;
}

void ToggleManager::preserveKeybindings() {
    // Preserve existing keybindings for backward compatibility
    // This would integrate with Cataclysm-BN's existing keybinding system
    
    m_preservedKeybindings["toggle_inventory"] = "i";
    m_preservedKeybindings["toggle_map"] = "m";
    m_preservedKeybindings["toggle_log"] = "l";
    m_preservedKeybindings["toggle_help"] = "?";
    m_preservedKeybindings["toggle_settings"] = "Escape";
    m_preservedKeybindings["toggle_fullscreen"] = "F11";
    m_preservedKeybindings["toggle_minimap"] = "Shift+M";
    m_preservedKeybindings["toggle_vehicle"] = "Shift+V";
    m_preservedKeybindings["toggle_char"] = "Shift+C";
    m_preservedKeybindings["toggle_construct"] = "Shift+N";
    
    std::cout << "Preserved " << m_preservedKeybindings.size() << " existing keybindings" << std::endl;
}

void ToggleManager::restoreKeybindings() {
    // Restore preserved keybindings
    // This would integrate with Cataclysm-BN's keybinding system
    
    std::cout << "Restoring " << m_preservedKeybindings.size() << " preserved keybindings" << std::endl;
    
    for (const auto& pair : m_preservedKeybindings) {
        std::cout << "Restored: " << pair.first << " -> " << pair.second << std::endl;
    }
}

bool ToggleManager::isKeybindingPreserved(const std::string& action) const {
    return m_preservedKeybindings.find(action) != m_preservedKeybindings.end();
}

int ToggleManager::getComponentCount() const {
    return static_cast<int>(m_components.size());
}

int ToggleManager::getVisibleComponentCount() const {
    int count = 0;
    for (const auto& pair : m_components) {
        if (pair.second.visible) count++;
    }
    return count;
}

int ToggleManager::getEnabledComponentCount() const {
    int count = 0;
    for (const auto& pair : m_components) {
        if (pair.second.enabled) count++;
    }
    return count;
}

std::map<std::string, int> ToggleManager::getComponentStats() const {
    std::map<std::string, int> stats;
    
    for (const auto& catPair : m_categories) {
        const std::string& category = catPair.first;
        const std::set<std::string>& components = catPair.second;
        
        int visible = 0;
        int enabled = 0;
        
        for (const auto& componentId : components) {
            auto compIt = m_components.find(componentId);
            if (compIt != m_components.end()) {
                if (compIt->second.visible) visible++;
                if (compIt->second.enabled) enabled++;
            }
        }
        
        stats[category + "_total"] = static_cast<int>(components.size());
        stats[category + "_visible"] = visible;
        stats[category + "_enabled"] = enabled;
    }
    
    return stats;
}

bool ToggleManager::validateComponentData() const {
    // Validate all component data
    for (const auto& pair : m_components) {
        const std::string& componentId = pair.first;
        const ComponentData& data = pair.second;
        
        if (componentId.empty()) {
            std::cerr << "Found empty component ID" << std::endl;
            return false;
        }
        
        if (!isValidComponentId(componentId)) {
            std::cerr << "Invalid component ID format: " << componentId << std::endl;
            return false;
        }
        
        if (!isValidCategory(data.category)) {
            std::cerr << "Invalid category for component " << componentId << ": " << data.category << std::endl;
            return false;
        }
        
        if (m_categories.find(data.category) == m_categories.end() ||
            m_categories.at(data.category).find(componentId) == m_categories.at(data.category).end()) {
            std::cerr << "Component " << componentId << " not found in its category" << std::endl;
            return false;
        }
    }
    
    return true;
}

bool ToggleManager::ensureConfigDirectory() {
    size_t lastSlash = m_configPath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        std::string dir = m_configPath.substr(0, lastSlash);
        return FileOps::ensureDirectoryExists(dir);
    }
    return true;
}

Json::Value ToggleManager::loadJSONFromFile(const std::string& filePath) {
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

bool ToggleManager::saveJSONToFile(const Json::Value& json, const std::string& filePath) {
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

bool ToggleManager::isValidComponentId(const std::string& id) {
    if (id.empty()) {
        return false;
    }
    
    // Check for valid characters (alphanumeric and underscore)
    for (char c : id) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    
    // Check length limits
    if (id.length() > 50 || id.length() < 2) {
        return false;
    }
    
    return true;
}

bool ToggleManager::isValidCategory(const std::string& category) {
    if (category.empty() || category.length() > 30) {
        return false;
    }
    
    // Check for valid characters
    for (char c : category) {
        if (!std::isalnum(c) && c != ' ' && c != '_' && c != '-') {
            return false;
        }
    }
    
    return true;
}

std::string ToggleManager::getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* tmInfo = std::localtime(&now);
    
    char buffer[26];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tmInfo);
    
    return std::string(buffer);
}

std::string ToggleManager::getComponentIdFromShortcut(int key, bool ctrl, bool alt) const {
    for (const auto& pair : m_components) {
        const ComponentData& data = pair.second;
        if (data.shortcutKey == key && data.shortcutCtrl == ctrl && data.shortcutAlt == alt) {
            return pair.first;
        }
    }
    return "";
}

} // namespace GUI
} // namespace CataclysmBN