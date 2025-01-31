#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>

#define MAX_FANS 100
#define MAX_STANDS 3
#define MAX_PEOPLE_PER_STAND 3
#define VIP_THRESHOLD 0.005 // VIPs: < 0.5% of total fans

// Ścieżka oraz ID dla mechanizmów IPC
#define KEY_PATH "/tmp"
#define MSG_ID 'A'
#define SHM_ID 'B'
#define SEM_MAIN_TECH_ID 'C'
#define SEM_FAN_COUNT_ID 'D'
#define SEM_WAITING_FANS 'E'
#define MSG_MANAGER_ID 'F'
#define SEM_STAND_BASE 'G'

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

// Rodzaje komunikatów
typedef enum
{
    JOIN_CONTROL = 1,
    OTHER_TEAM = 2,
    LET_FAN_GO = 3,
    AGGRESSIVE_FAN = 4,
    JOIN_VIP = 5,
    JOIN_WITH_CHILDREN = 6,
    VIP_ENTER = 7,
    ENTER_WITH_CHILDREN = 8,
    EVACUATION_COMPLETE = 9,
    HAVE_FUN = 10,
} MessageTypes;

// Dane kibica
typedef struct
{
    pid_t fan_pid;
    int fan_id;        // Unikalny identyfikator kibica
    Team team;         // Drużyna kibica
    int age;           // Wiek kibica
    bool is_vip;       // Czy jest VIP-em
    bool is_child;     // Czy jest dzieckiem
    int aggressive_counter; // Czy jest agresywny
} FanData;

// Struktura komunikatu
typedef struct
{
    long message_type; // Typ komunikatu (do kolejek komunikatów)
    pid_t sender; // ID nadawcy
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
