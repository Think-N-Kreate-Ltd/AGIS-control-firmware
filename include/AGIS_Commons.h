#ifndef D8C84950_1B28_4594_B461_3D52C4A78A6B
#define D8C84950_1B28_4594_B461_3D52C4A78A6B

#include <AGIS_Types.h>

extern volatile unsigned int numDrops;
extern volatile unsigned int dripRate;
extern volatile unsigned int infusedVolume_x100;
extern volatile unsigned long infusedTime;
extern unsigned int dropFactor;

extern unsigned int targetVTBI;
extern unsigned int targetTotalTime;
extern unsigned int targetDripRate;
extern unsigned int targetNumDrops;
extern infusionState_t infusionState;

extern volatile float current_mA;
extern volatile float busvoltage;
extern volatile float shuntvoltage;
extern volatile float power_mW;
extern volatile float avgCurrent_mA;

#endif /* D8C84950_1B28_4594_B461_3D52C4A78A6B */
