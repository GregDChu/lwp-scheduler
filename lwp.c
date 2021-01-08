#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "lwp.h"

/* SCHEDULER */
context *sched_threads = NULL; // Pool (list) of all scheduler's threads
context *sched_back = NULL; // Back of scheduler's thread list
context *selected_thread = NULL; // Most recent thread returned by my_next()
// Initialize the scheduler structure
static struct scheduler my_sched = {NULL, NULL, my_admit, my_remove, my_next};
// Initialize the current scheduler with round robin scheduler
scheduler curr_sched = &my_sched;

void my_admit(thread new)
{
    // Verify the thread exists before doing work
    if(new != NULL)
    {
        // Start new thread list
        if(sched_threads == NULL)
        {
            sched_threads = new;
            sched_back = new;
        }
        // Or append to existing thread list
        else
        {
            sched_back->sched_one = new;
            new->sched_one = NULL;
            sched_back = new;
        }
    }
}

void my_remove(thread victim)
{
    context *prev_thread = NULL;
    context *curr_thread = sched_threads;
    // Find the thread
    while(curr_thread != NULL && curr_thread != victim)
    {
        prev_thread = curr_thread;
        curr_thread = curr_thread->sched_one;
    }
    // If thread exists, remove it
    if(curr_thread != NULL)
    {
        // Thread is at the front
        if(prev_thread == NULL)
        {
            sched_threads = curr_thread->sched_one;
        }
        // Thread is not at the front
        else
        {
            // Check if the thread is at the back
            if(curr_thread == sched_back)
            {
                sched_back = prev_thread;
            }
            prev_thread->sched_one = curr_thread->sched_one;
        }
        // Handle case where victim is currently active
        if(selected_thread == victim)
        {
            selected_thread = NULL;
        }
        
    }
}

thread my_next()
{
    // Stop if there are no threads to return
    if(sched_threads == NULL)
    {
        return NO_THREAD;
    }
    // Grab the next thread if it exists
    if(selected_thread != NULL && selected_thread->sched_one != NULL)
    {
        selected_thread = selected_thread->sched_one;
    }
    // Otherwise, grab from the front
    else
    {
        selected_thread = sched_threads;
    }
    return selected_thread;
}

void  lwp_set_scheduler(scheduler fun)
{
    // Check if no scheduler was passed
    // Default to round robin (my_sched)
    if(fun == NULL)
    {
        fun = &my_sched;
    }
    // Guard against setting the same sched twice
    scheduler sched = lwp_get_scheduler();
    // Implementation for no pre-existing scheduler
    if(sched == NULL)
    {
        // Init new scheduler
        if(fun->init != NULL)
        {
            fun->init();
        }
        // Assign current scheduler
        curr_sched = fun;
    }
    // Implementation for existing scheduler
    else if(sched != fun)
    {
        // Init new scheduler
        if(fun->init != NULL)
        {
            fun->init();
        }
        // Transfer all threads from old to new scheduler
        thread pulled_thrd = sched->next();
        while(pulled_thrd != NULL)
        {
            //fprintf(stderr, "Pulling thread %d\n", (int)pulled_thrd->tid);
            sched->remove(pulled_thrd);
            fun->admit(pulled_thrd);
            pulled_thrd = sched->next();
        }
        // Break down current scheduler
        if(sched->shutdown != NULL)
        {
            sched->shutdown();
        }
        // Update the current scheduler to new one
        curr_sched = fun;
    }
}

scheduler lwp_get_scheduler()
{
    return curr_sched;
}

/* LWP METHODS */
tid_t thread_count = 0; // Used for thread ID
context *threads = NULL; // List of existing threads
context *real_context = NULL; // A home for the real original context
context *active_thread = NULL; // The current, active thread being run
static unsigned long *tmp_stack = NULL; // A place to build a temporary stack

#define TMP_STACK_SIZ 1024 // Temporary stack size

