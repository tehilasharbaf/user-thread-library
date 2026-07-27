
#include "uthreads.h"
#include "threads.h"

#include <sys/time.h>



Scheduler *scheduler;
Spawner *spawner;

/**   
 * Blocks the timer signal
 */
void block_timer()
{
    sigset_t set;
    if (sigemptyset(&set) < 0) {
        std::cerr << SYS_ERR_TXT << "sigemptyset failed\n";
        exit(1);
    }
    if(sigaddset(&set, SIGVTALRM) < 0) {
         std::cerr << SYS_ERR_TXT << "sigaddset failed\n";
        exit(1);
    }
    if (sigprocmask(SIG_BLOCK, &set, NULL) < 0)
    {
        std::cerr << SYS_ERR_TXT << "sigprocmask failed\n";
        exit(1);
    }
}

/**   
 * Unblocks the timer signal
 */
void unblock_timer()
{
    sigset_t set;
    if (sigemptyset(&set) < 0 ) {
        std::cerr << SYS_ERR_TXT << "sigemptyset failed\n";
        exit(1);
    }
    if (sigaddset(&set, SIGVTALRM) < 0 ) {
        std::cerr << SYS_ERR_TXT << "sigadset failed\n";
        exit(1);
    }
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
    {
        std::cerr << SYS_ERR_TXT << "sigprocmask failed\n";
        exit(1);
    }
}

/**   
 * Validates the thread ID and throws an error if it is invalid. Returns 0 if the ID is valid, -1 otherwise.
 */
int validate_id_and_throw_err(int tid) {
    if(spawner->validate_id(tid)) {
        return 0;
    }

    std::cerr << THRD_ERR_TXT << "illegal treds location accessed index " << tid << "\n";
    return -1;
}

/**   
 * Destructs all the resources allocated by the library, including the spawner and scheduler.
 */
void destruct_all() {
    block_timer();
    delete spawner;
    delete scheduler;
}

/**   
 * Destructs all the resources allocated by the library and exits the process with exit code 0.
 */
void destruct_and_exit() {
    destruct_all();
    exit(0);
}


/**
 * Initializes the thread library.
 */

int uthread_init(int quantum_usecs)
{

    if (quantum_usecs <= 0)
    {
        std::cerr << THRD_ERR_TXT << "quanta smaller than 0." << "\n";
        return -1;
    }

    spawner = new Spawner();
    scheduler = new Scheduler(quantum_usecs, spawner, destruct_and_exit);

    scheduler->current = (*spawner).spawn_thread(nullptr);
    scheduler->current->setState(RUNNING);

    scheduler->set_alarm();

    
    spawner->get_thread_by_id(0)->run_count=1;

    return 0;
}


/**
 * Returns the thread ID of the calling thread.
 */
int uthread_get_tid()
{
    return scheduler->current->getId();
}


/**
 * Creates a new thread, whose entry point is the function entry_point with the signature
 */
int uthread_spawn(thread_entry_point entry_point)
{
    block_timer();
    if (entry_point == nullptr)
    {
        std::cerr << THRD_ERR_TXT << "function called with null entry_point" << "\n";
        unblock_timer();
        return -1;
    }
    if (spawner->get_num_active_threads() >= MAX_THREAD_NUM)
    {
        std::cerr << THRD_ERR_TXT << "more than MAX_THREAD_NUM concurent threads" << "\n";
        unblock_timer();
        return -1;
    }
    std::shared_ptr<CThread> tred = spawner->spawn_thread(entry_point);

    scheduler->add_thread_to_ready_list(tred);
    unblock_timer();
    return tred->getId();
}


/**
 * Terminates the thread with ID tid and deletes it from all relevant control structures.
 */
int uthread_terminate(int tid)
{
    block_timer();
    scheduler->clear_dead_list();  

    if (validate_id_and_throw_err(tid) < 0) {
        unblock_timer();
        return -1;
    }

    if (tid == 0)
    {
        if (uthread_get_tid() == 0) {
            unblock_timer();
            destruct_all();
            exit(0);
        } else {
            scheduler->shut_off();
            scheduler->current = spawner->get_thread_by_id(0);
            siglongjmp(scheduler->current->env, 1);
        }
    }
    std::shared_ptr<CThread> thread_to_kill = spawner->get_thread_by_id(tid);
    if (thread_to_kill == nullptr)
    {
        std::cerr << THRD_ERR_TXT << "thread does not exists for id:" << tid << "\n";
        unblock_timer();
        return -1;
    }

    // remove it from everywhere
    scheduler->remove_from_ready_list(thread_to_kill);
    spawner->remove_by_id(tid);

    if (tid == uthread_get_tid())
    {
        scheduler->push_to_dead_list(thread_to_kill);  
        thread_to_kill.reset();
        spawner->remove_by_id(tid);
        scheduler->load_next_thread_from_ready_list();
    }
    unblock_timer();

    return 0;
}

