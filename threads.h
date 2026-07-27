#ifndef THREADS_H
#define THREADS_H

#include "uthreads.h"
#include "setjmp.h"
#include <string>
#include <stdlib.h>

#include <list>
#include <deque>
#include <vector>
#include <iostream>
#include <memory> 

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

#include <functional>


const int RET_ERROR = -1;
const int RET_SUCCESS = 0;
const std::string SYS_ERR_TXT = "system error: ";
const std::string THRD_ERR_TXT = "thread library error: ";
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7


enum tState {
    BLOCKED,
    READY,
    RUNNING
};


class CThread {

    public:
        CThread(thread_entry_point funci, int id);
        
        int getId();
        tState getState();
        thread_entry_point getFunc();

        void setId(int id);
        void setState(tState state);
        void setFunction(thread_entry_point func1);

        sigjmp_buf env;
        int run_count;
        // blocked by sleep when -1
        int wake_up_quantum;

        // bool sleeping = false;
        // blocked by blocking
        bool blocked = false;
        char * stack;

        ~CThread();


    private:
        int tid;
        tState state;
        thread_entry_point func;

};


// Spawner class implementation.
// spawner class is responsible for creating and managing threads, including spawning new threads, validating thread IDs, and keeping track of active threads.
class Spawner {

    private:
        std::vector<std::shared_ptr<CThread>> threads;
        int active_threads;
        
        public:
        Spawner();
        
        bool validate_id(int id);
        std::shared_ptr<CThread> spawn_thread(thread_entry_point funci);
        std::shared_ptr<CThread> get_thread_by_id(int tid);

        int remove_by_id(int id);
        int get_num_active_threads();
        void block_by_id(int id);

        void set_blocked(int id);


        int get_arr_size();

        std::shared_ptr<CThread> resume_by_id(int id);

        tState get_state_by_id(int id);

        void apply_to_active_threads(const std::function<void(std::shared_ptr<CThread>&)>& action);
        
        ~Spawner();

        
};


class Scheduler{

    public:

        std::shared_ptr<CThread> current;
        int quantum_count;


        Scheduler(int quantum_in, Spawner* spa, void (*powerofff)());


        void add_thread_to_ready_list(std::shared_ptr<CThread> thread);

        void shut_off();

        ~Scheduler();

        void swap_to_next_thread(tState state_to_transfer_to);

        void remove_from_ready_list(std::shared_ptr<CThread> thread_to_remove);

        void load_next_thread_from_ready_list();

        static void timer_handler(int sig);

        void set_alarm();

        void handle_tick();

        void push_to_dead_list(std::shared_ptr<CThread> thread_to_kill);

        int wake_up_quantum = -1;

        void clear_dead_list();

    private:
        int quantum;

        static Scheduler* instance;
        Spawner* spawner;

        std::list<std::shared_ptr<CThread>> ready_list = {};        
        std::list<std::shared_ptr<CThread>> dead_list = {};    
        
        void (*poweroff)();
        bool off;


};



#endif