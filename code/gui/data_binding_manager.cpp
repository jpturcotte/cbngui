#include "data_binding_manager.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace cataclysm {
namespace gui {

DataBindingManager::DataBindingManager()
    : event_adapter_(EventBusAdapter::getInstance()), initialized_(false), 
      update_rate_limit_ms_(16), total_updates_(0), skipped_updates_(0) {
}

DataBindingManager::DataBindingManager(EventBusAdapter& event_adapter)
    : event_adapter_(event_adapter), initialized_(false),
      update_rate_limit_ms_(16), total_updates_(0), skipped_updates_(0) {
}

DataBindingManager::~DataBindingManager() {
    if (initialized_.load()) {
        shutdown();
    }
}

void DataBindingManager::initialize() {
    if (initialized_.load()) {
        return;
    }
    
    initialized_.store(true);
    setupEventSubscriptions();
    
    std::cout << "DataBindingManager initialized with rate limit: " 
              << update_rate_limit_ms_.load() << "ms" << std::endl;
}

void DataBindingManager::shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    initialized_.store(false);
    
    {
        std::lock_guard<std::mutex> lock(bindings_mutex_);
        bindings_.clear();
        binding_index_map_.clear();
    }
    
    cleanupEventSubscriptions();
    
    std::cout << "DataBindingManager shutdown completed" << std::endl;
}

