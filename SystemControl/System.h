#ifndef SYSTEM_H
#define SYSTEM_H

// Define flags for measurement selection
#define MEASURE_EKGC (1 << 3)
#define MEASURE_TEMP (1 << 4)
#define MEASURE_PRES (1 << 5)
#define MEASURE_PULS (1 << 6)
#define MEASURE_RESP (1 << 7)

// Define constants for communication
#define START_OF_MESSAGE ("S")
#define END_OF_MESSAGE ("E")
#define MESSAGE_TOKENIZER ("|")
#define MESSAGE_TERMINATOR ('~')
#define COM_BUF_LEN (96)
#define COM_NUM_BUF (2)

#define EKG_SAMPLES (256)
#define EKG_BLOCK_SIZE (32)

// Define macros for flags
#define setFlag(flag, mask) ((flag) |= (mask))        // Sets the bits in mask to 1 in flag
#define unsetFlag(flag, mask) ((flag) &= ~(mask))     // Sets the bits in mask to 0 in flag

// Define enum for task IDs
typedef enum TaskIDStruct{MeasureTask, WarningTask, EKGTask} TaskID;
// Define enum for peripheral functions
typedef enum PFuncIDStruct{PMeasure, PWarning, PEKG} FuncID;

// Define structs for communication data
typedef struct PMeasureRequestStruct {
    unsigned short measurementSelect;
} PMeasureRequestData;

typedef struct PMeasureResponseStruct {
    unsigned int temperature;
    unsigned int systolic;
    unsigned int diastolic;
    unsigned int pulse;
    unsigned int resp;
    unsigned int ekgfreq;
} PMeasureResponseData;

typedef struct PEKGResponseStruct {
    int index;
    unsigned char data[EKG_BLOCK_SIZE];
} PEKGResponseData;

#endif // SYSTEM_H
