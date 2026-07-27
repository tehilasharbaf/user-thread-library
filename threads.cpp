
#include "threads.h"


Scheduler *Scheduler::instance = nullptr;

/** translate_address - Translates a virtual address to a physical address
 * @param addr: The virtual address to translate
 * @return: The translated physical address
 */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
                 : "=g"(ret)
                 : "0"(addr));
    return ret;
};


/* CThread class implementation. 
*  Our thread class, which holds the thread's state, id, function, and stack.
*/
CThread::CThread(thread_entry_point funci, int id)
{
    state = tState::READY;
    func = funci;
    tid = id;
    run_count = 0;
    wake_up_quantum = -1;
    stack = nullptr;
}


// Getters and setters for the CThread class, which allow us to access and modify the thread's state, id, and function.
int CThread::getId()
{
    return tid;
}

tState CThread::getState()
{
    return state;
}

thread_entry_point CThread::getFunc()
{
    return func;
}

void CThread::setId(int id)
{
    tid = id;
}

void CThread::setState(tState state1)
{
    state = state1;
}

void CThread::setFunction(thread_entry_point func1)
{
    func = func1;
}



/** Spawner class implementation.
 * Our spawner class, which is responsible for creating and managing threads.
 */
Spawner::Spawner()
{
    threads.reserve(MAX_THREAD_NUM);
}

/** Destructor for the Spawner class, which cleans up the threads vector when the spawner is destroyed.
 */
Spawner::~Spawner()
{
    threads.clear();
}

/** Utilizes the spawner to create a new thread, and returns a shared pointer to the new thread.
 * @param funci: The entry point for the new thread
 * @return: A shared pointer to the newly created thread
 */
std::shared_ptr<CThread> Spawner::spawn_thread(thread_entry_point funci)
{

    std::shared_ptr<CThread> ret = nullptr;

    // check for empty slots in the threads vector
    for (size_t i = 0; i < threads.size(); i++)
    {
        if (threads[i] == nullptr)
        {
            ret = std::make_shared<CThread>(funci, i);
            threads[i] = ret;
            break;
        }
    }

    // if no empty slots, and we haven't reached the max thread limit, create a new thread
    if (ret == nullptr && threads.size() < MAX_THREAD_NUM)
    {
        size_t i = threads.size();
        ret = std::make_shared<CThread>(funci, i);
        threads.push_back(ret);
    }

    // if we successfully created a new thread, set up its stack and environment
    if (ret != nullptr)
    {
        active_threads++;

        if (funci != nullptr)
        {
            ret->stack = new char[STACK_SIZE];
            if (ret->stack == nullptr)
            {
                std::cerr << SYS_ERR_TXT << "memory allocation failed" << "\n";
                exit(1);
            }

            address_t sp = (address_t)ret->stack + STACK_SIZE - sizeof(address_t);
            address_t pc = (address_t)funci;

            // Set up the signal mask to block SIGVTALRM while setting up the thread's environment
            sigset_t set;
            if(sigemptyset(&set) < 0 ) {
                std::cerr << SYS_ERR_TXT << "sigemptyset failed\n";
                exit(1);
            }
            if (sigaddset(&set, SIGVTALRM) < 0 ){
                std::cerr << SYS_ERR_TXT << "sigaddset failed\n";
                exit(1);
            }
           if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
            {
                std::cerr << SYS_ERR_TXT << "sigprocmask failed" << "\n";
                exit(1);
            }

            // Set up the thread's environment using sigsetjmp, and translate the stack and program counter addresses to physical addresses.
            sigsetjmp(ret->env, 1);

            if (sigprocmask(SIG_BLOCK, &set, NULL) < 0)
                {
                    std::cerr << SYS_ERR_TXT << "sigprocmask failed" << "\n";
                    exit(1);
                }

            (ret->env->__jmpbuf)[JB_SP] = translate_address(sp);
            (ret->env->__jmpbuf)[JB_PC] = translate_address(pc);
        }
    }

    return ret;
}

/** Destructor for the CThread class, which cleans up the thread's stack when the thread is destroyed.
 * 
 */ 
CThread::~CThread() {
    if (stack != nullptr){
    delete[] stack;}
}

