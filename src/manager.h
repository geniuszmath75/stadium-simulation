#ifndef MANAGER_H
#define MANAGER_H

void run_manager(pid_t main_tech_id);
void send_signal_to_workers(int signal, pid_t main_tech_id);

#endif // MANAGER_H