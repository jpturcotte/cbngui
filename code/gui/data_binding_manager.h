#ifndef GUI_DATA_BINDING_MANAGER_H
#define GUI_DATA_BINDING_MANAGER_H

#include "event_bus.h"
#include "event_bus_adapter.h"
#include "events.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <typeindex>
#include <any>

namespace cataclysm {
namespace gui {

/**
 * Interface for data sources that can provide data for binding.
 * Implementations should provide type-safe access to data values.
 */
class IDataSource {
public:
    virtual ~IDataSource() = default;
    
    /**
     * Get the type of data this source provides.
     * @return std::type_index representing the data type
     */
    virtual std::type_index getDataType() const = 0;
    
    /**
     * Get a string representation of the data source name.
     * @return Human-readable name for debugging
     */
    virtual std::string getName() const = 0;
    
    /**
     * Check if the data has changed since the last read.
     * @return true if data has changed
     */
    virtual bool hasChanged() const = 0;
    
    /**
     * Get the current data as an any object.
     * @return Current data as std::any
     */
    virtual std::any getData() const = 0;
    
    /**
     * Force a data refresh (useful for polling-based data sources).
     */
    virtual void refresh() {}
};

/**
 * Template implementation of IDataSource for type-safe data access.
 */
template<typename T>
class TypedDataSource : public IDataSource {
public:
    explicit TypedDataSource(std::string name, std::function<T()> data_provider)
        : name_(std::move(name)), data_provider_(std::move(data_provider)), 
          last_value_(), has_value_(false) {}
    
    std::type_index getDataType() const override {
        return std::type_index(typeid(T));
    }
    
    std::string getName() const override {
        return name_;
    }
    
    bool hasChanged() const override {
        T current_value = data_provider_();
        if (!has_value_) {
            return true;
        }
        return current_value != last_value_;
    }
    
    std::any getData() const override {
        return data_provider_();
    }
    
    void refresh() override {
        // Force a data refresh for polling-based sources
        // This is a no-op for function-based sources but can be overridden
    }
    
    /**
     * Get the current value directly (type-safe).
     * @return Current value
     */
    T getValue() const {
        return data_provider_();
    }
    
private:
    std::string name_;
    std::function<T()> data_provider_;
    mutable T last_value_;
    mutable bool has_value_;
};

/**
 * Data binding between a GUI element and a data source.
 * Handles automatic updates when the source data changes.
 */
class DataBinding {
public:
    DataBinding() = default;
    
    DataBinding(std::string binding_id, std::shared_ptr<IDataSource> data_source)
        : binding_id_(std::move(binding_id)), data_source_(std::move(data_source)),
          dirty_(true), last_update_timestamp_(0) {}
    
    // Delete copy operations, allow move
    DataBinding(const DataBinding&) = delete;
    DataBinding& operator=(const DataBinding&) = delete;
    
    DataBinding(DataBinding&&) = default;
    DataBinding& operator=(DataBinding&&) = default;
    
    const std::string& getBindingId() const { return binding_id_; }
    std::shared_ptr<IDataSource> getDataSource() const { return data_source_; }
    
    bool isDirty() const { return dirty_.load(); }
    void setDirty(bool dirty) { dirty_.store(dirty); }
    
    std::uint64_t getLastUpdateTimestamp() const { return last_update_timestamp_; }
    void setLastUpdateTimestamp(std::uint64_t timestamp) { last_update_timestamp_ = timestamp; }
    
    /**
     * Check if this binding should be updated.
     * @return true if the binding needs updating
     */
    bool needsUpdate() const {
        return dirty_.load() || (data_source_ && data_source_->hasChanged());
    }

private:
    std::string binding_id_;
    std::shared_ptr<IDataSource> data_source_;
    std::atomic<bool> dirty_;
    std::uint64_t last_update_timestamp_;
};

/**
 * Callback for data binding updates.
 * Called when bound data changes and UI needs to be updated.
 */
using DataBindingUpdateCallback = std::function<void(const std::string& binding_id, std::any data)>;

/**
 * Manager for data bindings between GUI components and data sources.
 * Handles automatic updates through the event system and provides
 * efficient data binding management for real-time UI updates.
 */
class DataBindingManager {
public:
    DataBindingManager();
    explicit DataBindingManager(EventBusAdapter& event_adapter);
    ~DataBindingManager();
    
