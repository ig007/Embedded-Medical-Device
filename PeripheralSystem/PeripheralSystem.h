#ifndef PERIPHERALSYSTEM_H
#define PERIPHERALSYSTEM_H

#include "System.h"

#define PULS_IN_PIN (2) // Digital
#define RESP_IN_PIN (3) // Digital
#define TEMP_IN_PIN (0) // Analog
#define PRES_LED_PIN (6) // Digital
#define PRES_SW_PIN (7) // Digital
#define PRES_INC_PIN (8) // Digital
#define PRES_MEAS_PIN (9) // Digital
#define EKG_IN_PIN (1) // Analog

// For measureFlags
#define SYST_READY (1 << 0)
#define DIAS_READY (1 << 1)
#define EKG_MEASURE (1 << 2)
#define EKG_READY (1 << 3)

#define EKG_PERIOD (19)
#define RECORD_PERIOD (3)
#define PULS_NUM_PERIOD (2)
#define RESP_NUM_PERIOD (4)

// Declare functions
void pulseISR();
void respISR();
void queueMeasure(void* input);
void recordMeasurements(short measurementSelect);
void measureTemperature();
void measurePressure();
void measurePulse();
void measureRespiration();
void measureEKG();
void toggleLED(void* input);
void sendMessage(int task, int request, char* data, int data_len);
void* parseData(char* data, TaskID request);
void ekgprocessing();

#endif // PERIPHERALSYSTEM_H
