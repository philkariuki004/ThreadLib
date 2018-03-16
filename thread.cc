﻿#include "thread.h"
#include "interrupt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ucontext.h>
#include <deque>
//#include <unordered_map>
#include <iostream>
#include <map>

using namespace std;

ucontext_t *ucontext_ptr;
   
    

//DECLARE SHARED VARIABLES WITH STATIC
/*

Use this two to swap context
*/
static ucontext_t *old_context;


static std::deque<ucontext_t*> readyQueue ;
static ucontext_t *curr=new ucontext_t;//The thread currently running.

static bool HAS_INIT=false;// Boolean to make sure we don't initialize thread library twice


int start(thread_startfunc_t func, void *arg); //The stub Function as described in lecture!


static map< unsigned int, std::deque<ucontext_t*> > allLocks; //A Hashmap of each lockID to lock_queue. The first item of the lock_queue is the lock that has aquired the lock. 

static map<std::pair<unsigned int, unsigned int>, std::deque<ucontext_t*> > monitors; //A Hashmap of each Monitor / mutex to the cv_queue. 


extern int thread_libinit(thread_startfunc_t func, void *arg){
  interrupt_disable();
  if (HAS_INIT==true)
  {
    // interrupt_enable();
      return -1;

  }
  HAS_INIT=true;

  //Initialize current thread/context to old_context.
    
  old_context=new ucontext_t;

  if (getcontext(old_context)==-1)
  {
      interrupt_enable();
      return -1;
  }
    
  //old_context->DONE=false;
  char *stack=new char[STACK_SIZE];
  old_context->uc_stack.ss_sp=stack;
  old_context->uc_stack.ss_size=STACK_SIZE;
  old_context->uc_stack.ss_flags=0;
  old_context->uc_link=NULL;

  //Declare Parent thread:
  ucontext_t *parent=new ucontext_t;
   

  //Parent thread initialization
  int err_get = getcontext(parent);
  if (err_get==-1)
  {
      interrupt_enable();
      return -1;
  }

  //parent->DONE=false;
  char *p_stack=new char[STACK_SIZE];
  parent->uc_stack.ss_sp=p_stack;
  parent->uc_stack.ss_size=STACK_SIZE;
  parent->uc_stack.ss_flags=0;
  parent->uc_link=NULL;

  //modify context pointer
  makecontext(parent, (void (*)()) start, 2, func, arg);

  curr=parent;

  if (swapcontext(old_context, parent)==-1)
  {
      interrupt_enable();
      return -1;
  }

  //DELETE THREADS IF OLD_CONTEXT IS EVER REACTIVATED
    
  // TODO: CLEAN UP AND FREE SPACE.
    
   while(!(readyQueue.empty())){
    
  delete curr->uc_stack.ss_sp;
  curr->uc_stack.ss_sp=NULL;
  curr->uc_stack.ss_size=0;
  curr->uc_stack.ss_flags=0;
  delete curr;
    
  curr=readyQueue.front();
  readyQueue.pop_front();
  cout << "I did reached cleanup ON WHILE LOOP END\n";

    
  //TODO:Switch to another thread
   if (swapcontext(old_context,curr)==-1)
  {
    
      interrupt_enable();
      return -1;
  }
    

}

    
      cout << "Thread library exiting.\n";
      exit(0);

    

}

extern int thread_create(thread_startfunc_t func, void *arg){
  interrupt_disable();
  if (HAS_INIT==false)
  {
      interrupt_enable();
    
      return -1;
  }
    
  //Declare child thread:
  ucontext_t *child = new ucontext_t;
    
  //child thread initialization from curr context
  int err_get = getcontext(child);
  if (err_get==-1)
  { 
      interrupt_enable();
      return -1;
  }

  //child->DONE=false;
  char *stack=new char[STACK_SIZE];
  child->uc_stack.ss_sp=stack;
  child->uc_stack.ss_size=STACK_SIZE;
  child->uc_stack.ss_flags=0;
  child->uc_link=NULL;



  makecontext(child, (void (*)()) start, 2, func, arg);
    

  readyQueue.push_back(child);//PUSH TO BACK OF READY QUEUE.
  interrupt_enable();


  return 0;

}

extern int thread_yield(void){
    interrupt_disable();
    if (HAS_INIT==false)
    {
        interrupt_enable();
      return -1;
    }
    

    //cout <<curr<<'\n';
    //cout <<readyQueue.front()<<'\n';

    
    if(readyQueue.empty()){
       return 0;
    }
   
    
    ucontext_t *oldOne;
    oldOne=curr;

    curr=readyQueue.front();
    readyQueue.pop_front();
    
    readyQueue.push_back(oldOne);
    //cout << "Size is"<<readyQueue.size()<<'\n';
    
    
  //TODO:
    if (swapcontext(oldOne,curr)==-1)
  {
      //interrupt_enable();
    
      return -1;
  }
    // interrupt_enable();
     

  //interrupt_enable();
    return 0;

}
//Stub Function
int start(thread_startfunc_t func,void *arg){
    

   // interrupt_disable();
  if (HAS_INIT==false)
  {
      //interrupt_enable();
      return -1;
  }

    
  func(arg);

  //DELETE THREAD AFTER IT FINISHES RUNNING:WE HAVE TO GO TO ANOTHER CONTEXT TO DO SO.
  //Go back to old context. Seems to be only context not modified.
  if (swapcontext(curr,old_context)==-1)
  {
      //interrupt_enable();
    
      return -1;
  }

  //interrupt_enable();
    return 0;//Thread exits

}


