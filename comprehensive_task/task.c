#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#define TABLES 20
#define CUSTOMER 30
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int threads_number;
atomic_int active_customer = 0;
pthread_t customers[CUSTOMER];
time_t pthreads_time[CUSTOMER];

struct resturant_detail
{
    atomic_int table_left;
    sem_t *semaphore;
};
struct threads_specific_data
{
    struct resturant_detail *data;
    int thread_number;
};

void logger(char *log)

{
    pthread_mutex_lock(&mutex);
    FILE *filepd = fopen("resturant.txt", "a+");
    if (!filepd)
    {
        perror("File is not created proeperly\n");
    }
    char buffer[256];
    char t[50];
    time_t raw_time = time(NULL);
    struct tm *tn;
    tn = localtime(&raw_time);
    strftime(t, sizeof(t), "%H:%M:%S", tn);
    snprintf(buffer, sizeof(buffer), "%s : %s \n", t, log);
    fputs(buffer, filepd);
    fclose(filepd);
    pthread_mutex_unlock(&mutex);
}
int sem_timewait(sem_t *semaphore)
{
    time_t starting_time = time(NULL);
    while ((time(NULL) - starting_time) < 4)
    {
        if (sem_trywait(semaphore) == 0)
        {
            return 0;
        }
    }

    return -1;
}
void cleanup(void *args)
{
    sem_t *semaphore = (sem_t *)args;

    sem_post(semaphore);
}
void *customer_task(void *args)
{
    struct threads_specific_data *d = (struct threads_specific_data *)args;
    struct resturant_detail *details = d->data;
    printf("------------threads number is %d \n", d->thread_number);

    if (sem_timewait(details->semaphore) == -1)
    {
        char buff[200];
        sprintf(buff, "Customer  %d was waiting too long so left  ", d->thread_number);
        logger(buff);
    };
    threads_number = d->thread_number;
    atomic_fetch_add(&active_customer, 1);

    int sl = rand() % 10;
    printf("sleeping for %d sec   \n thread number %d\n ", sl, d->thread_number);

    if (atomic_load(&details->table_left) == 0)
    {
        logger("RESTURANT WAS FULL: CUSTOMER LEFT");
        atomic_fetch_sub(&active_customer, 1);
        free(d);
        sem_post(details->semaphore);
        return NULL;
    }
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    sleep(sl);
    pthread_cleanup_push(cleanup, details->semaphore);

    atomic_fetch_sub(&details->table_left, 1);
    char buff[200];
    sprintf(buff, "Customer  %d ahve taken the seat: ", d->thread_number);
    logger(buff);

    pthread_cleanup_pop(1);
    sem_post(details->semaphore);
    free(d);

    return NULL;
}
void *check(void *args)
{

    while ((atomic_load(&active_customer)) > 0)
    {
        for (int i = 0; i < CUSTOMER; i++)
        {
            time_t current_time = time(NULL);
            if ((current_time - pthreads_time[i]) > 5)
            {
                char buff[200];
                sprintf(buff, "CUSTOMER %d HAVE TAKEN MORE TIME THAN ALLOCATED SO THEY ARE BEING KICKED OUT \n", i);
                logger(buff);
                pthread_cancel(customers[i]);
            }
        }
    }
    return NULL;
}

int main()
{
    sem_t *semaphore = sem_open("mysemaphore", O_CREAT, 0600, 1);
    if (semaphore == SEM_FAILED)
    {
        printf("Error in creating the semaphore");
    }

    int shared_struct = shm_open("struct", O_CREAT | O_RDWR, 0700);
    if (shared_struct < 0)
    {
        printf("error in creaing the shared memroy\n");
    }
    if ((ftruncate(shared_struct, sizeof(struct resturant_detail))) < 0)
    {
        printf("error in truncatin the shared memory \n");
    };
    struct resturant_detail *shared_details = mmap(0, sizeof(*shared_details), PROT_READ | PROT_WRITE, MAP_SHARED, shared_struct, 0);
    pid_t customer = fork();
    if (customer == 0)
    {
        shared_details->semaphore = semaphore;
        atomic_init(&shared_details->table_left, TABLES);

        for (int i = 0; i < CUSTOMER; i++)
        {
            struct threads_specific_data *datas = malloc(sizeof(*datas));
            datas->data = shared_details;
            datas->thread_number = i;
            pthreads_time[i] = time(NULL);
            if ((pthread_create(&customers[i], NULL, customer_task, &datas)) != 0)
            {
                printf("Error in creating the threads \n");
            }
        }
        for (int i = 0; i < CUSTOMER; i++)
        {
            if ((pthread_join(customers[i], NULL)) != 0)
            {
                printf("error in joiining the threads");
            }
        }

        printf("helloooooo \n");
        exit(0);
    }
    pthread_t checker;
    if ((pthread_create(&checker, NULL, check, NULL)) != 0)
    {
        printf("Error in creating the thread \n");
    }
    pthread_join(checker, NULL);

    wait(NULL);

    shm_unlink("struct");
    munmap(shared_details, sizeof(*shared_details));
}
