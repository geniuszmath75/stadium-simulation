#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>

#define MAX_FANS 50
#define MAX_STANDS 3
#define MAX_PEOPLE_PER_STAND 3
#define VIP_THRESHOLD 0.005 // VIPs: < 0.5% of total fans, 1 - pojawiają się sami kibice VIP

// Ścieżka oraz ID dla mechanizmów IPC
#define KEY_PATH "/tmp"
#define MSG_ID 'A'
#define SHM_ID 'B'
#define SEM_EVACUATION_ID 'C'
#define SEM_ENTRY_PAUSED_ID 'D'
#define SEM_FAN_COUNT_ID 'E'
#define SEM_WAITING_FANS 'F'
#define MSG_MANAGER_ID 'G'

// Kolory dla komunikatów w konsoli
#define RESET "\033[0m"
#define ERROR "\033[1;31m"
#define WORKER "\033[1;34m"
#define FAN "\033[1;32m"
#define MANAGER "\033[1;35m"

#define TEST_FAN_LIMIT 0 // 0 - BRAK, 1 - podajemy liczbę kibiców do przybycia (większą od MAX_FANS)
#define TEST_AGGRESSIVE_FANS 0 // 0 - BRAK, 1 - pojawiają się sami agresywni kibice
#define TEST_SIGNAL_HANDLING 0 // 0 - EWAKUACJA, 1 - WSTRZYMANIE/WZNOWIENIE KONTROLI

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
    VIP_ENTER = 1,
    JOIN_CONTROL = 2,
    AGGRESSIVE_FAN = 3,
    LET_FAN_GO = 4,
    OTHER_TEAM = 5,
    JOIN_WITH_CHILDREN = 6,
    ENTER_WITH_CHILDREN = 7,
    EVACUATION_COMPLETE = 8,
    FINISH_WORKER = 9
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