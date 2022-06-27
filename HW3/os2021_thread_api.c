#include "os2021_thread_api.h"
#define HIGH 2
#define MEDIUM 1
#define LOW 0
#define FILE_NAME "init_threads.json"

struct itimerval Signaltimer;
ucontext_t dispatch_context;
ucontext_t finish_context;
ucontext_t timer_context;

struct thread
{
    char *name;
    int thread_id;
    //priority: H=2, M=1, L=0
    int original_priority;
    int current_priority;
    int cancel_mode; //0=asynchronous, 1=deffer(reclaimer can't goto the end)
    int cancel_status; //0=nothing, 1=waiting cancel
    int event_id; //event I want wait, -1 = no waiting
    unsigned long long	 queing_time; //queueing time
    unsigned long long	 waiting_time; //waiting time
    unsigned long long	 sleep_time; //if you need ThreadWaitTime
    unsigned long long	 running_time;
    ucontext_t context; //to save your context
    struct thread* next;
};
typedef struct thread thread;

thread* READY_QUEUE_HEAD = NULL;
thread* WAITING_QUEUE_HEAD = NULL;
thread* TERMINATE_QUEUE_HEAD = NULL;
thread* RUNNING_THREAD = NULL;
int THREAD_ID = 0;

int get_priority_int(char* priority)
{
    int result = -1;
    if(priority[0] == 'L')
        result = 0;
    else if(priority[0] == 'M')
        result = 1;
    else if(priority[0] == 'H')
        result = 2;
    return result;
}

char get_priority_char(int priority)
{
    char result;
    if(priority == 0)
        result = 'L';
    else if(priority == 1)
        result = 'M';
    else if(priority == 2)
        result = 'H';
    return result;
}

int get_time_quantum(int priority)
{
    int result = 0;
    if(priority == 0)
        result = 300;
    else if(priority == 1)
        result = 200;
    else if(priority == 2)
        result = 100;
    return result;
}

void print_ready_queue(thread* QUEUE_HEAD)
{
    thread *c = QUEUE_HEAD;
    while(c!=NULL )
    {
        printf("%s -> ", c->name);
    }
    printf("\n");
}

thread* new_thread( char *name, char* priority, int thread_id,int cancel_mode )
{
    thread* new_node = (thread*)malloc(sizeof(thread));
    new_node->name = name;
    new_node->thread_id = thread_id;
    new_node->original_priority = get_priority_int(priority);
    new_node->current_priority = get_priority_int(priority);
    new_node->cancel_mode = cancel_mode;
    new_node->cancel_status = 0;
    new_node->event_id = -1;
    new_node->queing_time = 0;
    new_node->waiting_time = 0;
    new_node-> sleep_time = 0;
    new_node-> running_time = 0;
    new_node->next = NULL;
    return new_node;
}

thread* ENQUEUE(thread* QUEUE_HEAD, thread* node)
{
    //QUEUE EMPTY
    if(QUEUE_HEAD == NULL)
    {
        node->next =NULL;
        QUEUE_HEAD = node;
    }
    else
    {
        thread* previous = NULL;
        thread* current = QUEUE_HEAD;
        while( (current != NULL) && (current->current_priority >= node->current_priority ) )
        {
            previous = current;
            current = current->next;
        }
        if(previous == NULL)
        {
            node->next = current;
            QUEUE_HEAD = node;
        }
        else
        {
            node->next = current;
            previous->next = node;
        }
    }
    return QUEUE_HEAD;
}

thread* DEQUEUE(thread** QUEUE_HEAD) //POP HEAD
{
    if(*QUEUE_HEAD == NULL)
        return NULL;

    thread* current_head = *QUEUE_HEAD;
    thread* new_head = current_head->next;
    *QUEUE_HEAD = new_head;
    current_head->next = NULL;
    return current_head;
}

thread* REMOVE_NODE(thread** QUEUE_HEAD, char* name)
{
    if(*QUEUE_HEAD == NULL)
        return NULL;

    thread *previous = NULL;
    thread *current = *QUEUE_HEAD;
    while(current != NULL)
    {
        if(current->name == name)
            break;
        else
        {
            previous = current;
            current = current->next;
        }
    }
    if(previous == NULL)  // removed node is head
    {
        *QUEUE_HEAD = current->next;
        current->next = NULL;
        return current;
    }
    else
    {
        previous->next = current->next;
        current->next = NULL;
        return current;
    }
}

