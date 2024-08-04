#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t input_to_display_thread_semaphore, input_to_duplicate_thread_semaphore,duplicate_to_display_thread_semaphore,
to_display_thread_semaphore;
struct LinkedList List;
int msgid;


struct Node
{
    char name[50];
    int age;
    char gender[50];
    struct Node *next;
};

struct LinkedList
{
    struct Node *head;       // linked list to store input values
    struct Node *duplicates; // linked list to store duplicate values
};

struct Message
{
    long mtype;
    struct Node data;
};

void add_node(struct Node *node)
{
   
    pthread_mutex_lock(&mutex);
    if (List.head == NULL)
    {
        List.head = node;
    }
    else
    {
        struct Node *current = List.head;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = node;
    }
    pthread_mutex_unlock(&mutex);
    
}

void add_duplicate_node(struct Node *node)
{
    pthread_mutex_lock(&mutex);
    if (List.duplicates == NULL)
    {
        List.duplicates = node;
    }
    else
    {
        struct Node *current = List.duplicates;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = node;
    }
    pthread_mutex_unlock(&mutex);
}

void display_all()
{
    pthread_mutex_lock(&mutex);
    struct Node *current = List.head;
    if(current == NULL){
        printf("no data...\n");
    }
    else
    {
        while (current != NULL)
        {
        printf("Name: %s, Age: %d, Gender: %s\n", current->name, current->age, current->gender);
        current = current->next;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void check_duplicates(struct Node *newNode){
    int count=0;
   struct Node *current = List.head;
        while (current != NULL)
        {
            if (strcmp(current->name, newNode->name) == 0 && current->age == newNode->age &&
                strcmp(current->gender, newNode->gender) == 0)
            {
                // Duplicate found
                count++;
                if(count>1)
                {
                     // Check if the node is already in the duplicates list
                    struct Node *dupCurrent = List.duplicates;
                    int duplicateExists = 0;
                    while (dupCurrent != NULL)
                    {
                        if (strcmp(dupCurrent->name, newNode->name) == 0 && dupCurrent->age == newNode->age &&
                            strcmp(dupCurrent->gender, newNode->gender) == 0)
                            {
                                duplicateExists = 1;
                                break;
                            }
                        dupCurrent = dupCurrent->next;
                    }

                // If not in duplicates list, add to duplicatelist
                if (!duplicateExists)
                {
                    add_duplicate_node(newNode);
                }

                break;

                }
                
            }
            current = current->next;
        }
}

  void timer_handler(union sigval sv)
    {
        sem_post(&to_display_thread_semaphore);
    }



void display_duplicates()
{
    pthread_mutex_lock(&mutex);
    struct Node *current = List.duplicates;
    if(current == NULL){
        printf("no duplicates..\n");
    }
    else{
    while (current != NULL)
    {
        printf("Duplicate: Name: %s, Age: %d, Gender: %s\n", current->name, current->age, current->gender);
        current = current->next;
    }
    }
    pthread_mutex_unlock(&mutex);
}


void *input_thread(void *arg)
{
    while (1)
    {
        struct Node *newNode = (struct Node *)malloc(sizeof(struct Node));

        printf("Enter name: ");
        scanf("%s", newNode->name);

        printf("Enter age: ");
        scanf("%d", &(newNode->age));

        printf("Enter gender: ");
        scanf("%s", newNode->gender);

        newNode->next = NULL;
        
        struct Message msg;
        msg.mtype = 1;
        msg.data = *newNode;
        msgsnd(msgid, &msg, sizeof(struct Node), 0);
        
        add_node(newNode);
        
   //     sem_post(&input_to_display_thread_semaphore);
   //     sem_post(&input_to_duplicate_thread_semaphore);
       sem_post(&to_display_thread_semaphore);
      
       
    }
}


void *display_thread(void *arg)
{
    struct sigevent sev;
    timer_t timer_id;

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timer_handler;

    struct itimerspec its;
    its.it_interval.tv_sec = 10;
    its.it_interval.tv_nsec = 0;
    its.it_value = its.it_interval;

    timer_create(CLOCK_REALTIME, &sev, &timer_id);
    timer_settime(timer_id, 0, &its, NULL);
    
    
    while (1)
    {
 //       sem_wait(&input_to_display_thread_semaphore);
//       sem_wait(&duplicate_to_display_thread_semaphore);

      sem_wait(&to_display_thread_semaphore);
     
        display_all();
       display_duplicates();

    }
}

void *check_duplicates_thread(void *arg)
{
    while (1)
    {
   //     sem_wait(&input_to_duplicate_thread_semaphore);
         struct Message msg;
        msgrcv(msgid, &msg, sizeof(struct Node), 1, 0);
        printf("Message received: Name: %s, Age: %d, Gender: %s\n",
               msg.data.name, msg.data.age, msg.data.gender);
        
        struct Node *added_Node = (struct Node *)malloc(sizeof(msg.data));
        *added_Node = msg.data;
        
        check_duplicates(added_Node);
   //     sem_post(&duplicate_to_display_thread_semaphore);
        sem_post(&to_display_thread_semaphore);
    }
}

int main()
{

    sem_init(&input_to_display_thread_semaphore, 0, 0);
    sem_init(&input_to_duplicate_thread_semaphore, 0, 0);
    sem_init(&duplicate_to_display_thread_semaphore, 0, 0);
    sem_init(&to_display_thread_semaphore, 0, 0);
    List.head = NULL;
    List.duplicates = NULL;

    key_t key = ftok("/tmp", 'Q');
    msgid = msgget(key, IPC_CREAT | 0666);

    pthread_t thread1, thread2, thread3;

    pthread_create(&thread1, NULL, &input_thread, NULL);
    pthread_create(&thread2, NULL, &display_thread, NULL);
    pthread_create(&thread3, NULL, &check_duplicates_thread, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

    return 0;
}
