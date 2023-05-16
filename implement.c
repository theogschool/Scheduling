//----------------------------
// NAME: Theo Gerwing
// STUDENT NUMBER: 7864797
// COURSE: COMP 3430
// INSTRUCTOR: Bristow
// ASSIGNMENT: 3
//
// REMARKS: Creates "tasks" and then runs them through a scheduling policy.
//
// -------------------------

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

//The different names of the scheduling policies.
const char ROUND_ROBIN[] = "rr";
const char SHORTEST_TIME[] = "stcf";
const char MLFQ[] = "mlfq";


int cpuTime = 0; //The time that has passed
int MAX_LINE = 500; //The of a line in the tasks.txt file
int timeslice = 5; //Length of timeslice
int mySchedule = 0; //A number representing the current policy; 0 = rr, 1 = stcf, 2 = mlfq

pthread_mutex_t callLock = PTHREAD_MUTEX_INITIALIZER; //Lock between cpu and dispatcher
pthread_mutex_t accessQueueLock = PTHREAD_MUTEX_INITIALIZER; //Lock for accessing the queue.
pthread_cond_t alertDispatcher = PTHREAD_COND_INITIALIZER; //Alert the dispatcher is empty
pthread_cond_t alertCpu = PTHREAD_COND_INITIALIZER; //Alert the cpus that the dispatcher is not empty
int done = 0; //When the queue is empty and not cpus are working, this becomes 1

//Nodes in the queue
typedef struct NODE{
  char *name;
  int type;
  int priority;
  int time;
  int io;
  struct NODE *next;
} Node;

//The queue struct
typedef struct QUEUE{
  Node *head;
  Node *tail;
  int size;
  int missingNodes;
} Queue;

Queue *runningQueue; //The currently running tasks
Queue *doneQueue; //Tasks that are done
Node *dispatcher; //The task in the dispatcher

//function definitions
void createQueue(int);
void insertToQueue(Node *, int);
void insertToDoneQueue(Node *);
void runSimulation(int);
void printResults(int);

//-------------------
//runTask()
//Purpose: Cpus run here. They get a task from the dispatcher, perform the task then wait for the dispatcher
//--------------------
static void *runTask(){
  Node *myNode = NULL; //The node from the dispatcher
  int myTimeSlice = 0; //Copy of the current time
  srand(time(0)); //rand for io
  while(done == 0){
    pthread_mutex_lock(&callLock);
    while(dispatcher == NULL && done == 0){
      pthread_cond_wait(&alertCpu, &callLock);
    }
    if(done == 0)
    {
      myNode = dispatcher;
      dispatcher = NULL;
      pthread_cond_signal(&alertDispatcher);
      pthread_mutex_unlock(&callLock);
      if((rand() % 101) < myNode->io)//Decide if io is performed
      {
        myTimeSlice = rand() % (timeslice + 1);
      }
      else
      {
        myTimeSlice = timeslice;
      }
      if(myTimeSlice > myNode->time)
      {
        myTimeSlice = myNode->time;
      }
      myNode->time = myNode->time - myTimeSlice;
      cpuTime = cpuTime + myTimeSlice;
      if(myNode->time <= 0)//If myNode <= 0 then put it in the doneQueue
      {
        myNode->time = cpuTime;
        pthread_mutex_lock(&accessQueueLock);
        insertToDoneQueue(myNode);
        runningQueue->missingNodes--;
        pthread_mutex_unlock(&accessQueueLock);
      }
      else
      {
        pthread_mutex_lock(&accessQueueLock);
        insertToQueue(myNode, mySchedule);
        runningQueue->missingNodes--;
        pthread_mutex_unlock(&accessQueueLock);
      }
      
    }
    else
    {
      pthread_cond_signal(&alertDispatcher);
      pthread_mutex_unlock(&callLock);
    }
  }

  return NULL;
}
//------------------
//runDispatcher()
//
//PURPOSE: Gets a nodes from the queue and puts it in the dispatcher. Alerts CPUs a task is avalible.
//------------------
static void *runDispatcher(){
  Node *myNode = NULL; //Node to put in the dispatcher
  while(done == 0)
  {
    pthread_mutex_lock(&callLock);
    while(dispatcher != NULL)
    {
      pthread_cond_wait(&alertDispatcher, &callLock);
    }
    pthread_mutex_lock(&accessQueueLock);
    if((runningQueue->head == NULL) && (runningQueue->missingNodes == 0))//Check if a task is ready, if so put it in the dispatcher
    {
      done = 1;
      pthread_cond_signal(&alertCpu);
      pthread_mutex_unlock(&callLock);
    }
    else if(runningQueue->head == NULL)
    {
      pthread_mutex_unlock(&accessQueueLock);
      pthread_cond_signal(&alertCpu);
      pthread_mutex_unlock(&callLock);
    }
    else
    {
      myNode = runningQueue->head;
      runningQueue->head = runningQueue->head->next;
      myNode->next = NULL;
      runningQueue->missingNodes++;
      runningQueue->size--;
      pthread_mutex_unlock(&accessQueueLock);
      dispatcher = myNode;
      pthread_cond_signal(&alertCpu);
      pthread_mutex_unlock(&callLock);
    }  
  }
  return NULL;
}

