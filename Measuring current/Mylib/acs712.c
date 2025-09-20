#include "acs712.h"
extern ADC_HandleTypeDef hadc1;
#define ACS712_30A_SENSITIVITY 66
acs712_t acs712;
float _freq = 50.0; 
float _n_total_period = 1.0; 
int adc;
int get_adc() {
    int raw;
		HAL_ADC_Start(&hadc1);
		HAL_ADC_PollForConversion(&hadc1,1000);
		int adc_read=HAL_ADC_GetValue(&hadc1);
		HAL_ADC_Stop(&hadc1);
    return (int)adc_read*1.5;
}
int autoCalibrate() {  
    long _adc = 0,_sample = 0;
    while (_sample < 100) {
        _adc += get_adc();
        _sample ++;
    }
    acs712.offset = (int)_adc / _sample; // average of 10 samples
    return acs712.offset;
}
float ACS712_getAC() {  // It can measure minimum 0.5 maximum
        float _signal_period = 1000.0 / _freq;                    // Hz -> ms
        _signal_period *= _n_total_period;
        unsigned long _total_time = (unsigned long)_signal_period ;
        unsigned long _start_time = HAL_GetTick();
        unsigned long _adc_sqr_sum = 0;
        unsigned int _adc_count = 0;
        long _adc;
        while (HAL_GetTick() - _start_time < _total_time) { 
            _adc = get_adc() - acs712.offset; 
            _adc_sqr_sum += _adc*_adc;
            _adc_count ++;

        }
        float _avg_adc_sqr_sum = _adc_sqr_sum / (float)_adc_count;
				adc=_adc_count;
        float _rms_current = sqrt(_avg_adc_sqr_sum) * acs712.results_adjuster;   // ADC x A/ADC -> A
        return _rms_current;
}
void setSensitivity(float sen)
{
    acs712.results_adjuster /= acs712.inv_sensitivity;
    acs712.inv_sensitivity = 1000.0 / sen;
    acs712.results_adjuster *= acs712.inv_sensitivity;
}
void reset()
{
    float sen =ACS712_30A_SENSITIVITY ; // d?m b?o `sensor_sen[]` và `type` t?n t?i
    setSensitivity(sen);
    acs712.offset = acs712._adc_fs / 2.0;
}
float ACS712_getDC(int _count) {  // required time, around 1 ms of default 10 samples.
  int _adc = 0, _sample = 0;
  while (_sample < _count) {
    _adc += get_adc();
		_sample ++;
  } 
    
  float _adc_avg = ((float)_adc / (float)_count) - acs712.offset;  // average of 10 samples and remove offset
  float _current = _adc_avg * acs712.results_adjuster;   // ADC x A/ADC -> A
  return _current;
}
void ACS712_init(){
     // Set the ADC reference voltage and sensitivity
	acs712._adc_ref = 3.3; // 3.3V
  acs712._adc_fs = 4095; // 12 bit ADC
  acs712.offset = 2970; // 2.5A offset
  acs712.inv_sensitivity = 1000.0 / ACS712_30A_SENSITIVITY; 
  acs712.results_adjuster = (float)acs712._adc_ref / (float)acs712._adc_fs * acs712.inv_sensitivity; // 3.3V / 4095 * 1000.0 / 185.0 = 0.0042
	HAL_ADCEx_Calibration_Start(&hadc1);
}