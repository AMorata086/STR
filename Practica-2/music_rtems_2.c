/**********************************************************
 *  INCLUDES
 *********************************************************/
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <rtems.h>
#include <rtems/termiostypes.h>
#include <rtems/shell.h>
#include <rtems/untar.h>
#include <bsp.h>

/**********************************************************
 *  CONSTANTS PART 1
 *********************************************************/
#define NSEC_PER_SEC 1000000000UL

#define DEV_NAME "/dev/com1"
#define FILE_NAME "/let_it_be_1bit.raw"

#define PERIOD_TASK_SEC    0            /* Period of Task */
#define PERIOD_TASK_NSEC  64000000
#define PERIOD_TASK2_SEC    2            /* Period of Task */
#define PERIOD_TASK2_NSEC  0
#define PERIOD_TASK3_SEC    5            /* Period of Task */
#define PERIOD_TASK3_NSEC  0 /* Period of Task */
#define SEND_SIZE 256                /* BYTES */

#define TARFILE_START _binary_tarfile_start
#define TARFILE_SIZE _binary_tarfile_size

#define SLAVE_ADDR 0x8

/**********************************************************
 *  GLOBALS
 *********************************************************/
extern int _binary_tarfile_start;
extern int _binary_tarfile_size;
int paused = 0;
pthread_mutex_t bufferM;
char toPause = '1';

/**********************************************************
 * Function: diffTime
 *********************************************************/
void diffTime(struct timespec end,
              struct timespec start,
              struct timespec *diff)
{
    if (end.tv_nsec < start.tv_nsec) {
        diff->tv_nsec = NSEC_PER_SEC - start.tv_nsec + end.tv_nsec;
        diff->tv_sec = end.tv_sec - (start.tv_sec+1);
    } else {
        diff->tv_nsec = end.tv_nsec - start.tv_nsec;
        diff->tv_sec = end.tv_sec - start.tv_sec;
    }
}

/**********************************************************
 * Function: addTime
 *********************************************************/
void addTime(struct timespec end,
              struct timespec start,
              struct timespec *add)
{
    unsigned long aux;
    aux = start.tv_nsec + end.tv_nsec;
    add->tv_sec = start.tv_sec + end.tv_sec +
                  (aux / NSEC_PER_SEC);
    add->tv_nsec = aux % NSEC_PER_SEC;
}

/**********************************************************
 * Function: compTime
 *********************************************************/
int compTime(struct timespec t1,
              struct timespec t2)
{
    if (t1.tv_sec == t2.tv_sec) {
        if (t1.tv_nsec == t2.tv_nsec) {
            return (0);
        } else if (t1.tv_nsec > t2.tv_nsec) {
            return (1);
        } else if (t1.tv_sec < t2.tv_sec) {
            return (-1);
        }
    } else if (t1.tv_sec > t2.tv_sec) {
        return (1);
    } else if (t1.tv_sec < t2.tv_sec) {
        return (-1);
    }
    return (0);
}
/**********************************************************
 * Function: read_keyb
 *********************************************************/
void *read_keyb(){
	char playPause;
	while(0 >= scanf("%c",&playPause)){
		if (playPause == '0'){
			toPause = '1';
		}
		else if (playPause == '1'){
			toPause = '0';
		}
	}
}

/**********************************************************
 * Function: read_music
 *********************************************************/
void *read_music()
{
    struct timespec start,end,diff,cycle;
    unsigned char buf[SEND_SIZE];
    int fd_file = -1;
    int fd_serie = -1;
    int ret = 0;
    struct termios portSettings;
    speed_t speed=B115200;
    tcgetattr(fd_serie, &portSettings);
    cfsetispeed(&portSettings, speed);
    cfsetospeed(&portSettings, speed);
    cfmakeraw(&portSettings);
    tcsetattr(fd_serie, TCSANOW, &portSettings);
    /* Open music file */
    printf("open file %s begin\n",FILE_NAME);
    fd_file = open (FILE_NAME, O_RDWR);
    if (fd_file < 0) {
        perror("open: error opening file \n");
        exit(-1);
    }
    int zero_buffer[SEND_SIZE];
    for(int i = 0; i< SEND_SIZE; i++)
    {
    	zero_buffer[i] = 0;
    }
    // loading cycle time
    cycle.tv_sec=PERIOD_TASK_SEC;
    cycle.tv_nsec=PERIOD_TASK_NSEC;

    clock_gettime(CLOCK_REALTIME,&start);
    while (1) {
    	pthread_mutex_lock(&bufferM);
    	int isPaused = paused;
        // read from music file
    	if (isPaused)
    	{
        	ret=read(fd_file,buf,SEND_SIZE);
        	if (ret < 0) {
        		printf("read: error reading file\n");
        		exit(-1);
        	}
        
        	// write on the serial/I2C port
        	ret=write(fd_serie,buf,SEND_SIZE);
        	if (ret < 0) {
        		printf("write: error writting serial\n");
        		exit(-1);
        	}
    	}
    	else{
    		//No sound is playing so we send 0
    		ret = write(fd_serie,zero_buffer, SEND_SIZE);
    		if (ret < 0) {
    		     printf("write: error writting serial\n");
    		     exit(-1);
    		 }
    	}
    	pthread_mutex_unlock(&bufferM);
        // get end time, calculate lapso and sleep
        clock_gettime(CLOCK_REALTIME,&end);
        diffTime(end,start,&diff);
        if (0 >= compTime(cycle,diff)) {
            printf("ERROR: lasted long than the cycle\n");
            exit(-1);
        }
        diffTime(cycle,diff,&diff);
        nanosleep(&diff,NULL);
        addTime(start,cycle,&start);
    }

}


/**********************************************************
 * Function: task_pause
 *********************************************************/
