#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include "structures.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>
#include <string.h>

// Generuje klucz dla mechanizmów komunikacji
key_t get_key(const char *path, int id);

// Funkcje pamięci dzielonej
int create_shared_memory(key_t key, size_t size, int flags);
void *attach_shared_memory(int shmid);
void detach_shared_memory(void *addr);
void remove_shared_memory(int shmid);

// Funkcje semaforów
int create_semaphore(key_t key, int sem_number, int initial_value, int flags);
int get_semaphore(key_t key, int flags);
void semaphore_wait(int semid, int sem_number);
void semaphore_signal(int semid, int sem_number);
int get_sem_value(int semid);
void remove_semaphore(int semid);

// Funkcje kolejki komunikatów(kolejka do kontroli)
int create_message_queue(key_t key, int flags);
void remove_message_queue(int msgid);

// Pliki logów
void log_file(int fan_id, const char* file_name, const char* mode);

#endif // IPC_UTILS_H