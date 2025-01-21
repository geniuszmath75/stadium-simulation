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

// Kolory dla komunikatów w konsoli
#define RESET "\033[0m"
#define ERROR "\033[1;31m"
#define WORKER "\033[1;34m"
#define FAN "\033[1;32m"
#define MANAGER "\033[1;35m"

typedef enum
{
    TEAM_A,
    TEAM_B,
    TEAM_UNKNOWN
} Team;

typedef enum
{
    CMD_PAUSE_ENTRY,
    CMD_RESUME_ENTRY,
    CMD_EVACUATE,
    CMD_ALL_LEFT
} CommandType;

typedef struct
{
    long message_type; // Typ komunikatu (do kolejek komunikatów)
    CommandType command;
} CommandMessage;

typedef struct {
    int fans_in_stadium; // Liczba kibiców na stadionie
    int fans_waiting[MAX_QUEUE_SIZE]; // Kolejka oczekujących kibiców
    int fans_on_stand[MAX_STANDS]; // Liczba kibiców w każdym stanowisku
    bool entry_paused; // Czy wejście jest wstrzymane
} SharedData;


typedef struct
{
    long message_type; // Typ komunikatu dla kolejki
    int fan_id;        // Unikalny identyfikator kibica
    Team team;   // Drużyna kibica
    int age;     // Wiek kibica
    bool is_vip; // Czy jest VIP-em
    bool is_child; // Czy jest dzieckiem
} FanData;

#endif
