#ifndef EA655F7D_5387_4B55_B11C_DB866F12DCC6
#define EA655F7D_5387_4B55_B11C_DB866F12DCC6

enum class motorState_t { UP, DOWN, OFF };
enum class buttonState_t { UP, DOWN, ENTER, IDLE };

// NOTE: when infusionState_t type is modified, update the same type in script.js 
enum class infusionState_t {
  NOT_STARTED,           // when the board is powered on and no drops detected
  STARTED,               // as soon as drops are detected
  IN_PROGRESS,           // when infusion is started by user and not completed yet
  ALARM_COMPLETED,       // when infusion has completed, i.e. infusedVolume reaches the target volume
  ALARM_STOPPED,         // when infusion stopped unexpectly, it's likely to have a problem
  ALARM_VOLUME_EXCEEDED  // when infusion has completed but we still detect drops
  // add more states here when needed
};

/**
 * Handler to the infusion monitoring data.
 * Data elements are packed as a struct s.t. it can be called from the timer callback.
 * Struct elements are pointers to actual data.
 */
typedef struct _infusion_monitoring_data_handle_t {
  volatile unsigned int * numDrops_p;
  volatile unsigned int * dripRate_p;
  volatile float * infusedVolume_p;
  volatile unsigned long * infusedTime_p;
  infusionState_t * infusionState_p;
} infusion_monitoring_data_handle_t;

#endif /* EA655F7D_5387_4B55_B11C_DB866F12DCC6 */
