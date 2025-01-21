#include "ipc_utils.h"
#include "structures.h"

key_t get_key(const char *path, int id) {
    key_t key = ftok(path, id);
    if (key == -1) {
        fprintf(stderr, ERROR "ftok: %s" RESET "\n", strerror(errno));
        // perror("ftok");
        exit(1);
    }
    return key;
}

int create_shared_memory(key_t key, size_t size, int flags) {
    int shmid = shmget(key, size, flags);
    if (shmid == -1) {
        perror(ERROR "Nie można utworzyć lub otworzyć pamięci dzielonej");
        exit(1);
    }
    return shmid;
}

void *attach_shared_memory(int shmid) {
    void *addr = shmat(shmid, NULL, 0);
    if (addr == (void *)-1) {
        fprintf(stderr, ERROR "shmat: %s" RESET "\n", strerror(errno));
        // perror("shmat");
        exit(1);
    }
    return addr;
}

void detach_shared_memory(void *addr) {
    if (shmdt(addr) == -1) {
        fprintf(stderr, ERROR "shmdt: %s" RESET "\n", strerror(errno));
        // perror("shmdt");
        exit(1);
    }
}

void remove_shared_memory(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        fprintf(stderr, ERROR "shmctl: %s" RESET "\n", strerror(errno));
        // perror("shmctl");
        exit(1);
    }
}

int create_semaphore(key_t key, int initial_value, int flags) {
    int semid = semget(key, 1, flags);
    if (semid == -1) {
        perror(ERROR "Nie można utworzyć lub otworzyć semafora");
        exit(1);
    }
    semctl(semid, 0, SETVAL, initial_value);
    return semid;
}

void semaphore_wait(int semid) {
    struct sembuf op = {0, -1, 0};
    if (semop(semid, &op, 1) == -1) {
        fprintf(stderr, ERROR "semop: %s" RESET "\n", strerror(errno));
        // perror("semop wait");
        exit(1);
    }
}

void semaphore_signal(int semid) 
{
    struct sembuf op = {0, 1, 0};
    if (semop(semid, &op, 1) == -1) {
        fprintf(stderr, ERROR "semaphore unlock: %s" RESET "\n", strerror(errno));
        // perror("semop signal");
        exit(1);
    }
}

void remove_semaphore(int semid) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        fprintf(stderr, ERROR "semctl IPC_RMID: %s" RESET "\n", strerror(errno));
        // perror("semctl IPC_RMID");
        exit(1);
    }
}

int create_message_queue(key_t key, int flags) {
    int msgid = msgget(key, flags);
    if (msgid == -1) {
        perror(ERROR "Nie można utworzyć lub otworzyć kolejki komunikatów");
        exit(1);
    }
    return msgid;
}

void remove_message_queue(int msgid) {
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        fprintf(stderr, ERROR "msgctl IRC_RMID: %s" RESET "\n", strerror(errno));
        // perror("msgctl IPC_RMID");
        exit(1);
    }
}