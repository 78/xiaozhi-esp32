#ifndef DEVICE_STATE_MACHINE_H
#define DEVICE_STATE_MACHINE_H

#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

#include "device_state.h"

/**
 * DeviceStateMachine - Manages device state transitions with validation
 * 
 * This class ensures strict state transition rules and provides a callback mechanism
 * for components to react to state changes.
 */
class DeviceStateMachine {
public:
    DeviceStateMachine();
    ~DeviceStateMachine() = default;

    // Delete copy constructor and assignment operator
    DeviceStateMachine(const DeviceStateMachine&) = delete;
    DeviceStateMachine& operator=(const DeviceStateMachine&) = delete;

    /**
     * Get the current device state
     */
    DeviceState GetState() const { return current_state_.load(); }

    /**
     * Attempt to transition to a new state
     * @param new_state The target state
     * @return true if transition was successful, false if invalid transition
     */
    bool TransitionTo(DeviceState new_state);

    /**
     * Check if transition to target state is valid from current state
     */
    bool CanTransitionTo(DeviceState target) const;

    /**
     * State change callback type
     * Parameters: old_state, new_state
     */
    using StateCallback = std::function<void(DeviceState, DeviceState)>;

    /**
     * Add a state change listener (observer pattern)
     * Callback is invoked in the context of the caller of TransitionTo()
     * @return listener id for removal
     */
    int AddStateChangeListener(StateCallback callback);

    /**
     * Remove a state change listener by id
     */
    void RemoveStateChangeListener(int listener_id);

    /**
     * Get state name string for logging
     */
    static const char* GetStateName(DeviceState state);

private:
    std::atomic<DeviceState> current_state_{kDeviceStateUnknown};
    std::vector<std::pair<int, StateCallback>> listeners_;
    int next_listener_id_{0};
    std::mutex mutex_;

    /**
     * Check if transition from source to target is valid
     */
    bool IsValidTransition(DeviceState from, DeviceState to) const;

    /**
     * Notify callback of state change
     */
    void NotifyStateChange(DeviceState old_state, DeviceState new_state);
};

#endif // DEVICE_STATE_MACHINE_H