/** Kills a thread by its id, removing it from the threads vector and decrementing the active thread count.
 * @param id: The id of the thread to kill
 * @return: The id of the killed thread, or RET_ERROR if the thread does not exist
 */
int Spawner::remove_by_id(int id)
{

    if (id >= 0 && static_cast<size_t>(id) < threads.size())
    {

        if (threads[id] != nullptr)
        {
            threads[id] = nullptr;
            active_threads--;
            return id;
        }
    }

    return RET_ERROR;
}

/** Validates a thread id, checking if it is within the bounds of the threads vector and if the thread at that id is not null.
 * @param id: The id of the thread to validate
 * @return: true if the thread exists and is valid, false otherwise
 */
bool Spawner::validate_id(int id)
{
    if (id >= 0 && static_cast<size_t>(id) < threads.size() && threads[id] !=nullptr)
    {
        return true;
    }
    return false;
}

/** Gets a thread by its id, returning a shared pointer to the thread if it exists, or nullptr if it does not.
 * @param tid: The id of the thread to get
 * @return: A shared pointer to the requested thread, or nullptr if it does not exist
 */
std::shared_ptr<CThread> Spawner::get_thread_by_id(int tid)
{
    return threads[tid];
}

/** Gets the number of active threads, returning the active thread count.
 * @return: The number of active threads
 */
int Spawner::get_num_active_threads()
{
    return active_threads;
}

/** Gets the size of the threads vector, returning the number of threads that have been created.
 * @return: The number of threads that have been created
 */
int Spawner::get_arr_size()
{
    return static_cast<int>(threads.size());
}


/** Sets a thread to blocked state, and checks its blocked boolean.
 * @param id: The id of the thread to block
 */
void Spawner::block_by_id(int id)
{
    threads[id]->blocked = true;
    threads[id]->setState(tState::BLOCKED);
}

/** Resumes a thread by its id, setting its blocked boolean to false and its state to READY, and returning a shared pointer to the thread.
 * @param id: The id of the thread to resume
 * @return: A shared pointer to the resumed thread
 */
std::shared_ptr<CThread> Spawner::resume_by_id(int id)
{
    // is called only after checking that is not sleeping zzz
    threads[id]->blocked = false;
    threads[id]->setState(tState::READY);
    return threads[id];
}

/** Gets the state of a thread by its id, returning the thread's state if it exists, or READY if it does not.
 * @param id: The id of the thread to get the state of
 * @return: The state of the requested thread
 */
tState Spawner::get_state_by_id(int id)
{
    return threads[id]->getState();
}

/** Applies a function to all active threads, passing a shared pointer to each thread to the function.
 * @param action: The function to apply to each active thread
 */
void Spawner::apply_to_active_threads(const std::function<void(std::shared_ptr<CThread> &)> &action)
{
    for (auto &thread : threads)
    {
        if (thread != nullptr)
        {
            action(thread);
        }
    }
}

/** Sets a thread to blocked state by its id, setting its blocked boolean to true.
 * @param tid: The id of the thread to block
 */
void Spawner::set_blocked(int tid) {
    threads[tid]->blocked = true;
}  


// Scheduler class implementation. 
// The scheduler class is responsible for managing the threads, including adding them to the ready list, terminating them, and swapping between them.
Scheduler::Scheduler(int quantum_in, Spawner *sp, void (*powerofff)())
{
    quantum = quantum_in;
    spawner = sp;
    ready_list = {};
    dead_list = {};
    quantum_count = 1;
    poweroff = powerofff;
    off = false;
    Scheduler::instance = this;
}


/**  Destructor for the Scheduler class, which cleans up the ready and dead lists when the scheduler is destroyed.
 */
Scheduler::~Scheduler()
{
    ready_list.clear();
    dead_list.clear();
}

/**  Shuts off the scheduler, setting the off boolean to true. This will cause the scheduler to stop scheduling threads and exit when the current thread finishes executing.
 * @return: None
 */
void Scheduler::shut_off() {
    off = true;
}

/** Adds a thread to the ready list, which is a list of threads that are ready to run.
 * @param thread: The thread to add to the ready list
 */
