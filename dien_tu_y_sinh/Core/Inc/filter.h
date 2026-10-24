#ifndef FILTER_H
#define FILTER_H

#include <math.h>

/* Bo loc thong cao */
typedef struct
{
    float prev_input;
    float prev_output;
    float alpha;
} HighPassFilter;

/* Bo loc thong thap */
typedef struct
{
    float prev_output;
    float alpha;
} LowPassFilter;

/* Khoi tao bo loc */
void HPF_Init(HighPassFilter *f, float cutoff, float fs);
void LPF_Init(LowPassFilter *f, float cutoff, float fs);

/* Xu ly mot mau du lieu */
float HPF_Update(HighPassFilter *f, float input);
float LPF_Update(LowPassFilter *f, float input);

void BandPass_Init(void);
void BandPass_Reset(void);
float BandPass_IR(float input);
float BandPass_Red(float input);

#endif /* FILTER_H */