//static map<std::pair<unsigned int, unsigned int>, std::deque<thread_TCB*> > monitors;
int thread_wait(unsigned int lockID, unsigned int cvID){
  
   //IMPORTANT / TO ADD - CALL UNLOCK(lockID)!! (I think we might need a stub instead of something else?)
   //Error: The thread tries to unlock a lock it doesn't have:
   if( allLocks[lockID].front()!=curr ) // The front of lockID's queue will be the TCB that currently holds the lock. 
   {
       return -1; //error
   }
   
   std::pair<unsigned int, unsigned int> new_pair (lockID, cvID);

   if( monitors.find(new_pair) == monitors.end()) // if pair / lockID-cvID is not found--> it should be in the monitor
   {
       std::deque<ucontext_t*> new_queue; 
       monitors.insert( std::make_pair(new_pair, new_queue) ); //then make the pair
   }  

   std::deque<ucontext_t*>& sleep_queue = monitors[new_pair]; // get queue associated w/pair
   
   /* Perform switch stuff: */
   ucontext_t* old_thread; // store the current thread. *** note is it okay that old_TCB is local?***
   old_thread = curr;
    
   if(readyQueue.empty())
   {
    return -1;// can't pop from an empty Q --or should it be 0?
   }

   curr = readyQueue.front(); //changes curr -> to front of ready list
   readyQueue.pop_front();
   
   sleep_queue.push_back(old_thread);//PUSHES RUNNING THREAD TO BACK OF PAIR"S SLEEP_QUEUE

   swapcontext(old_thread, curr); 

  
   //return 0;
}



int thread_signal(unsigned int lockID, unsigned int cvID){
   // Put head of cvID's sleep_queue on the tail of the ready queue BLOCKED -> READY



   return 0;
}

int thread_broadcast(unsigned int lockID, unsigned int cvID){
   // Put all of the cvID's sleep_queue on the ready queue. BLOCKED -> READY


   return 0;
}




/*Input is lockID*/

//static std::unordered_map<unsigned int, std::deque<thread_TCB*> > allLocks; <---note this is above
int thread_lock(unsigned int lockID){
    interrupt_disable(); 

    // if not in map of locks, create it:
    if (allLocks.find(lockID) == allLocks.end())
        {
              std::deque<ucontext_t*> new_queue;
              allLocks.insert(std::make_pair(lockID, new_queue)); //***SYNTAX CHECK
  }

    // Now it is in the map
    std::deque<ucontext_t*>& lock_queue = allLocks[lockID];
    
    if( lock_queue.front() == curr ) //Error: Thread tries to aquire a lock it already has
       {
       return -1; 
       }

    if ( lock_queue.empty() ) //if(Lock == FREE): curr aquires the lock!
       {
              lock_queue.push_front(curr); // put curr at the front of lock_queue, curr aquired the lock!
        return 0;
       }
  
     //Elif the lock is busy then current thread is pushed to the lock_queue. We perform a swap context. 
     else
        { 
          ucontext_t* old_thread; // store the current thread. *** note is it okay that this is local?***
          old_thread = curr;
    
          if(readyQueue.empty()) //person aquired lock, can't be on ready, can't be on lock, can't be running -> must be waiting.
    {
         return -1;
          }

          curr = readyQueue.front(); //changes curr to front of ready list
          readyQueue.pop_front();

    lock_queue.push_back(old_thread);//PUSHES RUNNING THREAD TO BACK OF LOCK QUEUE
    thread_unlock(lockID);

    swapcontext(old_thread, curr);  //RUNS FRONT OF READY QUEUE
    thread_lock(lockID);


  }

    interrupt_enable();
    return 0;
}


//Input is lockID - Somewhere need to make a stub so wait() can call unlock
int thread_unlock(unsigned int lockID){
    interrupt_disable();

    if (allLocks.find(lockID) != allLocks.end()) //If the lock is in the map:
    {
        //Error: The thread tries to unlock a lock it doesn't have:
        if( allLocks[lockID].front() != curr or allLocks[lockID].empty() )
        {
          return -1; //error
        }
        std::deque<ucontext_t*>& lock_queue = allLocks[lockID]; // Get lockID from the map. 
  
        allLocks[lockID].pop_front(); // Current thread releases the lock! But keeps running. (aquired lockID == TLB at front of it's lock_queue)
  
        if(!(lock_queue.empty())) // IF a thread is waiting on the lock queue: BLOCKED/lQ -> READY QUEUE & AQUIRES LOCK
        {
        
        ucontext_t* t = lock_queue.front(); // Store thread_TCB from the top of lock queue 
        //lock_queue.pop_front(); //nvm DON'T pop this TCB, keep at top of lock queue as t aquired the lock! 
        // Leave lock as aquired by this thread! (Hand off locks from lecture)

        readyQueue.push_back(t); // Put t from lockqueue on back of ready queue  BLOCKED -> READY QUEUE

        }
    }

    // else lockID not in the map, than no lock to unlock-> user error return -1.
    else
    {
        return -1;
    }

    interrupt_enable();
    return 0;
}




