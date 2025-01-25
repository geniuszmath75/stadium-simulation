#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdbool.h>

#define MAX_FANS 10
#define MAX_QUEUE_SIZE 10
#define MAX_STANDS 3
#define MAX_PEOPLE_PER_STAND 3
#define VIP_THRESHOLD 0.005 // VIPs: < 0.5% of total fans

// Ścieżka oraz ID dla mechanizmów IPC
#define KEY_PATH "/tmp"
#define MSG_ID 'A'
#define SHM_ID 'B'
#define SEM_FAN_COUNT_ID 'C'
#define SEM_STAND_BASE 'D'
#define SEM_WAITING_FANS 'E'
#define MSG_MANAGER_ID 'F'

// Kolory dla komunikatów w konsoli
#define RESET "\033[0m"
#define ERROR "\033[1;31m"
#define WORKER "\033[1;34m"
#define FAN "\033[1;32m"
#define MANAGER "\033[1;35m"

// Drużyny
typedef enum
{
    TEAM_UNKNOWN,
    TEAM_A,
    TEAM_B,
} Team;

// Rodzaje komunikatów stanowisk kontroli
typedef enum
{
    INVITE_TO_CONTROL = 1,
    EXIT_MANAGER = 6,
    OTHER_TEAM = 7,
    SAME_TEAM = 8,
    VIP_ENTER = 9,
    ENTER_WITH_CHILDREN = 10,
    EVACUATION_COMPLETE = 11,
} ControlMessageTypes;

// Rodzaje komunkiatów kibica
typedef enum
{
    JOIN_QUEUE = 1,
    JOIN_CONTROL = 2,
    AGGRESSIVE_FAN = 3,
    JOIN_VIP = 4,
    JOIN_WITH_CHILDREN = 5,
} FanMessageTypes;

// Dane kibica
typedef struct
{
    long message_type; // Typ komunikatu dla kolejki
    int fan_id;        // Unikalny identyfikator kibica
    Team team;         // Drużyna kibica
    int age;           // Wiek kibica
    bool is_vip;       // Czy jest VIP-em
    bool is_child;     // Czy jest dzieckiem
} FanData;

// Struktura komunikatu
typedef struct
{
    long message_type; // Typ komunikatu (do kolejek komunikatów)
    int sender; // ID nadawcy
    FanData fData; // Informacje o kibicu
} QueueMessage;

// Struktura pamięci dzielonej
typedef struct
{
    int fans_in_stadium;           // Liczba kibiców na stadionie
    int fans_on_stand[MAX_STANDS]; // Liczba kibiców w każdym stanowisku
    bool entry_paused;             // Czy wejście jest wstrzymane
    bool evacuation;               // Czy rozpoczęta ewakuacja
} SharedData;

#endif
