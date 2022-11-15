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

#include "displayD.h"

//-------------------------------------
//-  Constants
//-------------------------------------
#define MSG_LEN 9
#define SLAVE_ADDR 0x8
#define T_CYCLE 5
#define N_CYCLES_NORMAL 2
#define N_CYCLES_BRAKING 2
#define N_CYCLES_STOP 2
#define N_CYCLES_EMERGENCY 2

//-------------------------------------
//-  Global Variables
//-------------------------------------
float speed = 0.0;
struct timespec time_msg = {0, 400000000};
int fd_serie = -1;
time_t last_mixer_change;
struct timespec t_cycle = {T_CYCLE, 0};
int light;
int distance;
int mode = 0; // execution mode
// group of binary variables defining the states
int brake = 0;
int gas = 0;
int mixer = 0;
int dark = 0;
int stop = 0;

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
    else
    {
        mode = 3;
    }
    return 0;
}
//-------------------------------------
//-  Function: task_brake_normal
//-------------------------------------
int task_brake_normal()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    if (speed <= 55.0 && speed >= 0)
    {
        strcpy(request, "BRK: CLR\n");
        brake = 0;
    }
    else if (speed > 55.0 && speed <= 70.0)
    {
        strcpy(request, "BRK: SET\n");
        brake = 1;
    }
    else
    {
        mode = 3;
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
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
}

//-------------------------------------
//-  Function: task_brake_braking
//-------------------------------------
int task_brake_braking()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    if (speed <= 2.5 && speed >= 0)
    {
        strcpy(request, "BRK: CLR\n");
        brake = 0;
    }
    else if (speed > 2.5)
    {
        strcpy(request, "BRK: SET\n");
        brake = 1;
    }
    else
    {
        mode = 3;
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
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
}

//-------------------------------------
//-  Function: task_brake_emergency
//-------------------------------------
int task_brake_emergency()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    strcpy(request, "BRK: SET\n");
    brake = 1;

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
    }
    return 0;
}

//-------------------------------------
//-  Function: task_gas_normal
//-------------------------------------
int task_gas_normal()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    if (speed >= 0 && speed <= 55.0)
    {
        strcpy(request, "GAS: SET\n");
        gas = 1;
    }
    else if (speed > 55.0 && speed <= 70.0)
    {
        strcpy(request, "GAS: CLR\n");
        gas = 0;
    }
    else
    {
        mode = 3;
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
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
}

//-------------------------------------
//-  Function: task_gas_braking
//-------------------------------------
int task_gas_braking()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    if (speed >= 0 && speed <= 2.5)
    {
        strcpy(request, "GAS: SET\n");
        gas = 1;
    }
    else if (speed > 2.5)
    {
        strcpy(request, "GAS: CLR\n");
        gas = 0;
    }
    else
    {
        mode = 3;
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
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
}

//-------------------------------------
//-  Function: task_gas_emergency
//-------------------------------------
int task_gas_emergency()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    strcpy(request, "GAS: CLR\n");
    gas = 0;

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
    }
    return 0;
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
    else
    {
        return 0;
    }
    /*
    else if (elapsed >= 60 || elapsed < 0)
    {
        mode = 3;
    }
    */

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
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
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

    if (1 == sscanf(answer, "LIT:%d\n", &light))
    {
        if (light < 50)
        {
            dark = 1;
        }
        else
        {
            dark = 0;
        }
        displayLightSensor(dark);
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
}

//-------------------------------------
//-  Function: task_lamp_normal
//-------------------------------------
int task_lamp_normal()
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
    if (0 == strcmp(answer, "LAM:  OK\n"))
    {
        displayLamps(dark);
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
}

//-------------------------------------
//-  Function: task_lamp_braking
//-------------------------------------
int task_lamp_braking()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    strcpy(request, "LAM: SET\n");

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
    if (0 == strcmp(answer, "LAM:  OK\n"))
    {
        displayLamps(1);
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
}

