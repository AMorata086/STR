// --------------------------------------
// Include files
// --------------------------------------
#include <string.h>
#include <stdio.h>
#include <Wire.h>

// --------------------------------------
// Global Constants
// --------------------------------------
#define SLAVE_ADDR 0x8
#define MESSAGE_SIZE 9

// --------------------------------------
// Global Variables
// --------------------------------------
double speed = 55.5;
bool request_received = false;
bool requested_answered = false;
char request[MESSAGE_SIZE + 1];
char answer[MESSAGE_SIZE + 1];

int DELAY_MS = 10;
int engine = 0; // -1 brake, 1 accelerator, 0 none
bool mix = false;
int slope = 0; // -1 up, 0 flat, 1 down

// --------------------------------------
// Function: comm_server
// --------------------------------------
int comm_server()
{
    static int count = 0;
    char car_aux;

    // If there were a received msg, send the processed answer or ERROR if none.
    // then reset for the next request.
    // NOTE: this requires that between two calls of com_server all possible
    //       answers have been processed.
    if (request_received)
    {
        // if there is an answer send it, else error
        if (requested_answered)
        {
            Serial.print(answer);
        }
        else
        {
            Serial.print("MSG: ERR\n");
        }
        // reset flags and buffers
        request_received = false;
        requested_answered = false;
        memset(request, '\0', MESSAGE_SIZE + 1);
        memset(answer, '\0', MESSAGE_SIZE + 1);
    }

    while (Serial.available())
    {
        // read one character
        car_aux = Serial.read();

        // skip if it is not a valid character
        if (((car_aux < 'A') || (car_aux > 'Z')) &&
            (car_aux != ':') && (car_aux != ' ') && (car_aux != '\n'))
        {
            continue;
        }

        // Store the character
        request[count] = car_aux;

        // If the last character is an enter or
        // There are 9th characters set an enter and finish.
        if ((request[count] == '\n') || (count == 8))
        {
            request[count] = '\n';
            count = 0;
            request_received = true;
            break;
        }

        // Increment the count
        count++;
    }
}

// --------------------------------------
// Function: speed_req
// --------------------------------------
int speed_req()
{
    // If there is a request not answered, check if this is the one
    if ((request_received) && (!requested_answered))
    {
        if (0 == strcmp("SPD: REQ\n", request)) // velocidad
        {
            // send the answer for speed request
            char num_str[5];
            dtostrf(speed, 4, 1, num_str);
            sprintf(answer, "SPD:%s\n", num_str);
            // set request as answered
            requested_answered = true;
        }
        // ACELERADOR
        else if (0 == strcmp("GAS: SET\n", request))
        {
            engine = 1;
            digitalWrite(13, HIGH);
            sprintf(answer, "GAS:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("GAS: CLR\n", request))
        {
            engine = 0;
            digitalWrite(13, LOW);
            sprintf(answer, "GAS:  OK\n");
            requested_answered = true;
        }
        // FRENO
        else if (0 == strcmp("BRK: SET\n", request))
        {
            engine = -1;
            digitalWrite(12, HIGH);
            sprintf(answer, "BRK:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("BRK: CLR\n", request))
        {
            engine = 0;
            digitalWrite(12, LOW);
            sprintf(answer, "BRK:  OK\n");
            requested_answered = true;
        }
        // MEZCLADOR
        else if (0 == strcmp("MIX: SET\n", request))
        {
            mix = true;
            digitalWrite(11, HIGH);
            sprintf(answer, "MIX:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("MIX: CLR\n", request))
        {
            mix = false;
            digitalWrite(11, LOW);
            sprintf(answer, "MIX:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("SLP: REQ\n", request)) // LEER PENDIENTE
        {
            if (slope == -1)
            {
                sprintf(answer, "SLP:  UP\n");
            }
            else if (slope == 1)
            {
                sprintf(answer, "SLP:DOWN\n");
            }
            else if (slope == 0)
            {
                sprintf(answer, "SLP:FLAT\n");
            }
            requested_answered = true;
        }
    }
    return 0;
}

void physics()
{
    // calculo de pendiente
    int up = digitalRead(9);
    int down = digitalRead(8);

    if (up && !down)
    {
        slope = 1;
    }
    if (!up && down)
    {
        slope = -1;
    }
    else
    {
        slope = 0;
    }

    // calculo de velocidad (pendiente)
    speed += (DELAY_MS * 0.25 / 1000) * slope;

    // calculo de velocidad (freno/acelerador)
    speed += (DELAY_MS * 0.5 / 1000) * engine;
}

// --------------------------------------
// Function: setup
// --------------------------------------
void setup()
{
    // Setup Serial Monitor
    Serial.begin(9600);

    pinMode(13, OUTPUT); // ACELERADOR
    pinMode(12, OUTPUT); // FRENO
    pinMode(11, OUTPUT); // MEZCLADO
    pinMode(10, OUTPUT); // VELOCIDAD
    pinMode(9, INPUT);   // PENDIENTE ARRIBA
    pinMode(8, INPUT);   // PENDIENTE ABAJO
    pinMode(8, OUTPUT);
}

// --------------------------------------
// Function: loop
// --------------------------------------
void loop()
{
    physics();
    comm_server();
    speed_req();
    delay(DELAY_MS);
}
