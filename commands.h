#pragma once

/* Enters command mode */
bool CommandsInit();

/* Cleans up the resources. */
bool CommandsUninit();

/* Available commands: (no guaranteed delivery)*/
void takeoff();
void land();
void emergency();
void speed(int x);
void querybattery();
void rc(int roll, int pitch, int throttle, int yaw);

/* When all is lost (except the connection) */
void panic();