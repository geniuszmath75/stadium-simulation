#ifndef WORKER_H
#define WORKER_H

#include "structures.h"
#include <unistd.h>

void run_worker_main();
void run_worker_stand(int stand_id);

void control_fans(FanData* fans, int stand_id, int *stand_fan_id);
void send_controlled_fan_message(SharedData* data, int *msgid, FanData* fans, QueueMessage message, int *stand_fan_id);
void reset_fan_data(FanData *fan);
bool check_fans_team(Team checking_team, FanData *stand_fans, int fan_idx);

void handle_signal(int signal);
void cleanup(SharedData* data, int msgid, int shmid, int msgid_manager, int sem_fan_count, int sem_evacuation, int sem_entry_paused, int sem_stands, int sem_fans_waiting);

void evacuate_stadium(SharedData *data, int msgid_manager);

void log_file(SharedData* data);

#endif // WORKER_H