//-Uncomment to compile with arduino support
//#define ARDUINO

//-------------------------------------
//-  Include files
//-------------------------------------
#include <termios.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <rtems.h>
#include <rtems/termiostypes.h>
#include <bsp.h>

#include "displayC.h"

//-------------------------------------
//-  Constants
//-------------------------------------
#define MSG_LEN 9
#define SLAVE_ADDR 0x8

//-------------------------------------
//-  Global Variables
//-------------------------------------
float speed = 0.0;
struct timespec time_msg = {0, 400000000};
int fd_serie = -1;
time_t last_mixer_change;
struct timespec t_cycle = {10, 0};
int light;
// group of binary variables defining the states
int brake = 0;
int gas = 0;
int mixer = 0;
int dark = 0;

//-------------------------------------
//-  Function: timespec_subtract
//      Subtracts x - y timespec structs
//-------------------------------------
int timespec_subtract(result, x, y)
struct timespec *result, *x, *y;
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_nsec < y->tv_nsec)
    {
        int nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
        y->tv_nsec -= 1000000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_nsec - y->tv_nsec > 1000000000)
    {
        int nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
        y->tv_nsec += 1000000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
       tv_nsec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_nsec = x->tv_nsec - y->tv_nsec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

//-------------------------------------
//-  Function: read_msg
//-------------------------------------
int read_msg(int fd, char *buffer, int max_size)
{
    char aux_buf[MSG_LEN + 1];
    int count = 0;
    char car_aux;

    // clear buffer and aux_buf
    memset(aux_buf, '\0', MSG_LEN + 1);
    memset(buffer, '\0', MSG_LEN + 1);

    while (1)
    {
        car_aux = '\0';
        read(fd_serie, &car_aux, 1);
        // skip if it is not valid character
        if (((car_aux < 'A') || (car_aux > 'Z')) &&
            ((car_aux < '0') || (car_aux > '9')) &&
            (car_aux != ':') && (car_aux != ' ') &&
            (car_aux != '\n') && (car_aux != '.') &&
            (car_aux != '%'))
        {
            continue;
        }
        // store the character
        aux_buf[count] = car_aux;

        // increment count in a circular way
        count = count + 1;
        if (count == MSG_LEN)
            count = 0;

        // if character is new_line return answer
        if (car_aux == '\n')
        {
            int first_part_size = strlen(&(aux_buf[count]));
            memcpy(buffer, &(aux_buf[count]), first_part_size);
            memcpy(&(buffer[first_part_size]), aux_buf, count);
            return 0;
        }
    }
    strncpy(buffer, "MSG: ERR\n", MSG_LEN);
    return 0;
}

//-------------------------------------
//-  Function: task_speed
//-------------------------------------
int task_speed()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    //--------------------------------
    //  request speed and display it
    //--------------------------------

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    // request speed
    strcpy(request, "SPD: REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    // display speed
    if (1 == sscanf(answer, "SPD:%f\n", &speed))
    {
        displaySpeed(speed);
    }
    return 0;
}
//-------------------------------------
//-  Function: task_brake
//-------------------------------------
int task_brake()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    if (speed <= 55.0)
    {
        strcpy(request, "BRK: CLR\n");
        brake = 0;
    }
    else
    {
        strcpy(request, "BRK: SET\n");
        brake = 1;
    }

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    if (0 == strcmp(answer, "BRK:  OK\n"))
    {
        displayBrake(brake);
        return 0;
    }
    else
    {
        return 1;
    }
}

//-------------------------------------
//-  Function: task_gas
//-------------------------------------
int task_gas()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    if (speed <= 55.0)
    {
        strcpy(request, "GAS: SET\n");
        gas = 1;
    }
    else
    {
        strcpy(request, "GAS: CLR\n");
        gas = 0;
    }

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    if (0 == strcmp(answer, "GAS:  OK\n"))
    {
        displayGas(gas);
        return 0;
    }
    else
    {
        return 1;
    }
}

//-------------------------------------
//-  Function: task_mix
//-------------------------------------
int task_mix()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    time_t current_time = time(NULL);

    double elapsed = difftime(current_time, last_mixer_change);
    if (elapsed > 30.0)
    {
        if (mixer == 0)
        {
            strcpy(request, "MIX: SET\n");
            mixer = 1;
        }
        else if (mixer == 1)
        {
            strcpy(request, "MIX: CLR\n");
            mixer = 0;
        }
    }

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    if (0 == strcmp(answer, "MIX:  OK\n"))
    {
        displayMix(mixer);
        last_mixer_change = current_time;
        return 0;
    }
    else
    {
        return 1;
    }
}