void *task_pause()
{
    //change code
    struct timespec start_task3, end_task3, diff_task3, task3_cycle;
    task3_cycle.tv_nsec = PERIOD_TASK3_NSEC; // 0ms
    task3_cycle.tv_sec = PERIOD_TASK3_SEC;   //5s

    clock_gettime(CLOCK_REALTIME, &start_task3);
    while (1)
    {
    	pthread_mutex_lock(&bufferM);
        int localPaused = paused;
        if (localPaused == 0)
            printf("La musica se está reproduciendo\n");
        else
            printf("La musica está en pausa\n");

        pthread_mutex_unlock(&bufferM);

        clock_gettime(CLOCK_REALTIME, &end_task3);
        diffTime(end_task3, start_task3, &diff_task3);
        if (0 >= compTime(task3_cycle, diff_task3))
        {
            printf("ERROR: lasted long than the cycle on task 3\n");
            exit(-1);
        }
        diffTime(task3_cycle, diff_task3, &diff_task3);
        nanosleep(&diff_task3, NULL);
        addTime(start_task3, task3_cycle, &start_task3);
    }


}

/**********************************************************
 * Function: status_music
 *********************************************************/
void *status_music()
{
    struct timespec start_task2, end_task2, diff_task2, task2_cycle;
    task2_cycle.tv_nsec = PERIOD_TASK2_NSEC; // 0ms
    task2_cycle.tv_sec = PERIOD_TASK2_SEC;   //5s

    clock_gettime(CLOCK_REALTIME, &start_task2);
    while (1)
    {
        pthread_mutex_lock(&bufferM);
        if(toPause == '0'){
        	paused = 1;
        }else if(toPause == '1'){
        	paused = 0;
        }
        pthread_mutex_unlock(&bufferM);

        clock_gettime(CLOCK_REALTIME, &end_task2);
        diffTime(end_task2, start_task2, &diff_task2);
        if (0 >= compTime(task2_cycle, diff_task2))
        {
            printf("ERROR: lasted long than the cycle on task 2\n");
            exit(-1);
        }
        diffTime(task2_cycle, diff_task2, &diff_task2);
        nanosleep(&diff_task2, NULL);
        addTime(start_task2, task2_cycle, &start_task2);
    }
}

/*****************************************************************************
 * Function: Init()
 *****************************************************************************/
rtems_task Init (rtems_task_argument ignored)
{
    printf("Populating Root file system from TAR file.\n");
    Untar_FromMemory((unsigned char *)(&TARFILE_START),
                     (unsigned long)&TARFILE_SIZE);

    rtems_shell_init("SHLL", RTEMS_MINIMUM_STACK_SIZE * 4,
                     100, "/dev/foobar", false, true, NULL);

    /*------------THREADS----------*/
     pthread_t play_status;
     pthread_t status_music_thread;
     pthread_t music_file_thread;
     pthread_t scan_keyb;
     pthread_attr_t attr_tty;
     pthread_attr_t attr_status_music;
     pthread_attr_t attr_music_file;
     pthread_mutexattr_t m_attr;
     pthread_mutexattr_setprotocol(&m_attr, PTHREAD_PRIO_INHERIT);
     pthread_mutex_init(&bufferM, &m_attr);
     pthread_attr_init(&attr_tty);
     pthread_attr_init(&attr_status_music);
     pthread_attr_init(&attr_music_file);
     pthread_attr_setdetachstate(&attr_tty, PTHREAD_CREATE_JOINABLE);
     pthread_attr_setdetachstate(&attr_status_music, PTHREAD_CREATE_JOINABLE);
     pthread_attr_setdetachstate(&attr_music_file, PTHREAD_CREATE_JOINABLE);
     pthread_attr_setscope(&attr_tty, PTHREAD_SCOPE_SYSTEM);
     pthread_attr_setscope(&attr_status_music, PTHREAD_SCOPE_SYSTEM);
     pthread_attr_setscope(&attr_music_file, PTHREAD_SCOPE_SYSTEM);
     pthread_attr_setinheritsched(&attr_tty, PTHREAD_EXPLICIT_SCHED);
     pthread_attr_setinheritsched(&attr_status_music, PTHREAD_EXPLICIT_SCHED);
     pthread_attr_setinheritsched(&attr_music_file, PTHREAD_EXPLICIT_SCHED);
     pthread_attr_setschedpolicy(&attr_tty, SCHED_FIFO);
     pthread_attr_setschedpolicy(&attr_status_music, SCHED_FIFO);
     pthread_attr_setschedpolicy(&attr_music_file, SCHED_FIFO);
     struct sched_param tty_param;
     struct sched_param status_music_param;
     struct sched_param music_file_param;
     music_file_param.sched_priority = 3;
     tty_param.sched_priority = 2;
     status_music_param.sched_priority = 1;
     pthread_attr_setschedparam(&attr_tty, &tty_param);
     pthread_attr_setschedparam(&attr_status_music, &status_music_param);
     pthread_attr_setschedparam(&attr_music_file, &music_file_param);
     pthread_create(&music_file_thread, &attr_music_file, read_music, NULL);
     pthread_create(&play_status, &attr_tty, task_pause, NULL);
     phtread_create(&scan_keyb, NULL, read_keyb, NULL);
     pthread_create(&status_music_thread, &attr_status_music,status_music, NULL);
     pthread_join(play_status, NULL);
     pthread_join(status_music_thread, NULL);
     pthread_join(music_file_thread, NULL);
     pthread_attr_destroy(&attr_tty);
     pthread_attr_destroy(&attr_status_music);
     pthread_attr_destroy(&attr_music_file);
     pthread_exit(0);
    exit(0);

}

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 20
#define CONFIGURE_UNIFIED_WORK_AREAS
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_INIT
#include <rtems/confdefs.h>