void Scheduler::add_thread_to_ready_list(std::shared_ptr<CThread> thread)
{
    ready_list.push_back(thread);
};

/** Removes a thread from the ready list, which is a list of threads that are ready to run.
 * @param thread: The thread to remove from the ready list
 */
void Scheduler::clear_dead_list() {
    dead_list.clear();
}

/** Swaps to the next thread in the ready list, saving the current thread's state and loading the next thread's state.
 * @param state_to_transfer_to: The state to transfer to the next thread
 */
void Scheduler::swap_to_next_thread(tState state_to_transfer_to)
{

    int retVal = sigsetjmp(current->env, 1);

    // if a snapshot has been taken...
    // we want to replace with a new one
    if (retVal == 0)
    {

        current->setState(state_to_transfer_to);
        if (state_to_transfer_to == READY)
        {
            ready_list.push_back(current);
        }
        else
        {
            // BLOCKED state
            ready_list.remove(current);
        }

        load_next_thread_from_ready_list();
    }

    // if we are here, it means we have returned from a siglongjmp, and we want to continue executing the current thread
    if(off && current->getId() == 0) {
        if(poweroff != nullptr) {
            poweroff();
        }
    }
}

/** Loads the next thread from the ready list, setting it as the current thread and resuming its execution.
 * @return: None
 */
void Scheduler::load_next_thread_from_ready_list()
{

    if (ready_list.empty())
    {
        return;
        // exit(1);
    }

    // get new thread
    current = ready_list.front();
    ready_list.pop_front();

    current->setState(RUNNING);

    // raise count
    ++quantum_count;
    ++(current->run_count);

    spawner->apply_to_active_threads([this](std::shared_ptr<CThread> &thread) {

        if ( thread->wake_up_quantum !=-1 && quantum_count > thread->wake_up_quantum) {
            // thread->sleeping = false;
            thread->wake_up_quantum = -1;
            
            if (!thread->blocked) {
                thread->setState(READY);
                add_thread_to_ready_list(thread);
            }
            
        } });


    set_alarm();

    // resume new thread
    siglongjmp(current->env, 1);
}

/** Adds a thread to the dead list, which is a list of threads that have finished executing and are ready to be cleaned up.
 * @param thread_to_kill: The thread to add to the dead list
 */
void Scheduler::push_to_dead_list(std::shared_ptr<CThread> thread_to_kill) {
    dead_list.push_back(thread_to_kill);
}

/** Removes a thread from the ready list, which is a list of threads that are ready to run.
 * @param to_remove: The thread to remove from the ready list
 */
void Scheduler::remove_from_ready_list(std::shared_ptr<CThread> to_remove)
{
    ready_list.remove(to_remove);
}

/** Handles a timer tick, which is called when the timer expires and the scheduler needs to switch to the next thread.
 * @param sig: The signal number
 */
void Scheduler::timer_handler(int sig)
{
    Scheduler::instance->handle_tick();
}

/** Sets an alarm for the scheduler, which will trigger a timer interrupt after the specified quantum has elapsed.
 * @return: None
 */
void Scheduler::set_alarm()
{
    struct sigaction sa = {0};
    struct itimerval timer = {0};

    sa.sa_handler = &Scheduler::timer_handler;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        std::cerr << SYS_ERR_TXT << "sigacton error" << "\n";
        exit(1);
    }

    // Convert the quantum from microseconds to seconds and microseconds for the timer.
    int secs = quantum / 1000000;
    int usecs = quantum % 1000000;

    timer.it_value.tv_sec = secs;     
    timer.it_value.tv_usec = usecs;   
    timer.it_interval.tv_sec = secs;  
    timer.it_interval.tv_usec = usecs;

    // Set the timer to trigger a SIGVTALRM signal after the specified quantum has elapsed, and repeat at the same interval.
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) < 0)
    {
        std::cerr << SYS_ERR_TXT << "setittimer error" << "\n";
        exit(1);
    }
}

/** Handles a timer tick, which is called when the timer expires and the scheduler needs to switch to the next thread.
 * @return: None
 */
void Scheduler::handle_tick()
{
    swap_to_next_thread(READY);
}