tid_t lwp_create(lwpfun function, void * argument , size_t stacksize)
{
    // Try to create the thread context before doing any work
    context *new_context = calloc(1, sizeof(context));
    if(new_context == NULL)
    {
        fprintf(stderr, "lwp_create(): Couldn't allocate context memory...\n");
        return -1;
    }
    // Assign a thread ID
    new_context->tid = ++thread_count;
    // Load the passed argument into register storage of context
    new_context->state.rdi = (unsigned long)argument;
    // Deal with float stuff
    new_context->state.fxsave = FPU_INIT;
    // Create thread stack
    unsigned long *stack = calloc(stacksize, sizeof(unsigned long));
    if(stack == NULL)
    {
        fprintf(stderr, "lwp_create(): Couldn't allocate stack memory...\n");
        return -1;
    }
    new_context->stacksize = stacksize * sizeof(unsigned long);
    new_context->stack = stack;
    // Calculate the top of the stack
    unsigned long *stack_start = (unsigned long *)(stack + stacksize - 1);
    int offset = 0;
    // Push exit funtion on the stack
    // (for programs that simply 'return')
    stack_start[offset--] = (unsigned long)lwp_exit;
    // Push the return address onto the stack
    // (the function to start running)
    stack_start[offset--] = (unsigned long)function;
    // Move the base pointer to new base
    new_context->state.rbp = (unsigned long)(stack_start + offset);
    // Push old base pointer address onto stack
    stack_start[offset--] = 0;

    // Set stack pointer to end of stack
    // (assuming remaining space is for local vars)
    new_context->state.rsp = (unsigned long)stack;

    // Add it to a thread pool
    new_context->lib_one = threads;
    threads = new_context;
    // Add it to the scheduler
    scheduler sched = lwp_get_scheduler();
    // Verify that a scheduler was found
    if(sched == NULL)
    {
        fprintf(stderr, "lwp_exit_rest(): Can't find a scheduler...\n");
        return -1;
    }
    sched->admit(new_context);
    // Return the thread ID
    return new_context->tid;
}

thread tid2thread(tid_t tid)
{
    // Search through all existing threads
    context *thread = threads;
    while(thread != NULL &&
    thread->tid != tid)
    {
        thread = thread->lib_one;
    }
    // If it exists, return a match
    if(thread != NULL)
    {
        return thread;
    }
    // Otherwise, return NO_THREAD
    return NO_THREAD;
}

tid_t lwp_gettid()
{
    if(active_thread == NULL)
    {
        return NO_THREAD;
    }
    return active_thread->tid;
}

void  lwp_start()
{
    if(active_thread != NULL)
    {
        fprintf(stderr, "lwp_start(): An instance is already running...\n");
        return;
    }
    // Get a thread from the scheduler
    scheduler sched = lwp_get_scheduler();
    // Verify that a scheduler was found
    if(sched == NULL)
    {
        fprintf(stderr, "lwp_start(): Can't find a scheduler...\n");
        return;
    }
    context *next_thread = sched->next();
    // Return if the scheduler has no thread to offer
    if(next_thread == NO_THREAD)
    {
        return;
    }
    // Update active thread
    active_thread = next_thread;
    // Check if the current original context has a place to live
    if(real_context == NULL)
    {
        // Allocate memory for the original context
        real_context = calloc(1, sizeof(context));
        if(real_context == NULL)
        {
            fprintf(stderr, "lwp_start(): Memory allocation failed.\n");
            return;
        }
    }
    // Save the original context,
    // load the desired context,
    // and let magic64 run the rest
    swap_rfiles(&(real_context->state), &(next_thread->state));
    // Once it returns from lwp_stop, 
    // free up the current context just to be safe
    free(real_context);
    real_context = NULL;
    active_thread = NULL;
}

void lwp_stop()
{
    // Clean up temporary stack in case it was used
    if(tmp_stack != NULL)
    {
        free(tmp_stack);
        tmp_stack = NULL;
    }
    // Verify that a real context exists
    if(real_context == NULL)
    {
        fprintf(stderr, "lwp_stop(): There is no context to return to...\n");
        return;
    }
    // Guard against swapping using a NULL active thread
    if(active_thread == NULL)
    {
        fprintf(stderr, "lwp_stop(): There is no active thread to stop...\n");
        return;
    }
    // Saves the current active thread's context
    // and loads the original context
    swap_rfiles(&(active_thread->state), &(real_context->state));
}