//-------------------------------------
//-  Function: task_lamp_stop
//-------------------------------------
int task_lamp_stop()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    strcpy(request, "LAM: SET\n");

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
    if (0 == strcmp(answer, "LAM:  OK\n"))
    {
        displayLamps(1);
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
}

//-------------------------------------
//-  Function: task_lamp_emergency
//-------------------------------------
int task_lamp_emergency()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    strcpy(request, "LAM: SET\n");

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
    if (0 == strcmp(answer, "LAM:  OK\n"))
    {
        displayLamps(1);
    }
    return 0;
}

//-------------------------------------
//-  Function: task_distance
//-------------------------------------
int task_distance()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    //--------------------------------
    //  request distance and display it
    //--------------------------------

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    // request distance
    strcpy(request, "DS:  REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    // display distance
    if (1 == sscanf(answer, "DS:%d\n", &distance))
    {

        switch (mode)
        {
        case 0:
            if (distance < 11000 && distance > 0)
            {
                mode = 1; // we switch to break mode
            }
            break;

        case 1:
            if (distance <= 0 && speed <= 10)
            {
                mode = 2; // we switch to stop mode
                distance = 0;
            }
            break;
        }
    }
    displayDistance(distance);
    return 0;
}

//----------------------------------
//  request stop information
//----------------------------------
int task_stop()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    // request stop
    strcpy(request, "STP: REQ\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    // display stop
    if (0 == strcmp(answer, "STP:  GO\n"))
    {
        stop = 0;
        mode = 0; // we switch to normal mode
        displayStop(stop);
    }
    else if (0 == strcmp(answer, "STP:STOP\n"))
    {
        stop = 1;
        displayStop(stop);
    }
    // else if (0 == strcmp(answer, "MSG: ERR\n"))
    // {
    //     mode = 3;
    // }
    return 0;
}

//-------------------------------------
//-  Function: task_emergency
//-------------------------------------
int task_emergency()
{
    char request[MSG_LEN + 1];
    char answer[MSG_LEN + 1];

    // clear request and answer
    memset(request, '\0', MSG_LEN + 1);
    memset(answer, '\0', MSG_LEN + 1);

    // request emergency
    strcpy(request, "ERR: SET\n");

#if defined(ARDUINO)
    // use UART serial module
    write(fd_serie, request, MSG_LEN);
    nanosleep(&time_msg, NULL);
    read_msg(fd_serie, answer, MSG_LEN);
#else
    // Use the simulator
    simulator(request, answer);
#endif

    // display stop
    if (0 == strcmp(answer, "ERR:  OK\n"))
    {
        mode = 3;
    }
    return 0;
}