//-------------------------------------
//-  Function: task_slope
//-------------------------------------
int task_slope()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    //--------------------------------
    //  request slope and display it
    //--------------------------------

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    // request slope
    strcpy(request, "SLP: REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    // display slope
    if (0 == strcmp(answer, "SLP:DOWN\n"))
        displaySlope(-1);
    if (0 == strcmp(answer, "SLP:FLAT\n"))
        displaySlope(0);
    if (0 == strcmp(answer, "SLP:  UP\n"))
        displaySlope(1);

    return 0;
}

//-------------------------------------
//-  Function: read_light
//-------------------------------------
int task_light()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    //----------------------------------
    //  request light sensor information
    //----------------------------------

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    // request light
    strcpy(request, "LIT: REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    // display light
    char light_val[2];
    if (light < 10)
    {
        sprintf(light_val, "0%d", light);
    }
    else
    {
        sprintf(light_val, "%d", light);
    }

    if (1 == sscanf(answer, "LIT: %s%%\n", &light_val))
    {
        if (light < 50)
        {
            dark = 0;
        }
        else
        {
            dark = 1;
        }
        displayLightSensor(dark);
        return 0;
    }
    else
    {
        return 1;
    }
}

//-------------------------------------
//-  Function: task_lamp
//-------------------------------------
int task_lamp()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    if (dark == 0)
    {
        strcpy(request, "LAM: CLR\n");
    }
    else
    {
        strcpy(request, "LAM: SET\n");
    }
#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    // display lamp
    if (strcmp(answer, "LAM:  OK\n"))
    {
        displayLamps(dark);
        return 0;
    }
    else
    {
        return 1;
    }
}

//-------------------------------------
//-  Function: controller
//-------------------------------------
void *controller(void *arg)
{

    struct timespec t_init, t_end, t_diff;

    // Endless loop
    while (1)
    {
        if (clock_gettime(CLOCK_REALTIME, &t_init) < 0)
            fprintf(stderr, "Error while getting init time");

        // calling task of speed
        if (task_speed() != 0)
            printf("Error in task_speed");
        // calling task of slope
        if (task_slope() != 0)
            printf("Error in task_slope");
        // calling task of brake
        if (task_brake() != 0)
            printf("Error in task_brake");
        // calling task of gas
        if (task_gas() != 0)
            printf("Error in task_gas");
        // calling task of mixer
        if (task_mix() != 0)
            printf("Error in task_gas");

        if (clock_gettime(CLOCK_REALTIME, &t_end) < 0)
            fprintf(stderr, "Error while getting end time");

        // get the execution time
        timespec_subtract(&t_diff, &t_end, &t_init);
        // subtract the total cycle time and execution time
        timespec_subtract(&t_diff, &t_cycle, &t_diff);
        // sleep for the time remaining
        nanosleep(&t_diff, NULL);
    }
}

//-------------------------------------
//-  Function: Init
//-------------------------------------
rtems_task Init(rtems_task_argument ignored)
{
    pthread_t thread_ctrl;
    sigset_t alarm_sig;
    int i;

    /* Block all real time signals so they can be used for the timers.
       Note: this has to be done in main() before any threads are created
       so they all inherit the same mask. Doing it later is subject to
       race conditions */
    sigemptyset(&alarm_sig);
    for (i = SIGRTMIN; i <= SIGRTMAX; i++)
    {
        sigaddset(&alarm_sig, i);
    }
    sigprocmask(SIG_BLOCK, &alarm_sig, NULL);

    // init display
    displayInit(SIGRTMAX);

#if defined(ARDUINO)
    /* Open serial port */
    char serial_dev[] = "/dev/com1";
    fd_serie = open(serial_dev, O_RDWR);
    if (fd_serie < 0)
    {
        printf("open: error opening serial %s\n", serial_dev);
        exit(-1);
    }

    struct termios portSettings;
    speed_t speed = B9600;

    tcgetattr(fd_serie, &portSettings);
    cfsetispeed(&portSettings, speed);
    cfsetospeed(&portSettings, speed);
    cfmakeraw(&portSettings);
    tcsetattr(fd_serie, TCSANOW, &portSettings);
#endif

    /* Create first thread */
    pthread_create(&thread_ctrl, NULL, controller, NULL);
    pthread_join(thread_ctrl, NULL);
    exit(0);
}

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_MAXIMUM_TASKS 1
#define CONFIGURE_MAXIMUM_SEMAPHORES 10
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 30
#define CONFIGURE_MAXIMUM_DIRVER 10
#define CONFIGURE_MAXIMUM_POSIX_THREADS 2
#define CONFIGURE_MAXIMUM_POSIX_TIMERS 1

#define CONFIGURE_INIT
#include <rtems/confdefs.h>
