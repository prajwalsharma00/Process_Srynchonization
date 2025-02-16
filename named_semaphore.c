#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
/*
Create 3 child processes that try to access a shared resource (e.g., writing to a file).
Limit only 1 process to access the resource at a time using a named semaphore.*/
sem_t *named_semaphore;

#define OUTPUT_FILE "output.txt"
void semaphore_initialization()
{
    sem_unlink("/sem0");
    named_semaphore = sem_open("/sem0", O_CREAT | O_EXCL, 0600, 1);

    if (named_semaphore == SEM_FAILED)
    {
        fprintf(stderr, "Error on creation of the semaphore\n");
    }
}

void semaphore_clenaup()
{
    if (sem_close(named_semaphore) < 0)
    {
        fprintf(stderr, "Eroro on closing the semaphore\n");
    }
    if (sem_unlink("/sem0") < 0)
    {
        fprintf(stderr, "Error on joining the semaphore\n");
    }
}
int sem_timewait(sem_t *named, int t)
{
    struct timespec ts;
    ts.tv_nsec += 10000;
    time_t end = time(NULL) + t;

    while (time(NULL) < end)
    {
        if (sem_trywait(named) == 0)
        {
            return 0;
        }
        nanosleep(&ts, NULL);
    }
    errno = ETIMEDOUT;
    return -1;
}
void write_function(int filedescriptor)
{

    int s = sem_timewait(named_semaphore, 5);
    if (s == -1)
    {
        printf("cannot wait any more \n");
    }

    int random = rand() % 100;
    printf("%d \n", random);
    sleep(rand() % 15);
    char buffer[5];
    sprintf(buffer, "%d\n", random);
    fprintf(stdout, "%s", buffer);
    write(filedescriptor, buffer, strlen(buffer));
    sem_post(named_semaphore);
}
int main()
{
    srand(time(NULL));
    int fd = open(OUTPUT_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
    semaphore_initialization();
    pid_t child1 = fork();
    if (child1 == 0)
    {

        for (int i = 0; i < 10; i++)
        {
            write_function(fd);
        }
        exit(0);
    }
    pid_t child2 = fork();
    if (child2 == 0)
    {
        for (int i = 0; i < 10; i++)
        {
            write_function(fd);
        }
        exit(0);
    }
    pid_t child3 = fork();
    if (child3 == 0)
    {
        for (int i = 0; i < 10; i++)
        {
            write_function(fd);
        }
        exit(0);
    }

    // no to control teh exit of that we will use watipid(with -1 statsu  and wnohang )
    // it have 3 return type 0 when the child is still wiaing  have sucessfull reutrn -1 when some error and
    // int status;
    // pid_t childpid;
    // while (1)
    // {
    //     childpid = waitpid(-1, &status, WNOHANG);
    //     if (childpid > 0)
    //     {
    //         if (WIFEXITED(status))
    //         {
    //             fprintf(stdout, "Child %d have exited \n", childpid);
    //         }
    //         else if (WIFSIGNALED(status))
    //         {
    //             fprintf(stdout, "Child %d is terminated by signal %d  \n", childpid, WTERMSIG(status));
    //         }
    //     }

    //     if (childpid == 0)
    //     {
    //         printf("No child have exited yet \n");
    //         sleep(1);
    //     }
    //     else
    //     {
    //         break;
    //     }
    // }
    int status;
    while (waitpid(-1, &status, 0) > 0)
    {
        if (WIFEXITED(status))
        {
            printf("Child exited with status %d\n", WEXITSTATUS(status));
        }
    }
    close(fd);
    semaphore_clenaup();
}