#ifndef SCHEDULE_REMINDER_H
#define SCHEDULE_REMINDER_H

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <esp_timer.h>

/**
 * @brief Schedule item data structure
 * 
 * Represents a single schedule reminder with all necessary information
 * for triggering and managing the reminder.
 */
struct ScheduleItem {
    std::string id;           ///< Unique identifier
    std::string title;        ///< Reminder title
    std::string description;  ///< Detailed description
    time_t trigger_time;      ///< Trigger time (Unix timestamp)
    bool enabled;            ///< Whether the reminder is enabled
    bool recurring;          ///< Whether this is a recurring reminder
    int repeat_interval;     ///< Repeat interval in seconds
    std::string created_at;  ///< Creation timestamp
    
    ScheduleItem() : 
        trigger_time(0), 
        enabled(true), 
        recurring(false), 
        repeat_interval(0) 
    {}
};

/**
 * @brief Schedule reminder error codes
 */
enum class ScheduleError {
    kSuccess = 0,           ///< Operation completed successfully
    kMaxItemsReached,       ///< Maximum number of schedule items reached
    kDuplicateId,           ///< Schedule with this ID already exists
    kInvalidTime,           ///< Invalid trigger time specified
    kStorageError,          ///< Error accessing storage
    kNotFound,              ///< Schedule item not found
    kNotInitialized         ///< Schedule reminder not initialized
};

/**
 * @brief Schedule reminder management class
 * 
 * Provides functionality for managing and triggering schedule reminders
 * using the system's existing notification mechanisms. This class follows
 * the singleton pattern to ensure global consistency.
 */
class ScheduleReminder {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static ScheduleReminder& GetInstance();
    
    /**
     * @brief Initialize the schedule reminder system
     * @return true if initialization successful, false otherwise
     */
    bool Initialize();
    
    /**
     * @brief Shutdown the schedule reminder system
     */
    void Shutdown();
    
    /**
     * @brief Add a new schedule
     * @param item Schedule item to add
     * @return ScheduleError indicating operation result
     */
    ScheduleError AddSchedule(const ScheduleItem& item);
    
    /**
     * @brief Remove a schedule by ID
     * @param id Schedule ID to remove
     * @return ScheduleError indicating operation result
     */
    ScheduleError RemoveSchedule(const std::string& id);
    
    /**
     * @brief Update an existing schedule
     * @param id Schedule ID to update
     * @param item New schedule data
     * @return ScheduleError indicating operation result
     */
    ScheduleError UpdateSchedule(const std::string& id, const ScheduleItem& item);
    
    /**
     * @brief Get all schedules
     * @return Vector of all schedule items
     */
    std::vector<ScheduleItem> GetSchedules() const;
    
    /**
     * @brief Get a specific schedule by ID
     * @param id Schedule ID to retrieve
     * @return Pointer to schedule item if found, nullptr otherwise
     */
    ScheduleItem* GetSchedule(const std::string& id);
    
    /**
     * @brief Check for due schedules and trigger reminders
     */
    void CheckDueSchedules();
    
    /**
     * @brief Set the reminder callback function
     * @param callback Function to call when a reminder is triggered
     */
    void SetReminderCallback(std::function<void(const ScheduleItem&)> callback);
    
private:
    /// Private constructor for singleton pattern
    ScheduleReminder();
    
    /// Destructor
    ~ScheduleReminder();
    
    /// Setup the periodic timer for checking schedules
    void SetupTimer();
    
    /// Load schedules from persistent storage
    void LoadSchedules();
    
    /// Save schedules to persistent storage
    void SaveSchedules();
    
    /// Process a due schedule item
    void ProcessDueSchedule(const ScheduleItem& item);
    
    /// Timer callback function
    static void TimerCallback(void* arg);
    
    std::vector<ScheduleItem> schedules_;                    ///< List of schedule items
    std::function<void(const ScheduleItem&)> reminder_callback_; ///< Reminder callback function
    esp_timer_handle_t check_timer_;                         ///< Timer handle for periodic checks
    bool initialized_;                                       ///< Initialization flag
    mutable std::mutex schedules_mutex_;                     ///< Mutex for thread safety
};

#endif // SCHEDULE_REMINDER_H