int OS2021_ThreadCreate(char* name, char* entry_function, char* priority, int cancel_mode)
{
    thread* new_node = new_thread(name, priority, ++THREAD_ID, cancel_mode);
    if(entry_function[0] == 'R')
        CreateContext(&new_node->context, &finish_context, &ResourceReclaim);
    else if(entry_function[8] == '1')
        CreateContext(&new_node->context, &finish_context, &Function1);
    else if(entry_function[8] == '2')
        CreateContext(&new_node->context, &finish_context, &Function2);
    else if(entry_function[8] == '3')
        CreateContext(&new_node->context, &finish_context, &Function3);
    else if(entry_function[8] == '4')
        CreateContext(&new_node->context, &finish_context, &Function4);
    else if(entry_function[8] == '5')
        CreateContext(&new_node->context, &finish_context, &Function5);
    else
    {
        free(new_node);
        return -1;
    }
    READY_QUEUE_HEAD = ENQUEUE(READY_QUEUE_HEAD, new_node);
    return THREAD_ID;
}

void OS2021_ThreadCancel(char* name)
{
    if( strcmp("reclaimer",name) == 0)
        return;

    // RUNNING_THREAD itself is the thread to be canceled
    if(RUNNING_THREAD->name == name)
    {
        if(RUNNING_THREAD->cancel_mode == 0)
        {
            TERMINATE_QUEUE_HEAD = ENQUEUE(TERMINATE_QUEUE_HEAD, RUNNING_THREAD);
            printf("Thread %s move to TERMINATE QUEUE. \n", RUNNING_THREAD->name);
            return;
        }
        else
        {
            printf("Thread %s sets cancel status to 1. \n", RUNNING_THREAD->name);
            RUNNING_THREAD->cancel_status = 1;
            return;
        }
    }
    // READY QUEUE
    thread* current_node_ready = READY_QUEUE_HEAD;
    while(current_node_ready != NULL) // Find the node named name
    {
        if(current_node_ready->name == name)
            break;
        else
            current_node_ready = current_node_ready->next;
    }
    if(current_node_ready->cancel_mode == 0)
    {
        thread* removed =  REMOVE_NODE(&READY_QUEUE_HEAD, name);
        TERMINATE_QUEUE_HEAD = ENQUEUE(TERMINATE_QUEUE_HEAD, removed);
        printf("Thread %s move to TERMINATE QUEUE. \n", RUNNING_THREAD->name);
        return;
    }
    else
    {
        printf("Thread %s sets cancel status to 1. \n", current_node_ready->name);
        current_node_ready->cancel_status = 1;
        return;
    }
    // WAITING QUEUE
    thread* current_node_waiting = WAITING_QUEUE_HEAD;
    while(current_node_waiting != NULL) // Find the node named name
    {
        if(current_node_waiting->name == name)
            break;
        else
            current_node_waiting = current_node_waiting->next;
    }
    if(current_node_waiting->cancel_mode == 0)
    {
        thread* removed =  REMOVE_NODE(&WAITING_QUEUE_HEAD, name);
        TERMINATE_QUEUE_HEAD = ENQUEUE(TERMINATE_QUEUE_HEAD, removed);
        printf("Thread %s move to TERMINATE QUEUE. \n", RUNNING_THREAD->name);
        return;
    }
    else
    {
        printf("Thread %s sets cancel status to 1. \n", current_node_waiting->name);
        current_node_waiting->cancel_status = 1;
        return;
    }
}

void OS2021_ThreadWaitEvent(int event_id)
{
    RUNNING_THREAD->event_id = event_id;
    printf("%s wants to wait for event %d\n", RUNNING_THREAD->name, RUNNING_THREAD->event_id);

    if(RUNNING_THREAD->current_priority == LOW || RUNNING_THREAD->current_priority == MEDIUM )
    {
        printf("The priority of thread %s is changed from %c to %c\n", RUNNING_THREAD->name, get_priority_char(RUNNING_THREAD->current_priority), get_priority_char(RUNNING_THREAD->current_priority+1));
        RUNNING_THREAD->current_priority++;
    }

    WAITING_QUEUE_HEAD = ENQUEUE(WAITING_QUEUE_HEAD, RUNNING_THREAD);
    swapcontext(&RUNNING_THREAD->context, &dispatch_context);
}