/**
 * Returns the total number of quantums since the library was initialized, including the current quantum.
 */
int uthread_get_total_quantums()
{
    return scheduler->quantum_count;
}


/**
 * Returns the number of quantums the thread with ID tid was in RUNNING state.
 */
int uthread_get_quantums(int tid)
{
    if (validate_id_and_throw_err(tid) < 0) {
        return -1;
    }

    std::shared_ptr<CThread> ptr = spawner->get_thread_by_id(tid);
    if (ptr == nullptr)
    {
        std::cerr << THRD_ERR_TXT << "tried to get quantums for non-existing thread." << "\n";
        return -1;
    }
    else
    {
        return ptr->run_count;
    }
}


/**
 * Blocks the RUNNING thread for num_quantums quantums.
 */
int uthread_sleep(int num_quantums)
{
    block_timer();

    if (num_quantums < 0 || (num_quantums != 0 && scheduler->current->getId() == 0))
    {
        std::cerr << THRD_ERR_TXT << "quanta is negative or for tid0 not0" << "\n";
        return -1;
    }

    // if 0, we yield
    if (num_quantums == 0)
    {
        scheduler->swap_to_next_thread(READY);
    }
    else
    {
        // spawner->block_by_id(scheduler->current->getId());
        // scheduler->inflict_sleep(scheduler->current, num_quantums);

        // scheduler->current->sleeping = true;
        scheduler->current->wake_up_quantum = uthread_get_total_quantums() + num_quantums;

        scheduler->swap_to_next_thread(BLOCKED);
    }
    unblock_timer();
    return 0;
}


/**
 * Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 */
int uthread_block(int tid)
{
    block_timer();
    if (validate_id_and_throw_err(tid) < 0) {
        unblock_timer();
        return -1;
    }

    // if the thread is already blocked, we do nothing
    if (tid == 0)
    {
        std::cerr << THRD_ERR_TXT << "Tried to block main thread" << "\n";
        unblock_timer();
        return -1;
    }

    // if the thread is already blocked, we do nothing
    spawner->set_blocked(tid);
    if (spawner->get_state_by_id(tid) == tState::READY) {
        std::shared_ptr<CThread> tred = spawner->get_thread_by_id(tid);
        scheduler->remove_from_ready_list(tred);
    }

    // if the thread is the current thread, we need to swap to the next thread
    if (tid == scheduler->current->getId())
    {
        scheduler->swap_to_next_thread(BLOCKED);
        unblock_timer();
        return 0;
    }

    spawner->block_by_id(tid);
    unblock_timer();
    return 0;
}


/**
 * Resumes a blocked thread with ID tid and moves it to the READY state.
 */
int uthread_resume(int tid)
{
    block_timer();

    if (validate_id_and_throw_err(tid) < 0) {
        unblock_timer();
        return -1;
    }
    std::shared_ptr<CThread> thread = spawner->get_thread_by_id(tid);
    if (thread == nullptr) {
        std::cerr << THRD_ERR_TXT << "tried to resume non existing thread." << "\n";
        unblock_timer();
        return -1;
    }
    thread->blocked = false;

    // if the thread is sleeping, we do nothing
    if (thread->wake_up_quantum != -1)
    {
        unblock_timer();
        return 0;
    }

    tState tatse = thread->getState();
    // bad calls...
    if (tid == scheduler->current->getId() || tatse != tState::BLOCKED)
    {
        unblock_timer();
        return 0;
    }

    // if the thread is blocked, we resume it and add it to the ready list
    std::shared_ptr<CThread> tred = spawner->resume_by_id(tid);
    if (tred != nullptr)
    {
        scheduler->add_thread_to_ready_list(tred);
        unblock_timer();

        return 0;
    }
        unblock_timer();

    return -1;
}