void lwp_exit_rest()
{
    // Terminate lwp
    // Save active thread ptr for free-ing
    context *exiting_thrd = active_thread;
    // Remove thread from scheduler
    scheduler sched = lwp_get_scheduler();
    // Verify that a scheduler was found
    if(sched == NULL)
    {
        fprintf(stderr, "lwp_exit(): Can't find a scheduler...\n");
        return;
    }
    sched->remove(active_thread);
    // Find thread in library thread pool
    context *prev_thread = NULL;
    context *curr_thread = threads;
    while(curr_thread != NULL && curr_thread != exiting_thrd)
    {
        prev_thread = curr_thread;
        curr_thread = curr_thread->lib_one;
    }
    // Throw error if the thread can't be found
    if(curr_thread == NULL)
    {
        fprintf(stderr, "lwp_exit(): Can't find thread in thread pool\n");
        return;
    }
    // Remove thread from library thread pool
    if(prev_thread == NULL)
    {
        threads = curr_thread->lib_one;
    }
    else
    {
        prev_thread->lib_one = curr_thread->lib_one;
    }
    // Free the thread
    free(exiting_thrd);
    // Gets the next available thread
    context *next_thread = sched->next();
    active_thread = next_thread;
    // Before any context loading, ensure
    // the original context exists
    if(real_context == NULL)
    {
        fprintf(stderr, "lwp_exit(): Unsafe to continue. \
        No context to return to...\n");
        return;
    }
    // Restore the original context if there are no more threads
    if(next_thread == NULL)
    {
        load_context(&(real_context->state));
    }
    // Otherwise, load next thread's context
    else
    {
        load_context(&(next_thread->state));
    }
}

void  lwp_exit()
{
    // Prior to freeing lwp resources:
    // Set up temporary stack if not already set up
    if(tmp_stack == NULL)
    {
        tmp_stack = calloc(TMP_STACK_SIZ, sizeof(unsigned long));
        if(tmp_stack == NULL)
        {
            fprintf(stderr, "lwp_exit(): Memory allocation failed.\n");
            return;
        }
    }
    // Save SP in case of failure
    unsigned long real_sp;
    GetSP(real_sp);
    // Move stack pointer and base pointer to temporary stack
    // (This should keep it safe while thread is freed)
    SetSP(tmp_stack + TMP_STACK_SIZ - 1);
    // Let the machine create a nice stack frame
    // and finish the rest of the work in there
    lwp_exit_rest();
    // If lwp_exit_rest returned, some error occured...
    fprintf(stderr, "lwp_exit(): Returning to a safe place...\n");
    // Return to a safe place
    SetSP(real_sp);
    return;
}

void  lwp_yield()
{
    context *yielding_thrd = active_thread;
    // Verify that an active thread exists
    if(yielding_thrd == NULL)
    {
        fprintf(stderr, "lwp_yield(): No active thread to yield...\n");
        return;
    }
    scheduler sched = lwp_get_scheduler();
    // Verify that a scheduler was found
    if(sched == NULL)
    {
        fprintf(stderr, "lwp_yield(): Can't find a scheduler...\n");
        return;
    }
    // Verify a real context exists
    if(real_context == NULL)
    {
        fprintf(stderr, "lwp_stop(): There is no context to return to...\n");
        return;
    }
    context *next_thread = sched->next();
    // Swap to real context if no remaining threads
    if(next_thread == NO_THREAD)
    {
        swap_rfiles(&(yielding_thrd->state), &(real_context->state));
    }
    // Otherwise, swap to next available thread
    else
    {
        active_thread = next_thread;
        swap_rfiles(&(yielding_thrd->state), &(next_thread->state));
    }
}