    // Delete copy and move operations
    DataBindingManager(const DataBindingManager&) = delete;
    DataBindingManager& operator=(const DataBindingManager&) = delete;
    DataBindingManager(DataBindingManager&&) = delete;
    DataBindingManager& operator=(DataBindingManager&&) = delete;
    
    /**
     * Initialize the data binding manager.
     */
    void initialize();
    
    /**
     * Shutdown the data binding manager.
     */
    void shutdown();
    
    /**
     * Create a new data binding.
     * @param binding_id Unique identifier for the binding
     * @param data_source Data source to bind to
     * @param update_callback Callback to invoke when data changes
     * @return true if binding was created successfully
     */
    bool createBinding(const std::string& binding_id, 
                      std::shared_ptr<IDataSource> data_source,
                      DataBindingUpdateCallback update_callback);
    
    /**
     * Remove a data binding.
     * @param binding_id ID of the binding to remove
     * @return true if binding was found and removed
     */
    bool removeBinding(const std::string& binding_id);
    
    /**
     * Update all dirty bindings.
     * This should be called during the UI update cycle.
     */
    void updateDirtyBindings();
    
    /**
     * Force update of a specific binding.
     * @param binding_id ID of the binding to update
     * @return true if binding was found and updated
     */
    bool forceUpdateBinding(const std::string& binding_id);
    
    /**
     * Get the number of active bindings.
     * @return Number of active data bindings
     */
    size_t getBindingCount() const;
    
    /**
     * Get binding information for debugging.
     * @return Map of binding IDs to their status
     */
    std::unordered_map<std::string, std::string> getBindingStatus() const;
    
    /**
     * Clear all bindings.
     */
    void clearAllBindings();
    
    /**
     * Set the update rate limit (minimum time between updates for the same binding).
     * @param rate_limit_ms Rate limit in milliseconds
     */
    void setUpdateRateLimit(int rate_limit_ms);
    
    /**
     * Check if the manager has any active bindings.
     * @return true if no active bindings exist
     */
    bool isEmpty() const;

private:
    void setupEventSubscriptions();
    void cleanupEventSubscriptions();
    void updateBinding(DataBinding& binding);
    std::uint64_t getCurrentTimestamp() const;
    
    EventBusAdapter& event_adapter_;
    std::vector<DataBinding> bindings_;
    std::unordered_map<std::string, size_t> binding_index_map_;
    mutable std::mutex bindings_mutex_;
    std::vector<std::shared_ptr<EventSubscription>> event_subscriptions_;
    std::atomic<bool> initialized_;
    std::atomic<int> update_rate_limit_ms_;
    std::atomic<uint64_t> total_updates_;
    std::atomic<uint64_t> skipped_updates_;
};

/**
 * Helper class for creating typed data sources easily.
 */
class DataSourceBuilder {
public:
    /**
     * Create a typed data source with automatic type deduction.
     * @tparam T Type of data
     * @param name Name for the data source
     * @param data_provider Function that provides the data
     * @return Shared pointer to the data source
     */
    template<typename T>
    static std::shared_ptr<IDataSource> create(const std::string& name, std::function<T()> data_provider) {
        return std::make_shared<TypedDataSource<T>>(name, std::move(data_provider));
    }
    
    /**
     * Create a simple value-based data source.
     * @tparam T Type of data
     * @param name Name for the data source
     * @param value Current value (will be captured by reference)
     * @return Shared pointer to the data source
     */
    template<typename T>
    static std::shared_ptr<IDataSource> createValueSource(const std::string& name, T& value) {
        return create<T>(name, [&value]() -> T { return value; });
    }
};

} // namespace gui
} // namespace cataclysm

#endif // GUI_DATA_BINDING_MANAGER_H