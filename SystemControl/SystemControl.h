#ifndef SYSTEMCONTROL_H
#define SYSTEMCONTROL_H

#include "System.h"

// Define constant values for the scheduling
#define TASKDELAY_5s (20)
#define TASKDELAY_1s (4)
#define TASKCOUNT (8)

// Define the constnt values for warning blinking rate
#define BLOODPRESSURE_RATE (1)
#define TEMPERATURE_RATE (2)
#define PULSERATE_RATE (4)

// Define constant values for measure
#define MEASURE_BUF_LEN (8)
#define EKG_FFT_BUF_LEN (16)

// Define constant values for remote communication
#define REM_COM_BUF_LEN (96)
#define REM_COM_NUM_BUF (2)

// Defines Bool type
typedef enum BoolStruct { FALSE = 0, TRUE = 1 } Bool;

// Define enum for commands
typedef enum CommandIDStruct{
    InitializeCommand = 'I',
    StartCommand = 'S',
    StopCommand = 'P',
    DisplayCommand = 'D',
    MeasureCommand = 'M',
    WarningCommand = 'W',
    ErrorResponse = 'E'} CommandID;

// Defines TCB struct
typedef struct TaskStruct {
  void(*taskPtr)(void*);
  void* taskInputPtr;
  struct TaskStruct* nextTask;
  struct TaskStruct* prevTask;
  int id;
} TCB;  //Define this struct as a "TCB"

// Defines structs for TCB Inputs
typedef struct MeasureData {
  unsigned int* temperatureRawBufPtr;
  unsigned int* bloodPressRawBufPtr;
  unsigned int* pulseRateRawBufPtr;
  unsigned int* respirationRateRawBufPtr;
  unsigned short* measurementSelectPtr;
} MeasureTaskInput;

typedef struct EKGData {
  unsigned int* ekgRawBufPtr;
  unsigned int* ekgFreqBufPtr;
} EKGTaskInput;

typedef struct ComputeData {
  unsigned int* temperatureRawBufPtr;
  unsigned int* bloodPressRawBufPtr;
  unsigned int* pulseRateRawBufPtr;
  unsigned int* respirationRateRawBufPtr;
  unsigned int* tempCorrectedBufPtr;
  unsigned int* bloodPressCorrectedBufPtr;
  unsigned int* prCorrectedBufPtr;
  unsigned int* respirationRateCorrectedBufPtr;
  unsigned short* measurementSelectPtr;
} ComputeTaskInput;

typedef struct WarningData {
  unsigned int* temperatureRawBufPtr;
  unsigned int* bloodPressRawBufPtr;
  unsigned int* pulseRateRawBufPtr;
  unsigned int* respirationRateRawBufPtr;
  unsigned int* ekgFreqBufPtr;
  unsigned short* batteryStatePtr;
} WarningTaskInput;

typedef struct DisplayData {
  unsigned short* displayStatePtr;
  unsigned int* tempCorrectedBufPtr;
  unsigned int* bloodPressCorrectedBufPtr;
  unsigned int* prCorrectedBufPtr;
  unsigned int* respirationRateRawBufPtr;
  unsigned int* ekgFreqBufPtr;
  unsigned short* batteryStatePtr;
} DisplayTaskInput;

typedef struct StatusData {
  unsigned short* batteryStatePtr;
} StatusTaskInput;

typedef struct KeypadData {
	unsigned short* displayStatePtr;
	unsigned short* measurementSelectPtr;
	unsigned short* alarmAckPtr;
} KeypadTaskInput;

typedef struct CommunicationsData {
	unsigned short* measurementSelectPtr;
	unsigned int* tempCorrectedBufPtr;
	unsigned int* bloodPressCorrectedBufPtr;
	unsigned int* prCorrectedBufPtr;
} CommunicationsTaskInput;

typedef struct RemoteDisplayData {
  unsigned int* tempCorrectedBufPtr;
  unsigned int* bloodPressCorrectedBufPtr;
  unsigned int* prCorrectedBufPtr;
  unsigned int* respirationRateCorrectedBufPtr;
  unsigned int* ekgFreqBufPtr;
  unsigned long* tempWarningAmtPtr;
  unsigned long* bpWarningAmtPtr;
  unsigned long* prWarningAmtPtr;
  unsigned long* rrWarningAmtPtr;
  unsigned long* ekgWarningAmtPtr;
} RemoteDisplayTaskInput;

typedef struct CommandData {
  //Command task data here
} CommandTaskInput;

typedef struct RemoteCommunicationData {
  //Remote communication task data here
} RemoteCommunicationTaskInput;

