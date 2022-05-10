// File:	worker.c

// List all group member's name:js2592 , am2670
// username of iLab:js2592
// iLab Server: atlas.cs.rutgers.edu

#include "worker.h"
// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE
#define RUNNING 0
#define READY 1
#define SCHEDULED 2
#define BLOCKED 3
#define DONE 4
#define STACKSIZE SIGSTKSZ
#define QUANTUM 5

//globals
worker_t threadCount; //count of threads
tqueue* currentRunning;//current running thread
tqueue* head;//head of runqueue
tqueue* arr;//head of thread list
ucontext_t* sContext;//scheduler context

//timer
struct itimerval timer;
int interr;


/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE
	//no scheduler (first worker_create call)
	if (sContext == NULL){
		sContext = (ucontext_t*)malloc(sizeof(ucontext_t));
		getcontext(sContext);

		//init scheduler context
        sContext->uc_stack.ss_sp = malloc(STACKSIZE);
		sContext->uc_stack.ss_size = STACKSIZE;
     	sContext->uc_stack.ss_flags = 0;
        sContext->uc_link = 0;
        makecontext(sContext, schedule, 0);
		//printf("here\n");
		//init signal handler for scheudler time quantum
		signal(SIGPROF, sigScheduler); //sigScheduler initialize
		//now create thread
		tqueue *thread1 = (tqueue*)malloc(sizeof(tqueue));
		thread1->threadCB = (tcb*)malloc(sizeof(tcb));
		thread1->threadCB->tid = 0;
		thread1->threadCB->context = (ucontext_t*)malloc(sizeof(ucontext_t));
		thread1->threadCB->priority = 3;
		thread1->threadCB->status = RUNNING;
		
		//printf("here\n");

		//init arr
		tqueue *threadArr = (tqueue*)malloc(sizeof(tqueue));
		threadArr->threadCB = thread1->threadCB;
		threadArr->threadCB->priority = 3;
		
		//increment
		threadCount++;
		//add thread
		enqueue_thread(arr, threadArr);
		enqueue_rr(head, thread1);
		
		//switch to the current thread
		currentRunning = thread1;

		interr = 1;
	}
	//init new_tcb
	tqueue *new_tcb = (tqueue*)malloc(sizeof(tqueue));
	new_tcb->threadCB = (tcb*)malloc(sizeof(tcb));
	new_tcb->threadCB->status = READY;
	new_tcb->threadCB->tid = threadCount;
	new_tcb->threadCB->priority = 3;
    

	//init context
	ucontext_t* new_context = (ucontext_t*)malloc(sizeof(ucontext_t));
    getcontext(new_context);
	new_tcb->threadCB->context = new_context;
	new_tcb->threadCB->context->uc_stack.ss_sp = malloc(STACKSIZE);
	new_tcb->threadCB->context->uc_stack.ss_size = STACKSIZE;
	new_tcb->threadCB->context->uc_stack.ss_flags = 0;
	
	new_tcb->threadCB->context->uc_link = 0;
	*thread = (worker_t)threadCount;

	//update
	threadCount++;
	//make the context
    makecontext(new_tcb->threadCB->context, (void*)function, 1, arg);
	
	
	//init arr
	tqueue *new_arr = (tqueue*)malloc(sizeof(tqueue));
	new_arr->threadCB = new_tcb->threadCB;
	new_arr->threadCB->priority = 3;
		
	//update
	threadCount++;
	//add thread
	enqueue_thread(arr, new_arr);
	enqueue_rr(head, new_tcb);

	if(interr){
		//start the timer if interr has value
		startTimer();
	}
    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	//need the timer to stop here
	stopTimer();
	currentRunning->threadCB->status = READY;
	//swapcontext() saves and switches
	swapcontext(currentRunning->threadCB->context, sContext);

	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
	//need the timer to stop here
	stopTimer();
	//change status to exit
	currentRunning->threadCB->status = DONE;
	//set retval to value_ptr to exit
	currentRunning->threadCB->retVal = value_ptr;
	//free stuff
	free(currentRunning->threadCB->context->uc_stack.ss_sp);
	//free rest
	swapcontext(currentRunning->threadCB->context, sContext);

};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
	tqueue* curr = arr;
	//find thread
	while(curr->threadCB->tid != thread){
		curr = curr->next;
	}
	//terminate if not already
	while(curr->threadCB->status != DONE){
		worker_yield();
	}
	if (value_ptr){
		value_ptr = &curr->threadCB->retVal;
	}
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE
	mutex->lock = 0;
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
	while(__sync_lock_test_and_set (&(mutex->lock), 1)){
		//need the timer to stop here
		stopTimer();
		currentRunning->threadCB->status = READY;
		//swapcontext() saves and switches
		swapcontext(currentRunning->threadCB->context, sContext);
	}
	mutex->lock = 1;
    return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	mutex->lock = 0;
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init
	//there isnt anything to free since mutexes are integers
	return 0;
};

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function

	// - invoke scheduling algorithms according to the policy (RR or MLFQ)

	// if (sched == RR)
	//		sched_rr();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