bool DataBindingManager::createBinding(const std::string& binding_id, 
                                      std::shared_ptr<IDataSource> data_source,
                                      DataBindingUpdateCallback update_callback) {
    if (!data_source) {
        std::cerr << "Failed to create binding '" << binding_id << "': invalid data source" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(bindings_mutex_);
    
    // Check if binding already exists
    if (binding_index_map_.find(binding_id) != binding_index_map_.end()) {
        std::cerr << "Failed to create binding '" << binding_id << "': binding already exists" << std::endl;
        return false;
    }
    
    // Create the binding
    DataBinding binding(binding_id, data_source);
    binding.setDirty(true);
    binding.setLastUpdateTimestamp(getCurrentTimestamp());
    
    // Store callback separately (in a real implementation, you'd store it with the binding)
    // For now, we'll invoke the callback directly when updating
    
    size_t index = bindings_.size();
    bindings_.push_back(std::move(binding));
    binding_index_map_[binding_id] = index;
    
    std::cout << "Created data binding: " << binding_id 
              << " (type: " << data_source->getDataType().name() << ")" << std::endl;
    
    return true;
}

bool DataBindingManager::removeBinding(const std::string& binding_id) {
    std::lock_guard<std::mutex> lock(bindings_mutex_);
    
    auto it = binding_index_map_.find(binding_id);
    if (it == binding_index_map_.end()) {
        return false;
    }
    
    size_t index = it->second;
    
    // Remove from vector by swapping with last element (if not the last)
    if (index < bindings_.size() - 1) {
        bindings_[index] = std::move(bindings_.back());
        
        // Update the index map for the moved binding
        const std::string& moved_binding_id = bindings_[index].getBindingId();
        binding_index_map_[moved_binding_id] = index;
    }
    
    // Remove last element
    bindings_.pop_back();
    binding_index_map_.erase(it);
    
    std::cout << "Removed data binding: " << binding_id << std::endl;
    return true;
}

void DataBindingManager::updateDirtyBindings() {
    if (!initialized_.load()) {
        return;
    }
    
    std::vector<DataBinding*> dirty_bindings;
    std::uint64_t current_time = getCurrentTimestamp();
    int rate_limit = update_rate_limit_ms_.load();
    
    {
        std::lock_guard<std::mutex> lock(bindings_mutex_);
        
        for (auto& binding : bindings_) {
            if (binding.needsUpdate()) {
                // Check rate limiting
                if (rate_limit > 0) {
                    std::uint64_t time_since_update = current_time - binding.getLastUpdateTimestamp();
                    if (time_since_update < static_cast<std::uint64_t>(rate_limit)) {
                        skipped_updates_.fetch_add(1);
                        continue;
                    }
                }
                
                dirty_bindings.push_back(&binding);
            }
        }
    }
    
    // Update dirty bindings outside the mutex
    for (DataBinding* binding : dirty_bindings) {
        updateBinding(*binding);
    }
}

bool DataBindingManager::forceUpdateBinding(const std::string& binding_id) {
    std::lock_guard<std::mutex> lock(bindings_mutex_);
    
    auto it = binding_index_map_.find(binding_id);
    if (it == binding_index_map_.end()) {
        return false;
    }
    
    DataBinding& binding = bindings_[it->second];
    updateBinding(binding);
    
    return true;
}

size_t DataBindingManager::getBindingCount() const {
    std::lock_guard<std::mutex> lock(bindings_mutex_);
    return bindings_.size();
}

std::unordered_map<std::string, std::string> DataBindingManager::getBindingStatus() const {
    std::lock_guard<std::mutex> lock(bindings_mutex_);
    
    std::unordered_map<std::string, std::string> status;
    
    for (const auto& binding : bindings_) {
        std::ostringstream oss;
        oss << "dirty=" << (binding.isDirty() ? "true" : "false")
            << ",type=" << binding.getDataSource()->getDataType().name()
            << ",last_update=" << binding.getLastUpdateTimestamp();
        status[binding.getBindingId()] = oss.str();
    }
    
    return status;
}

void DataBindingManager::clearAllBindings() {
    std::lock_guard<std::mutex> lock(bindings_mutex_);
    bindings_.clear();
    binding_index_map_.clear();
    
    std::cout << "Cleared all data bindings" << std::endl;
}

void DataBindingManager::setUpdateRateLimit(int rate_limit_ms) {
    update_rate_limit_ms_.store(rate_limit_ms);
    std::cout << "Set data binding update rate limit to " << rate_limit_ms << "ms" << std::endl;
}

bool DataBindingManager::isEmpty() const {
    return getBindingCount() == 0;
}

void DataBindingManager::setupEventSubscriptions() {
    // Subscribe to UI data binding update events
    event_subscriptions_.push_back(
        event_adapter_.subscribe<UiDataBindingUpdateEvent>(
            [this](const UiDataBindingUpdateEvent& event) {
                std::cout << "Received data binding update event: " << event.getBindingId() << std::endl;
                
                // Force update the specific binding
                forceUpdateBinding(event.getBindingId());
            }
        )
    );
    
    // Subscribe to inventory change events to update related bindings
    event_subscriptions_.push_back(
        event_adapter_.subscribeToInventoryChange(
            [this](const GameplayInventoryChangeEvent& event) {
                std::cout << "Inventory change detected, marking related bindings as dirty" << std::endl;
                
                std::lock_guard<std::mutex> lock(bindings_mutex_);
                for (auto& binding : bindings_) {
                    // Mark bindings as dirty if they might be affected by inventory changes
                    if (binding.getDataSource()->getName().find("inventory") != std::string::npos ||
                        binding.getBindingId().find("inventory") != std::string::npos) {
                        binding.setDirty(true);
                    }
                }
            }
        )
    );
    
    // Subscribe to status change events
    event_subscriptions_.push_back(
        event_adapter_.subscribeToStatusChange(
            [this](const GameplayStatusChangeEvent& event) {
                std::cout << "Status change detected: " << event.getStatusType() 
                          << ", marking related bindings as dirty" << std::endl;
                
                std::lock_guard<std::mutex> lock(bindings_mutex_);
                for (auto& binding : bindings_) {
                    // Mark bindings as dirty if they might be affected by status changes
                    if (binding.getDataSource()->getName().find("status") != std::string::npos ||
                        binding.getBindingId().find(event.getStatusType()) != std::string::npos) {
                        binding.setDirty(true);
                    }
                }
            }
        )
    );
}

void DataBindingManager::cleanupEventSubscriptions() {
    for (auto& subscription : event_subscriptions_) {
        if (subscription) {
            subscription->unsubscribe();
        }
    }
    event_subscriptions_.clear();
}

void DataBindingManager::updateBinding(DataBinding& binding) {
    auto data_source = binding.getDataSource();
    if (!data_source) {
        return;
    }
    
    // Check if data has actually changed
    if (!data_source->hasChanged() && !binding.isDirty()) {
        return;
    }
    
    try {
        // Get the data
        std::any data = data_source->getData();
        
        // Update the binding timestamp
        binding.setLastUpdateTimestamp(getCurrentTimestamp());
        binding.setDirty(false);
        
        total_updates_.fetch_add(1);
        
        // In a real implementation, you would invoke the stored callback here
        // For now, we just log the update
        std::cout << "Updated binding: " << binding.getBindingId() 
                  << " (data type: " << data_source->getDataType().name() << ")" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error updating binding '" << binding.getBindingId() << "': " 
                  << e.what() << std::endl;
        binding.setDirty(true); // Mark as dirty for retry
    }
}

std::uint64_t DataBindingManager::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

} // namespace gui
} // namespace cataclysm