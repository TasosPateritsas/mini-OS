
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "util.h"


static Mutex kernel_mutex=MUTEX_INIT;

PTCB* searchID(Tid_t tid){
	
  PTCB* ptcb = (PTCB*)tid;

	return ptcb = rlist_find(&CURPROC->ptcbs,ptcb,NULL) ? ptcb : NULL;
}
// same with start thread for spawn thread
void init_thread()
{
	PTCB* ptcb=CURTHREAD->ptcb;
	int exit_val;
	Task call = ptcb->main_task;
	int argl=ptcb->argl;
	void* args =ptcb->args;
	exit_val=call(argl,args);

	ThreadExit(exit_val);
}

//initialize ptcb
PTCB* spawn_ptcb(PCB* pcb, Task task,int argl,void* args){
			PTCB* ptcb = (PTCB*) xmalloc(sizeof(PTCB));
			ptcb->pcb=pcb;
			ptcb->argl= argl;
			ptcb->args = (args == NULL) ? NULL : args ;
			ptcb-> refcount=0;
			ptcb -> main_task = task;

			pcb->counter_ptcb++;
			ptcb -> is_detached =UNDETACH;
			ptcb -> cond_var = COND_INIT;
			ptcb->state=NOTEXITED;
			assert(ptcb!= NULL);
			rlnode_init(&ptcb->node,ptcb);
			rlist_push_back(&CURPROC->ptcbs,&ptcb->node);
			return ptcb;


}
/**
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
		if(task!= NULL){

			PTCB* ptcb=spawn_ptcb(CURPROC,task,argl,args);	// spawn ptcb
			TCB* tcb=spawn_thread(CURPROC,init_thread); // spawn tcb

				ptcb->tcb=tcb;// and initialize the ptcb's tcb
        tcb -> ptcb = ptcb;
				ptcb -> tid=(Tid_t)tcb;
				assert(ptcb!=NULL);
				//fprintf(stderr, "%ld %ld\n",rlist_len(&CURPROC->ptcbs),ptcb->tid);
			wakeup(ptcb->tcb);
			//	fprintf(stderr, "active_ptcb\n" );
			return (Tid_t)ptcb;
		}
	return NOTHREAD;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD->ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
			PTCB* ptcb= searchID(tid);
			if(ptcb==NULL)
				return -1;

			
			//assert(cur_id!=-1);

		//	fprintf(stderr, "%ld join\n",ptcb->tid);
		// for ptcb null and ptcb detach the func do nothing or the tcb is equal with
			if(ptcb==NULL ||ptcb->is_detached == DETACH || (Tid_t)CURTHREAD==tid ){
				return -1 ;
			}

			ptcb->refcount++; // increase the joining ptcbs
// until ptcb is exitd or detached waiting
			while(ptcb->state!=EXITED_STATE && ptcb->is_detached != DETACH ){
				kernel_wait(&ptcb->cond_var,SCHED_USER);
			}
		ptcb->refcount--;// decrease the joining ptcbs
			if(ptcb->is_detached==DETACH){
				return -1 ;
			}
// if th exitval doent null must initialize
			if(exitval!=NULL)
				*exitval=ptcb->exit_val;

			// there aren't any more joining ptcb and tcb has end then free ptcb
		if(ptcb->refcount==0){
				rlist_remove(&ptcb->node);
				free(ptcb);
			}


	return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{

	TCB* tcb =(TCB*)tid;
	if(tcb==NULL)return -1 ;
PTCB* ptcb=searchID(tid);

//		fprintf(stderr, "%ld detach\n",ptcb->tid );

	if(ptcb!=NULL || ptcb->state!=EXITED_STATE){
		ptcb->is_detached=DETACH;
		Cond_Broadcast(&ptcb->cond_var);
		return 0;
	}

	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
 			PTCB* ptcb=CURTHREAD->ptcb;
			//assert(ptcb!=NULL);
			if(ptcb!=NULL){
//	fprintf(stderr, "%ld exit\n",ptcb->tid );

	CURPROC->counter_ptcb--;
	ptcb->exit_val=exitval;
//broadcast all the tcbs
	Cond_Broadcast(&ptcb->cond_var);
	
	ptcb->state=EXITED_STATE;
	kernel_unlock();
	sleep_releasing(EXITED,&kernel_mutex,SCHED_USER,0);
	kernel_lock();
}
}

