#include "vars.h"
#include <string.h>
#include <stdio.h>


char temper_data[4] = { 0 };

const char *get_var_temper_data() {
    return temper_data;
}
void set_var_temper_data(float *value) {
   sprintf(temper_data,"%.0f",*value);
}

char humidity_data[4] = { 0 };

const char *get_var_humidity_data() {
    return humidity_data;
}

void set_var_humidity_data(float *value) {
    sprintf(humidity_data,"%.0f",*value);
}