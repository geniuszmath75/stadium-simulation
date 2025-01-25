#ifndef WORKER_H
#define WORKER_H

#include "structures.h"
#include <unistd.h>

void run_worker_main();
void run_worker_stand(int stand_id);

void handle_signal(int signal);
void cleanup(SharedData* data, int msgid, int shmid, int msgid_manager, int sem_fan_count);

void evacuate_stadium(SharedData *data, int msgid_manager);

#endif // WORKER_H