//-------------------------------------
//-  Function: controller
//-------------------------------------
void *controller(void *arg)
{

    struct timespec t_init, t_end, t_diff;
    int cycle_counter_normal = 0;
    int cycle_counter_braking = 0;
    int cycle_counter_stop = 0;
    int cycle_counter_emergency = 0;

    // Endless loop
    while (1)
    {
        if (clock_gettime(CLOCK_REALTIME, &t_init) < 0)
            fprintf(stderr, "Error while getting init time");

        switch (mode)
        {
        case 0: // normal mode
            /* tasks that execute every 5 seconds */
            // calling task of light sensor
            if (task_light() != 0)
                fprintf(stderr, "Error in task_light\n");
            // calling task of lamp
            if (task_lamp_normal() != 0)
                fprintf(stderr, "Error in task_lamp\n");
            switch (cycle_counter_normal)
            {
            case 0:
                /* tasks that execute every 10 seconds */
                // calling task of speed
                if (task_speed() != 0)
                    fprintf(stderr, "Error in task_speed\n");
                // calling task of slope
                if (task_slope() != 0)
                    fprintf(stderr, "Error in task_slope\n");
                // calling task of distance
                if (task_distance() != 0)
                    fprintf(stderr, "Error in task_distance\n");
                cycle_counter_normal = (cycle_counter_normal + 1) % N_CYCLES_NORMAL;
                break;
            case 1:
                /* tasks that execute every 10 seconds */
                // calling task of brake
                if (task_brake_normal() != 0)
                    fprintf(stderr, "Error in task_brake\n");
                // calling task of gas
                if (task_gas_normal() != 0)
                    fprintf(stderr, "Error in task_gas\n");
                // calling task of mixer
                if (task_mix() != 0)
                    fprintf(stderr, "Error in task_gas\n");
                cycle_counter_normal = (cycle_counter_normal + 1) % N_CYCLES_NORMAL;
                break;
            }
            break;
        case 1: // braking mode
            /* tasks that execute every 5 seconds */
            // calling task of speed
            if (task_speed() != 0)
                fprintf(stderr, "Error in task_speed\n");
            // calling task of brake
            if (task_brake_braking() != 0)
                fprintf(stderr, "Error in task_brake\n");
            // calling task of gas
            if (task_gas_braking() != 0)
                fprintf(stderr, "Error in task_gas\n");
            switch (cycle_counter_braking)
            {
            case 0:
                /* tasks that execute every 10 seconds */
                if (task_distance() != 0)
                    fprintf(stderr, "Error in task_distance\n");
                if (task_slope() != 0)
                    fprintf(stderr, "Error in task_slope\n");
                cycle_counter_braking = (cycle_counter_braking + 1) % N_CYCLES_BRAKING;
                break;
            case 1:
                /* tasks that execute every 10 seconds */
                if (task_lamp_braking() != 0)
                    fprintf(stderr, "Error in task_lamp_braking\n");
                if (task_mix() != 0)
                    fprintf(stderr, "Error in task_mix\n");
                cycle_counter_braking = (cycle_counter_braking + 1) % N_CYCLES_BRAKING;
                break;
            }
            break;
        case 2: // stop mode
            /* tasks that execute every 5 seconds */
            if (task_stop() != 0)
                fprintf(stderr, "Error in task_stop\n");
            switch (cycle_counter_stop)
            {
            case 0:
                /* tasks that execute every 10 seconds */
                if (task_mix() != 0)
                    fprintf(stderr, "Error in task_mix\n");
                cycle_counter_stop = (cycle_counter_stop + 1) % N_CYCLES_STOP;
                break;
            case 1:
                /* tasks that execute every 10 seconds */
                if (task_lamp_stop() != 0)
                    fprintf(stderr, "Error in task_lamp_stop\n");
                cycle_counter_stop = (cycle_counter_stop + 1) % N_CYCLES_STOP;
                break;
            }
            break;
        case 3: // emergency mode
            /* tasks that execute every 5 seconds */
            if (task_lamp_emergency() != 0)
                fprintf(stderr, "Error in task_lamp_emergency\n");
            switch (cycle_counter_emergency)
            {
            case 0:
                /* tasks that execute every 10 seconds */
                if (task_emergency() != 0)
                    fprintf(stderr, "Error in task_emergency\n");
                if (task_speed() != 0)
                    fprintf(stderr, "Error in task_speed\n");
                if (task_slope() != 0)
                    fprintf(stderr, "Error in task_slope\n");
                cycle_counter_emergency = (cycle_counter_emergency + 1) % N_CYCLES_EMERGENCY;
                break;
            case 1:
                /* tasks that execute every 10 seconds */
                if (task_brake_emergency() != 0)
                    fprintf(stderr, "Error in task_brake_emergency\n");
                if (task_gas_emergency() != 0)
                    fprintf(stderr, "Error in task_gas_emergency\n");
                if (task_mix() != 0)
                    fprintf(stderr, "Error in task_mix\n");
                cycle_counter_emergency = (cycle_counter_emergency + 1) % N_CYCLES_EMERGENCY;
                break;
            }
            break;
        }

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