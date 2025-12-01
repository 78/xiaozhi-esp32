# Features CMakeLists.txt
# This file includes all feature modules

# Schedule Reminder Feature
if(CONFIG_ENABLE_SCHEDULE_REMINDER)
    set(SCHEDULE_REMINDER_SRCS
        "schedule_reminder/schedule_reminder.cc"
        "schedule_reminder/schedule_manager.cc"
    )
    
    set(SCHEDULE_REMINDER_INCLUDES
        "schedule_reminder"
    )
    
    list(APPEND COMPONENT_SRCS ${SCHEDULE_REMINDER_SRCS})
    list(APPEND COMPONENT_PRIV_INCLUDES ${SCHEDULE_REMINDER_INCLUDES})
endif()
