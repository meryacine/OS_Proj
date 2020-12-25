#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#define BSZ 0
#define NUM 1
#define ADD 2
#define REM 3
struct msgbuff
{
    long mtype;
    char mtext[70];
};

union Semun
{
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    ushort *array;         /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};
void cleanSegements(int bdataid, int bufferid, int msgq_id, int mutex)
{
    if (bdataid != -1)
    {
        shmctl(bdataid, IPC_RMID, (struct shmid_ds *)0);
    }
    if (bufferid != -1)
    {
        shmctl(bufferid, IPC_RMID, (struct shmid_ds *)0);
    }
    if (msgq_id != -1)
    {
        msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
    }
    if (mutex != -1)
    {
        semctl(mutex, 0, IPC_RMID);
    }
}
void down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}
int bdataid = -1, bufferid = -1, msgq_id = -1, mutex = -1;
void handler(int signum)
{
    cleanSegements(bdataid, bufferid, msgq_id, mutex);
    killpg(getpgrp(), SIGKILL);
}
int main()
{
    signal(SIGINT, handler);
    union Semun semun;
    int rec_val, send_val;
    bdataid = shmget(ftok("key", 300), sizeof(int) * 4, 0644);
    msgq_id = msgget(ftok("key", 302), 0666 | IPC_CREAT);
    mutex = semget(ftok("key", 303), 1, 0666 | IPC_CREAT);
    if (mutex == -1 || msgq_id == -1)
    {
        perror("Error in create");
        exit(-1);
    }
    if (bdataid == -1)
    {
        printf("No buffer found\n");
        cleanSegements(bdataid, bufferid, msgq_id, mutex);
        exit(-1);
    }
    ////////////////////// wait and attach the shared memory ////////////////////////////////
    void *shmaddr = shmat(bdataid, (void *)0, 0);
    if (*((int *)shmaddr) == -1)
    {
        perror("Error in attach in data writer");
        cleanSegements(bdataid, bufferid, msgq_id, mutex);
        exit(-1);
    }

    int *pdata = (int *)shmaddr;
    bufferid = shmget(ftok("key", 301), sizeof(int) * pdata[BSZ], 0644);
    void *shmaddr2 = shmat(bufferid, (void *)0, 0);
    if (*((int *)shmaddr2) == -1)
    {
        perror("Error in attach in buffer writer");
        cleanSegements(bdataid, bufferid, msgq_id, mutex);
        exit(-1);
    }
    int *pbuffer = (int *)shmaddr2;
    ////////////////////// ================================= ////////////////////////////////
    printf("Consumer is attached to the shared memory\n");
    struct msgbuff message;
    int i;
    while (1)
    {
        rec_val = msgrcv(msgq_id, &message, sizeof(message.mtext), 2, IPC_NOWAIT); // clear buffer
        down(mutex);
        if (pdata[NUM] < 0)
        {
            exit(1); /* underflow */
            cleanSegements(bdataid, bufferid, msgq_id, mutex);
        }
        else if (pdata[NUM] == 0) /* block if buffer empty */
        {
            printf("\nnext step : wait for producer to send ");
            scanf("%d", &i);
            up(mutex);
            rec_val = msgrcv(msgq_id, &message, sizeof(message.mtext), 2, !IPC_NOWAIT);
            if (rec_val == -1)
                perror("Error in receive");
            down(mutex);
        }
        printf("\nnext step : consume ");
        scanf("%d", &i);
        /* if executing here, buffer not empty so remove element */
        i = pbuffer[pdata[REM]];
        pdata[REM] = (pdata[REM] + 1) % pdata[BSZ];
        pdata[NUM]--;
        printf("Consume value %d\n", i);
        if (pdata[NUM] == pdata[BSZ] - 1) /* buffer was full */
        {
            char str[] = "produce";
            message.mtype = 1; /* arbitrary value */
            strcpy(message.mtext, str);
            printf("\nnext step : send to producer ");
            scanf("%d", &i);
            send_val = msgsnd(msgq_id, &message, sizeof(message.mtext), !IPC_NOWAIT);
            if (send_val == -1)
                perror("Errror in send");
        }
        up(mutex);
    }

    return 0;
}