void OS2021_ThreadSetEvent(int event_id)
{
    thread* current = WAITING_QUEUE_HEAD;
    thread* removed;
    while(current != NULL)
    {
        if(current->event_id == event_id)
            break;
        else
            current = current->next;
    }
    if(current != NULL)
    {
        printf("%s changes the status of %s to READY.\n", RUNNING_THREAD->name, current->name);
        removed = REMOVE_NODE(&WAITING_QUEUE_HEAD, current->name);  // remove current from waiting queue to ready queue
        READY_QUEUE_HEAD = ENQUEUE(READY_QUEUE_HEAD, removed);
    }
    else
    {
        printf("No threads are waiting for the event, nothing is done.\n");
    }
    return;
}

void OS2021_ThreadWaitTime(int time)
{
    RUNNING_THREAD->sleep_time = time * 10;
    WAITING_QUEUE_HEAD = ENQUEUE(WAITING_QUEUE_HEAD, RUNNING_THREAD);
    if(RUNNING_THREAD->current_priority == LOW || RUNNING_THREAD->current_priority == MEDIUM )
    {
        RUNNING_THREAD->current_priority++;
        printf("The priority of thread %s is changed from %c to %c\n", RUNNING_THREAD->name, get_priority_char(RUNNING_THREAD->original_priority), get_priority_char(RUNNING_THREAD->current_priority));
    }
    swapcontext(&RUNNING_THREAD->context, &dispatch_context);
}

void OS2021_DeallocateThreadResource()
{
    thread *current = TERMINATE_QUEUE_HEAD;
    while( (current = DEQUEUE(&TERMINATE_QUEUE_HEAD)) != NULL  )
    {
        printf("The memory space by %s has been released.\n", current->name);
        free(current);
    }
    return;
}

void OS2021_TestCancel()
{
    if(RUNNING_THREAD->cancel_status == 1)
    {
        TERMINATE_QUEUE_HEAD = ENQUEUE(TERMINATE_QUEUE_HEAD, RUNNING_THREAD);
        swapcontext(&RUNNING_THREAD->context, &dispatch_context);
    }
}

void CreateContext(ucontext_t *context, ucontext_t *next_context, void *func)
{
    getcontext(context);
    context->uc_stack.ss_sp = malloc(STACK_SIZE);
    context->uc_stack.ss_size = STACK_SIZE;
    context->uc_stack.ss_flags = 0;
    context->uc_link = next_context;
    makecontext(context,(void (*)(void))func,0);
}

void TIME_HANDLER()
{
    // ready queue
    thread* current_node_ready = READY_QUEUE_HEAD;
    while(current_node_ready != NULL)
    {
        current_node_ready->queing_time += 10;
        current_node_ready = current_node_ready->next;
    }
    // waiting queue
    thread* current_node_waiting = WAITING_QUEUE_HEAD;
    while(current_node_waiting != NULL)
    {
        current_node_waiting->waiting_time += 10;
        if(current_node_waiting->sleep_time > 0) // thread wait time
        {
            current_node_waiting->sleep_time -= 10;
            if(current_node_waiting->sleep_time == 0) // move back into ready queue
            {
                thread* removed = REMOVE_NODE(&WAITING_QUEUE_HEAD, current_node_waiting->name);
                READY_QUEUE_HEAD = ENQUEUE(READY_QUEUE_HEAD, removed);
            }
            else
                current_node_waiting = current_node_waiting->next;
        }
        else
            current_node_waiting = current_node_waiting->next;
    }

    RUNNING_THREAD->running_time += 10;
    if(RUNNING_THREAD->running_time >= get_time_quantum(RUNNING_THREAD->current_priority) )
    {
        // 歸零
        RUNNING_THREAD->running_time = 0;
        if(RUNNING_THREAD->current_priority == HIGH || RUNNING_THREAD->current_priority == MEDIUM)
        {
            printf("The priority of thread %s is changed from %c to %c\n", RUNNING_THREAD->name, get_priority_char(RUNNING_THREAD->current_priority), get_priority_char(RUNNING_THREAD->current_priority-1) );
            RUNNING_THREAD->current_priority--;
        }
        READY_QUEUE_HEAD = ENQUEUE(READY_QUEUE_HEAD, RUNNING_THREAD);
        swapcontext(&RUNNING_THREAD->context, &dispatch_context);
    }
    ResetTimer();
}