//-------------------
//main
//
//PURPOSE: Gets information from the command line and runs different functions based on that information.
int main(int argc, const char * argv[])
{
  if(argc == 3)
  {
    if(!strcmp(argv[2], ROUND_ROBIN))
    {
      mySchedule = 0;
      createQueue(mySchedule);
      runSimulation(atoi(argv[1]));
      printResults(atoi(argv[1]));
    }
    else if(!strcmp(argv[2], SHORTEST_TIME))
    {
      mySchedule = 1;
      createQueue(mySchedule);
      runSimulation(atoi(argv[1]));
      printResults(atoi(argv[1]));
    }
    else if(!strcmp(argv[2], MLFQ))
    {
      mySchedule = 2;
      createQueue(mySchedule);
      runSimulation(atoi(argv[1]));
      printResults(atoi(argv[1]));
    }
    else
    {
      printf("Invalid scheduling policy\n");
    }
  }
  else
  {
    printf("Invalid Arguments\n");
  }
  return 0;
}

//----------------------------
//createQueue()
//
//PURPOSE: Create and allocate memory for the queue and it's nodes. Put all tasks into the runningQueue
//INPUT PARAMETERS:
//schedule: The int associated with the scheduling policy.
//------------------------------
void createQueue(int schedule){
  FILE *file = fopen("tasks.txt", "r"); //Opens tasks.txt
  char *line = (char *) malloc(MAX_LINE * sizeof(char)); //A line in task.txt will iterate through all of them
  char *split; //Helps to use strtok() as split to split on space
  Node *currentNode = NULL; //The node being created
  size_t maxOneLine = MAX_LINE; //The max size of a line in tasks.txt

  runningQueue = (Queue *) malloc(sizeof(Queue));
  runningQueue->head = NULL;
  runningQueue->tail = NULL;
  runningQueue->size = 0;
  runningQueue->missingNodes = 0;

  doneQueue = (Queue *) malloc(sizeof(Queue));
  doneQueue->head = NULL;
  doneQueue->tail = NULL;
  doneQueue->size = 0;
  doneQueue->missingNodes = 0;

  while(getline(&line,&maxOneLine, file) != -1)
  {
    currentNode = (Node *) malloc(sizeof(Node));
    split = strtok(line, " ");
    currentNode->name = strdup(split);
    split = strtok(NULL, " ");
    currentNode->type = atoi(strdup(split));
    split = strtok(NULL, " ");
    currentNode->priority = atoi(strdup(split));
    split = strtok(NULL, " ");
    currentNode->time = atoi(strdup(split));
    split = strtok(NULL, " ");
    currentNode->io = atoi(strdup(split));
    currentNode->next = NULL;
    insertToQueue(currentNode, schedule); 
  }
  fclose(file);
  free(line);
  
}

//----------------------------
//insertToDoneQueue
//
//PURPOSE: inserts the Node received into the doneQueue.
//INPUT PARAMETERS:
//newNode: the Node to be insert.
//----------------------------
void insertToDoneQueue(Node * newNode){
  if(doneQueue->head == NULL)
  {
    doneQueue->head = newNode;
    doneQueue->tail = newNode;
    doneQueue->size++;
  }
  else
  {
    doneQueue->tail->next = newNode;
    doneQueue->tail = newNode;
    doneQueue->size++;
  }
}

