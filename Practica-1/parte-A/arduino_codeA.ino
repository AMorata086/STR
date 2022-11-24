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
double speed = 55.5; // 55.5
bool request_received = false;
bool requested_answered = false;
char request[MESSAGE_SIZE + 1];
char answer[MESSAGE_SIZE + 1];
int count = 0;

int DELAY_MS = 200;
int brake = 0;
int gas = 0;
bool mix = false;
int slope = 0; // -1 up, 0 flat, 1 down


// --------------------------------------
// Function: comm_server
// --------------------------------------
int comm_server()
{
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
        count = 0;
    }

    while (Serial.available())
    {
        // read one character
        car_aux = Serial.read();
        //Serial.println(car_aux);
        // skip if it is not a valid character
        if (((car_aux < 'A') || (car_aux > 'Z')) &&
            (car_aux != ':') && (car_aux != ' ') && (car_aux != '\n'))
        {
            continue;
        }

        // Store the character
        request[count] = car_aux;

        // Serial.println(car_aux);
        // Serial.println(count);
        // If the last character is an enter finish
        if (request[count] == '\n') {
            request[count] = car_aux;
            request_received = true;
            break;
        }
        /*
        else if (count == 7) {
            request[count] = car_aux;

            request[count + 1 ] = '\n';
            request_received = true;
            break;
        }*/

        count++; // Increment the count
    }
    // while(Serial.available()){Serial.read();}// FLUSH
}

// --------------------------------------
// Function: speed_req
// --------------------------------------
int speed_req()
{
    // If there is a request not answered, check if this is the one
    if ((request_received) && (!requested_answered))
    {
        //Serial.println(request);
        if (0 == strcmp("SPD: REQ\n", request)) // LEER VELOCIDAD
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
            gas = 1;
            brake = 0;
            sprintf(answer, "GAS:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("GAS: CLR\n", request))
        {
            gas = 0;
            sprintf(answer, "GAS:  OK\n");
            requested_answered = true;
        }
        // FRENO
        else if (0 == strcmp("BRK: SET\n", request))
        {
            brake = 1;
            gas = 0;
            sprintf(answer, "BRK:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("BRK: CLR\n", request))
        {
            brake = 0;
            sprintf(answer, "BRK:  OK\n");
            requested_answered = true;
        }
        // MEZCLADOR
        else if (0 == strcmp("MIX: SET\n", request))
        {
            mix = true;
            sprintf(answer, "MIX:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("MIX: CLR\n", request))
        {
            mix = false;
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

        digitalWrite(11, mix);
        digitalWrite(13, gas);
        digitalWrite(12, brake);
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
        slope = -1;
    }
    else if (!up && down)
    {
        slope = 1;
    }
    else
    {
        slope = 0;
    }
    // Serial.println(slope);

    // calculo de velocidad (pendiente)
    speed += (DELAY_MS * 0.25 / 1000) * slope;

    // calculo de velocidad (freno/acelerador)
    if (gas) {
        speed += (DELAY_MS * 0.5 / 1000);
    }
    if (brake) { // El freno frena velocidad positiva y negativa
        if (speed > (DELAY_MS * 0.5 / 1000)) {
            speed -= (DELAY_MS * 0.5 / 1000);
        } else if (speed < -(DELAY_MS * 0.5 / 1000)){
            speed += (DELAY_MS * 0.5 / 1000);
        } else {
            speed = 0;
        }
    }

    // Calculo del brillo del led de velocidad
    int bright;
    if (speed <= 40){
      bright = 0;
    } else if (speed >= 70) {
      bright = 255;  
    } else {
      bright = (speed - 40) * 255 / 30;
    }

    analogWrite(10, bright);
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
}

// --------------------------------------
// Function: loop
// --------------------------------------
void loop()
{
    unsigned long start = millis();
    physics();
    comm_server();
    speed_req();
    unsigned long elapsed = millis() - start;
    delay(DELAY_MS - elapsed);
}