void CTRL_Z_HANDLER()
{
    printf("\n*********************************************************************************************************\n");
    printf("*\tTID\tName\t\tState\t\tB_Priority\tC_Pirority\tQ_Time\t\tW_Time\t*\n");
    printf("*\t%d\t%-10s\tRUNNING\t\t%c\t\t%c\t\t%lld\t\t%lld\t*\n", RUNNING_THREAD->thread_id, RUNNING_THREAD->name, get_priority_char(RUNNING_THREAD->original_priority), get_priority_char(RUNNING_THREAD->original_priority), RUNNING_THREAD->queing_time, RUNNING_THREAD->waiting_time);
    thread* p_ready = READY_QUEUE_HEAD;
    while(p_ready!=NULL)
    {
        printf("*\t%d\t%-10s\tREADY\t\t%c\t\t%c\t\t%lld\t\t%lld\t*\n", p_ready->thread_id, p_ready->name,get_priority_char(p_ready->original_priority), get_priority_char(p_ready->current_priority), p_ready->queing_time, p_ready->waiting_time);
        p_ready = p_ready->next;
    }
    thread* p_waiting = WAITING_QUEUE_HEAD;
    while(p_waiting != NULL)
    {
        printf("*\t%d\t%-10s\tWAITING\t\t%c\t\t%c\t\t%lld\t\t%lld\t*\n", p_waiting->thread_id, p_waiting->name,get_priority_char(p_waiting->original_priority), get_priority_char(p_waiting->current_priority), p_waiting->queing_time, p_waiting->waiting_time);
        p_waiting = p_waiting->next;
    }
    printf("*********************************************************************************************************\n");
}

void ResetTimer()
{
    Signaltimer.it_value.tv_sec = 0;
    Signaltimer.it_value.tv_usec = 10000; // 10ms
    if(setitimer(ITIMER_REAL, &Signaltimer, NULL) < 0)
    {
        printf("ERROR SETTING TIME SIGALRM!\n");
    }
}

void Dispatcher()
{
    while(1)
    {
        RUNNING_THREAD = DEQUEUE(&READY_QUEUE_HEAD);
        RUNNING_THREAD->running_time = 0;
        ResetTimer();
        swapcontext(&dispatch_context, &RUNNING_THREAD->context);
    }
}

void Finish()
{
    RUNNING_THREAD->next = NULL;
    TERMINATE_QUEUE_HEAD =  ENQUEUE(TERMINATE_QUEUE_HEAD, RUNNING_THREAD);   // change from running to terminate queue
    setcontext(&dispatch_context); // reschedule
}

void PARSE_JSON()
{
    struct json_object *parsed_json;
    struct json_object *threads;
    struct json_object *thread;
    struct json_object *name;
    struct json_object *entry_function;
    struct json_object *priority;
    struct json_object *cancel_mode;

    parsed_json = json_object_from_file(FILE_NAME);
    json_object_object_get_ex(parsed_json, "Threads", &threads);
    int thread_count = json_object_array_length(threads);
    if(thread_count > 0)
    {
        for(int i=0; i<thread_count; i++)
        {
            thread = json_object_array_get_idx(threads,i);
            json_object_object_get_ex(thread,"name",&name);
            json_object_object_get_ex(thread,"entry function",&entry_function);
            json_object_object_get_ex(thread,"priority",&priority);
            json_object_object_get_ex(thread,"cancel mode",&cancel_mode);

            char* NAME = (char*)json_object_get_string(name);
            char* ENTRY_FUNCTION = (char*)json_object_get_string(entry_function);
            char* PRIORITY = (char*)json_object_get_string(priority);
            int CANCEL_MODE = (int)json_object_get_int(cancel_mode);

            OS2021_ThreadCreate(NAME, ENTRY_FUNCTION, PRIORITY, CANCEL_MODE);
        }
    }
    return;
}

void StartSchedulingSimulation()
{
    // SET TIMER
    Signaltimer.it_interval.tv_usec = 0;
    Signaltimer.it_interval.tv_sec = 0;
    signal(SIGALRM, TIME_HANDLER);
    signal(SIGTSTP, CTRL_Z_HANDLER);

    // Create Context
    CreateContext(&dispatch_context, &timer_context, &Dispatcher);
    CreateContext(&finish_context, &dispatch_context, &Finish);

    PARSE_JSON();
    OS2021_ThreadCreate("reclaimer", "ResourceReclaim", "L", 1);
    ResetTimer();
    setcontext(&dispatch_context);
}