// - schedule policy
#ifndef MLFQ
	// Choose RR
	sched_rr();
#else 
	// Choose MLFQ
	sched_mlfq();
#endif
	startTimer();
	currentRunning = head;
	setcontext(head->threadCB->context);
}

/* Round-robin (RR) scheduling algorithm */
static void sched_rr() {
	// - your own implementation of RR
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	head = head->next;
	currentRunning->next = NULL;
	if (currentRunning->threadCB->status == READY || currentRunning->threadCB->status == BLOCKED){
		enqueue_rr(head, currentRunning);
	}
	else if (currentRunning->threadCB->status == DONE){
		//we dont have to do anything here
	}
	else{
		//throw error
		printf("Error in rr\n");
	}

}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
	head = head->next;
	currentRunning->next = NULL;
	if (currentRunning->threadCB->status == READY || currentRunning->threadCB->status == BLOCKED){
		enqueue_mlfq(head, currentRunning);
	}
	else if (currentRunning->threadCB->status == DONE){
		//we dont have to do anything here
	}
	else{
		//throw error
		printf("Error in rr\n");
	}
}

// Feel free to add any other functions you need

// YOUR CODE HERE
//signal handler
void sigScheduler(int signum){
	#ifndef MLFQ
	#else //lower priority if mlfq
	if (currentRunning->threadCB->priority > 0){
		currentRunning->threadCB->priority = currentRunning->threadCB->priority - 1;
	}
	#endif
	currentRunning->threadCB->status = READY;
	getcontext(currentRunning->threadCB->context);
	swapcontext(currentRunning->threadCB->context, sContext);
}

//add a thread to list of thread queues
void enqueue_thread(tqueue* thead, tqueue* new_thread){
	if (thead == NULL){
		arr = new_thread;
		return;
	} else {
		while(thead->next){
			thead = thead->next;
		}
		thead->next = new_thread;
		new_thread->next = NULL;
	}

}

//add a thread to the round robin queue
void enqueue_rr(tqueue* thead, tqueue* new_thread){
	if (thead == NULL){
		head = new_thread;
		head->next = NULL;
		currentRunning = new_thread;
		return;
	} else {
		while(thead->next){
			thead = thead->next;
		}
		thead->next = new_thread;
		new_thread->next = NULL;
	}

}

//add a thread to the mlfq queue
void enqueue_mlfq(tqueue* thead, tqueue* new_thread){
	if (thead == NULL){
		head = new_thread;
		return;
	} else {
		int temp = 0;
		if(thead->next->threadCB->priority > new_thread->threadCB->priority){
			temp = 1;
		}
		while(thead->next!=NULL && temp == 1){
			thead = thead->next;
		}
		if (thead->next == NULL){
			thead->next = new_thread;
			new_thread->next = NULL;
		} else {
			tqueue *temp2 = thead->next;
			thead->next = new_thread;
			new_thread->next = temp2;
		}
	}
}
//start the timer
void startTimer(){
	timer.it_value.tv_sec = 0;
  	timer.it_value.tv_usec = QUANTUM;	
  	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_PROF, &timer, NULL) == -1) {
    	printf("error calling setitimer()");
    	exit(1);
  	}
	
}

//stop the timer
void stopTimer(){
	timer.it_value.tv_sec = 0;
  	timer.it_value.tv_usec = 0;	
  	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 0;
   	if (setitimer(ITIMER_PROF, &timer, NULL) == -1) {
    	printf("error calling setitimer()");
    	exit(1);
  	}
}