typedef struct TrafficManagementData {
  //Traffic Management task data here
} TrafficManagementTaskInput;

typedef struct InitializeCommandStruct {
    char doctorName[40];
    char patientName[40];
} InitializeCommandData;

// Defines function prototypes
void schedule(void);                                      // Define function prototype for "schedule" task
void measure(void* dataPtr);                              // Define function prototype for "measure" task
void compute(void* input);                                // Define function prototype for "compute" task
void warning(void* input);                                // Define function prototype for "warning" task
void display(void* taskDataPtr);                          // Define function prototype for "display" task
void status(void* input);                                 // Define function prototype for "status" task
void keypad (void* input);								  // Define functio prototype for "keypad" task
void communications(void* input);

void getEKGValues(int index);
void storeEKGValues(void* input);

void sendMessage(int task, int request, char* data, int data_len);
void sendRemoteResponse(CommandID command, char* message, int message_len);
void* parseData(char* data, TaskID task);
void* parseCommandData(char* data, CommandID command);

void initializeCommandCallback(void* data);
void startCommandCallback(void* data);
void stopCommandCallback(void* data);
void displayCommandCallback(void* data);
void measureCommandCallback(void* data);
void warningCommandCallback(void* data);

//***************************TFT variables**********************************************************************

// The control pins for the LCD can be assigned to any digital or
// analog pins...but we'll use the analog pins as this allows us to
// double up the pins with the touch screen (see the TFT paint example).
#define LCD_CS A3    // Chip Select goes to Analog 3
#define LCD_CD A2    // Command/Data goes to Analog 2
#define LCD_WR A1    // LCD Write goes to Analog 1
#define LCD_RD A0    // LCD Read goes to Analog 0
#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// Color definitions
#define ILI9341_BLACK       0x0000      /*   0,   0,   0 */
#define ILI9341_NAVY        0x000F      /*   0,   0, 128 */
#define ILI9341_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define ILI9341_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define ILI9341_MAROON      0x7800      /* 128,   0,   0 */
#define ILI9341_PURPLE      0x780F      /* 128,   0, 128 */
#define ILI9341_OLIVE       0x7BE0      /* 128, 128,   0 */
#define ILI9341_LIGHTGREY   0xC618      /* 192, 192, 192 */
#define ILI9341_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define ILI9341_BLUE        0x001F      /*   0,   0, 255 */
#define ILI9341_GREEN       0x07E0      /*   0, 255,   0 */
#define ILI9341_CYAN        0x07FF      /*   0, 255, 255 */
#define ILI9341_RED         0xF800      /* 255,   0,   0 */
#define ILI9341_MAGENTA     0xF81F      /* 255,   0, 255 */
#define ILI9341_YELLOW      0xFFE0      /* 255, 255,   0 */
#define ILI9341_WHITE       0xFFFF      /* 255, 255, 255 */
#define ILI9341_ORANGE      0xFD20      /* 255, 165,   0 */
#define ILI9341_GREENYELLOW 0xAFE5      /* 173, 255,  47 */
#define ILI9341_PINK        0xF81F

/******************* UI details */
//X defines the MIDDLE of the width of first button in reference to the long side
//Y defines the MIDDLE of the height of first button in reference to short side
// 2 button mode (STATE 1)
#define BUTTON_X_1 160
#define BUTTON_Y_1 65
#define BUTTON_W_1 250

//selecting menu mode (state 2) Where a is the selections and b is Enter/Exit
#define BUTTON_H_2 20
#define BUTTON_X_2b 100
#define BUTTON_Y_2 25
#define BUTTON_W_2a 250
#define BUTTON_W_2b 100
#define BUTTON_SPACING_Y_2 10
////display mode (state 3)
#define BUTTON_Y_3 155

//annunciation mode
#define BUTTON_X_4 90
#define BUTTON_Y_4 220
//width of exit button will be 100
#define BUTTON_W_4 80
#define BUTTON_H_4 60
#define BUTTON_SPACING_X_2 60 

//default values
#define BUTTON_H 35
#define BUTTON_SPACING_Y 20
#define BUTTON_TEXTSIZE 2
#define BUTTON_SPACING_X 20


#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM 8   // can be a digital pin
#define XP 9   // can be a digital pin

//Touch For New ILI9341 TP
#define TS_MINX 70
#define TS_MAXX 920

#define TS_MINY 120
#define TS_MAXY 900
//***************************************************************************************

// Define touchpad constants
#define MINPRESSURE 10
#define MAXPRESSURE 1000


#endif // !SYSTEMCONTROL_H
