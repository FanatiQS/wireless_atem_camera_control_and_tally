// Include guard
#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h> // bool

void sensors_init(void);
bool sensors_voltage_read(float* voltage);
bool sensors_temperature_read(float* temperature);

#endif // SENSORS_H
