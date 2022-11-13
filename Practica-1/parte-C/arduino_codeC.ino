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
double speed = 0; // 55.5
bool request_received = false;
bool requested_answered = false;
char request[MESSAGE_SIZE + 1];
char answer[MESSAGE_SIZE + 1];
int count = 0;

int DELAY_MS = 200;
bool brake = false;
bool gas = false;
bool mix = false;
int slope = 0; // -1 up, 0 flat, 1 down

double light = 0;
bool lamp = false;

long distance = 0;
bool select = false;

/**
 * MODO
 * 0 - normal / seleccion de distancia
 * 1 - acercamiento al deposito
 * 2 - modo de parada
 * 3 - modo selección en parada
*/
int mode = 0;

void next_mode(){
    mode = (mode + 1)%4;
}

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
        // Serial.println(car_aux);
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
        // If the last character is an enter or
        // There are 9th characters set an enter and finish.
        if (request[count] == '\n') {
            request_received = true;
            break;
        }
        else if (count == 7) {
            request[count] = car_aux;

            request[count + 1 ] = '\n';
            request_received = true;
            break;
        }
        /*
        if ((request[count] == '\n') || (count == 7))
        {
            request[count] = car_aux;
            request[count + 1] = '\n';
            request_received = true;
            break;
        }*/

        count++; // Increment the count
    }
    while(Serial.available()){Serial.read();}
}

// --------------------------------------
// Function: speed_req
// --------------------------------------
int speed_req()
{
    // If there is a request not answered, check if this is the one
    if ((request_received) && (!requested_answered))
    {
        // Serial.println(request);
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
            gas = true;
            brake = false;
            sprintf(answer, "GAS:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("GAS: CLR\n", request))
        {
            gas = false;
            sprintf(answer, "GAS:  OK\n");
            requested_answered = true;
        }
        // FRENO
        else if (0 == strcmp("BRK: SET\n", request))
        {
            brake = true;
            gas = false;
            sprintf(answer, "BRK:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("BRK: CLR\n", request))
        {
            brake = false;
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
        else if (0 == strcmp("LIT: REQ\n", request)) // SENSOR LUZ
        {
            char num_str[2];
            dtostrf(light, 2, 0, num_str);
            if (light < 10) { 
                sprintf(num_str, "0%d", (int) light);
             }

            sprintf(answer, "LIT: %s%%\n", num_str);
            requested_answered = true;
        }
        // LUZ FAROS
        else if (0 == strcmp("LAM: SET\n", request))
        {
            lamp = true;
            sprintf(answer, "LAM:  OK\n");
            requested_answered = true;
        }
        else if (0 == strcmp("LAM: CLR\n", request))
        {
            lamp = false;
            sprintf(answer, "LAM:  OK\n");
            requested_answered = true;
        }
        // MOVIMIENTO
        else if (0 == strcmp("STP: REQ\n", request))
        {
            if (mode == 0) {
                sprintf(answer, "STP:  GO\n");
            } else if (mode == 1) {
                sprintf(answer, "STP:  GO\n");
            }
            requested_answered = true;
        }
        // LEER DISTANCIA (modo acercamiento)
        else if ( 
            (0 == strcmp("DS:  REQ\n", request)) &&
            (mode == 1)
        )
        {
            sprintf(answer, "DS:%05d\n", distance);
            requested_answered = true;
        }

        digitalWrite(11, mix);
        digitalWrite(13, gas);
        digitalWrite(12, brake);
        digitalWrite(7, lamp);
    }

    return 0;
}

void write7(int a3, int a2, int a1, int a0) {
    digitalWrite(2, a0);
    digitalWrite(3, a1);
    digitalWrite(4, a2);
    digitalWrite(5, a3);
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

    // M.R.U.A. para calcular distancia y velocidad

    int initial_speed = speed;
    
    // aceleración pendiente + motor
    int accel = 0.25 * slope;
    if (gas) { 
        accel += 0.5;
    }
    if (brake) { // El freno frena velocidad positiva y negativa
        if (speed > 0) {
            speed -= (DELAY_MS * 0.5 / 1000);
        } else {
            speed += (DELAY_MS * 0.5 / 1000);
        }
    }

    // calculo de velocidad (pendiente)
    speed += (DELAY_MS * 0.25 / 1000) * slope;
    // calculo de velocidad (freno/acelerador)
    if (gas) {
        speed += (DELAY_MS * 0.5 / 1000);
    }
    if (brake) { // El freno frena velocidad positiva y negativa
        if (speed > 0) {
            speed -= (DELAY_MS * 0.5 / 1000);
        } else {
            speed += (DELAY_MS * 0.5 / 1000);
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

    // Sensor de luz
    light = map(analogRead(0), 0, 1023, 0, 100);
    if (light >= 100)
        light = 99;
    else if (light <= 0)
        light = 0;

    switch (mode)
    {
    case 0: // MODO SELECCION DE DISTANCIA
        distance = map(analogRead(1), 0, 1023, 10000, 90000);

        int digit = (int) (distance/10000);
        switch (digit)
        {
            case 1: write7(0, 0, 0, 1); break;
            case 2: write7(0, 0, 1, 0); break;
            case 3: write7(0, 0, 1, 1); break;
            case 4: write7(0, 1, 0, 0); break;
            case 5: write7(0, 1, 0, 1); break;
            case 6: write7(0, 1, 1, 0); break;
            case 7: write7(0, 1, 1, 1); break;
            case 8: write7(1, 0, 0, 0); break;
            case 9: write7(1, 0, 0, 1); break;
            default: break;
        }

        // validación de distancia
        bool pulsed = digitalRead(7);
        if ( pulsed ) {
            select = true;
        }
        else if ( !pulsed && select ) { // switch de up a down
            select = false;
            next_mode();
        } 
        break;
    case 1: // MODO DE ACERCAMIENTO AL DEPOSITO
        // calculamos cuanto ha recorrido M.R.U.A
        distance = distance + initial_speed*(DELAY_MS/1000) + 0.5*
        break;
    case 2: break;
    case 3: break;
    default:
        break;
    }
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
    pinMode(7, OUTPUT);   // FOCOS
    pinMode(2, OUTPUT); // 7seg
    pinMode(3, OUTPUT); // 7seg
    pinMode(4, OUTPUT); // 7seg
    pinMode(5, OUTPUT); // 7seg
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