//---------------------------
//insertToQueue()
//
//PURPOSE: Insert the Node received into the runningQueue. Will change how it is insert based on the scheduling policy
//INPUT PARAMETERS:
//newNode = the node to be insert. schedule: The int representation of the scheduling algorithm.
//--------------------------
void insertToQueue(Node * newNode, int schedule){
  Node *currNode = runningQueue->head; //A node iterator
  Node *prevNode = NULL; //The node before the iterator
  int hasInsert = 0; //Whether the given node has been insert

  if(currNode == NULL)
  {
    runningQueue->head = newNode;
    runningQueue->tail = newNode;
    runningQueue->size++;
  }
  else if(schedule == 2)
  {
    while(currNode != NULL)
    {
      if((newNode->priority < currNode->priority) && (prevNode == NULL))
      {
        newNode->next = currNode;
        runningQueue->head = newNode;
        currNode = NULL;
        runningQueue->size++;
        hasInsert++;
      }
      else if(newNode->priority < currNode->priority)
      {
        newNode->next = currNode;
        prevNode->next = newNode;
        currNode = NULL;
        runningQueue->size++;
        hasInsert++;
      }
      else
      {
        prevNode = currNode;
        currNode = currNode->next;
      }
    }
    if(!hasInsert)
    {
      prevNode->next = newNode;
      runningQueue->tail = newNode;
      runningQueue->size++;
    }
  }
  else if(schedule == 1)
  {
    while(currNode != NULL)
    {
      if((newNode->time < currNode->time) && (prevNode == NULL))
      {
        newNode->next = currNode;
        runningQueue->head = newNode;
        currNode = NULL;
        runningQueue->size++;
        hasInsert++;
      }
      else if(newNode->time < currNode->time)
      {
        newNode->next = currNode;
        prevNode->next = newNode;
        currNode = NULL;
        runningQueue->size++;
        hasInsert++;
      }
      else
      {
        prevNode = currNode;
        currNode = currNode->next;
      }
    }
    if(!hasInsert)
    {
      prevNode->next = newNode;
      runningQueue->tail = newNode;
      runningQueue->size++;
    }
  }
  else
  {
    runningQueue->tail->next = newNode;
    runningQueue->size++;
    runningQueue->tail = newNode;
  }
}

//------------------------------------
//runSimulation()
//
//PURPOSE: Creates threads and then runs the cpu and dispatcher. 
//INPUT PARAMETERS: 
//numCpus: The number of cpus to run the simulation with
//------------------------------------
void runSimulation(int numCpus){
  pthread_t cpu[numCpus]; //The cpu threads
  pthread_t dispatch; //The dispatcher thread
  for(int i = 0; i < numCpus; i++)
  {
    pthread_create(&cpu[i], NULL, runTask, NULL);
  }
  pthread_create(&dispatch, NULL, runDispatcher, NULL);
  pthread_join(dispatch, NULL); 
}
//----------------------------------
//printResults()
//
//PURPOSE: Print the results of the simulation. Frees memory at the end
//INPUT PARAMETERS:
//numCpus: the number of cpus to run the simulation with.
void printResults(int numCpus){
  char *scheduleName; //The name of the schedule
  
  //Arrays holding the data to be printed
  int pTime[] = {0, 0, 0, 0};
  int pCount[] = {0, 0, 0, 0};
  int tTime[] = {0, 0, 0, 0};
  int tCount[] = {0, 0, 0, 0};
 
  Node *curr = NULL; //Node iterator

  for(int i = 0; i < doneQueue->size; i++)
  {
    if(curr == NULL)
    {
      curr = doneQueue->head;
    }
    else
    {
      curr = curr->next; 
    }
    pTime[curr->priority] = pTime[curr->priority] + curr->time;
    pCount[curr->priority]++;
    tTime[curr->type] = tTime[curr->type] + curr->time;
    tCount[curr->type]++;
  } 
  for(int i = 0; i < 4; i++)
  {
    if(pCount[i] == 0)
    {
      pCount[i] = 1;
      pTime[i] = 0;
    }
    
    if(tCount[i] == 0)
    {
      tCount[i] = 1;
      tTime[i] = 0;
    }
  }

  if(mySchedule == 0)
  {
    scheduleName = "pure round-robin";
  }
  else if(mySchedule == 1)
  {
    scheduleName = "shortest time to completion first";
  }
  else
  {
    scheduleName = "a multi-level queue";
  }

  printf("Using %s with %d CPUs.\n\n", scheduleName, numCpus);

  printf("Average run time per priority:\n");
  printf("Priority 0 average run time: %d\n", pTime[0] / pCount[0]);
  printf("Priority 1 average run time: %d\n", pTime[1] / pCount[1]);
  printf("Priority 2 average run time: %d\n\n\n", pTime[2] / pCount[2]);
  
  printf("Average run time per type:\n");
  printf("Type 0 average run time: %d\n", tTime[0] / tCount[0]);
  printf("Type 1 average run time: %d\n", tTime[1] / tCount[1]);
  printf("Type 2 average run time: %d\n", tTime[2] / tCount[2]);
  printf("Type 3 average run time: %d\n", tTime[3] / tCount[3]);

  //Frres memory.
  while(doneQueue->head != NULL)
  {
    curr = doneQueue->head;
    doneQueue->head = doneQueue->head->next;
    curr->next = NULL;
    free(curr);
  }
  free(doneQueue);
  free(runningQueue);
}
