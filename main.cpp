/**
 * This program implements the Shortest Job First (SJF) scheduling algorithm with preemption.
 * A scheduling thread is created to manage the execution of processes, which are represented by the Process struct.
 * The Process struct contains the following fields:
 * - pid: the process ID
 * - bt: the burst time (in milliseconds)
 * - art: the arrival time of the process
 * The scheduler thread uses a set (sorted by burst time) to store the processes that are ready to be executed.
 * When a process is selected for execution, its burst time is decremented in QUANTUM_TIME milliseconds
 * intervals until it reaches 0, at which point the process is terminated.
 * If a new process arrives with a shorter burst time than the currently executing process, the current process is
 * preempted and added back to the ready set, and the new process is executed.
 * The scheduler also keeps a log of its actions in a file named "scheduler.log".
 */

#include <iostream>
#include <set>
#include <chrono>
#include <thread>
#include <fstream>
#include <mutex>
#include <unordered_map>

using namespace std;
using namespace std::chrono;

// The time quantum for preemption
const int QUANTUM_TIME = 50;

// Struct representing a process
struct Process {
    int pid;
    int bt;
    time_point<system_clock> art;
    // Overloads the '<' operator to sort the processes by burst time
    bool operator<(const Process &p) const {
        return bt < p.bt;
    }
};

// Set of processes ready to be executed
set<Process> processes;

// Map containing the remaining burst time for each process
unordered_map<int, int> remaining_time;

// ID of the process currently being executed
int active_process_pid;

// Mutex for synchronizing access to the ready set and the active process ID
std::mutex mtx;

// Flag to indicate if the user is still entering processes
bool user_alive;

// Mutex for synchronizing access to the user_alive flag
std::mutex user_alive_mtx;

// Mutex for synchronizing access to the scheduler thread
std::mutex scheduler_alive_mtx;

/**
 * The scheduling function executed by the scheduler thread.
 * Continuously selects the process with the shortest burst time from the ready set,
 * decrements its burst time in QUANTUM_TIME intervals, and logs its actions.
 */
void schedule() {
    ofstream logfile; 
    logfile.open("scheduler.log"); // Open logfile for writing
    logfile << ">>> scheduler started <<<" << endl; // Log that scheduler has started
    std::unique_lock<std::mutex> scheduler_alive(scheduler_alive_mtx); // Take lock to ensure scheduler is alive

    while (true) {
        // Acquire lock
        std::unique_lock<std::mutex> lock(mtx);
        if (processes.empty()) { // check if there is any process to schedule
            std::unique_lock<std::mutex> user_liveness_lock(user_alive_mtx); // take lock to check the user is still alive
            if (!user_alive)
                break; // if user is not alive then break the loop
            user_liveness_lock.unlock(); // release the lock
            lock.unlock();
            this_thread::sleep_for(milliseconds(QUANTUM_TIME)); // sleep for quantum time
            continue;
        }

        Process curr = *processes.begin(); // get the process with highest priority
        processes.erase(processes.begin()); // remove the process from the set
        lock.unlock();

        // Log start of execution
        auto start = system_clock::now();
        logfile << time_point_cast<milliseconds>(start).time_since_epoch().count() << ": "
                        << "[ PID:" << curr.pid << " ] " << "Started" << endl;

        auto end = system_clock::now();
        while(true){
            int next_stop = min(QUANTUM_TIME, curr.bt); // get the next stop time
            this_thread::sleep_for(milliseconds(next_stop)); // sleep for next stop time
            curr.bt -= next_stop; // decrement the burst time
            end = system_clock::now();

            if (curr.bt == 0) {
                logfile << time_point_cast<milliseconds>(end).time_since_epoch().count() << ": "
                        << "[ PID:" << curr.pid << " ] " << "Terminated" << endl;
                break;
            }
            else {
                lock.lock();
                if(!processes.empty()){
                    auto next = processes.begin();
                    if(next->bt < curr.bt){ // check if the next process has lower burst time
                        logfile << time_point_cast<milliseconds>(end).time_since_epoch().count() << ": "
                                << "[ PID:" << curr.pid << " ] " << "Preempted by PID:" << next->pid << endl;
                        processes.insert(curr); // insert the current process back to the set
                        lock.unlock();
                        break;
                    }
                }
                lock.unlock();
            }
        }
    }

    logfile << ">>> scheduler ended <<<" << endl;
    scheduler_alive.unlock();
}

int main() {
    // Start the scheduling thread
    user_alive = true;

    thread scheduler(schedule);
    scheduler.detach();

    while (true) {
        int pid, bt;
        cout << "Enter the process ID and burst time (\"0 0\" to exit): \n";
        cin >> pid >> bt;
        if (pid == 0) break;
        Process p = {pid, bt * 1000, system_clock::now()};

        // Acquire lock
        std::unique_lock<std::mutex> lock(mtx);
        processes.insert(p);
        lock.unlock();
    }

    std::unique_lock<std::mutex> lock(user_alive_mtx);
    user_alive = false;
    lock.unlock();

    cout << "waiting for scheduler to exit..." << endl;
    std::unique_lock<std::mutex> scheduler_alive(scheduler_alive_mtx);
    cout << "Process Ended Successfully" << endl;
    return 0;
}