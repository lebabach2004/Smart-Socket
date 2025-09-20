#ifndef __ACS712_H
#define __ACS712_H
#include <stdint.h>
#include "main.h"
#include "math.h"

typedef struct{
    float _adc_ref ;
    int _adc_fs ;
    float inv_sensitivity;
    float results_adjuster;
    int offset;
    int adc_fs;
} acs712_t;
int autoCalibrate();
void setSensitivity(float sen);
float ACS712_getAC();
float ACS712_getDC(int _count);
void reset();
void ACS712_init();
void ADS1115_init();
int get_adc();
#endif