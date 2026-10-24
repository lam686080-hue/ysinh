#include "filter.h"

/* Bo loc cho kenh IR */
static HighPassFilter hpf_ir;
static LowPassFilter lpf_ir;

/* Bo loc cho kenh Red */
static HighPassFilter hpf_red;
static LowPassFilter lpf_red;

/* Khoi tao bo loc thong cao */
void HPF_Init(HighPassFilter *f, float cutoff, float fs)
{
    f->prev_input = 0.0f;
    f->prev_output = 0.0f;

    f->alpha = expf(-2.0f * 3.14159265f * cutoff / fs);
}

/* Xu ly bo loc thong cao */
float HPF_Update(HighPassFilter *f, float input)
{
    float output;

    output = f->alpha *
             (f->prev_output + input - f->prev_input);

    f->prev_input = input;
    f->prev_output = output;

    return output;
}

/* Khoi tao bo loc thong thap */
void LPF_Init(LowPassFilter *f, float cutoff, float fs)
{
    f->prev_output = 0.0f;

    f->alpha = 1.0f -
               expf(-2.0f * 3.14159265f * cutoff / fs);
}

/* Xu ly bo loc thong thap */
float LPF_Update(LowPassFilter *f, float input)
{
    float output;

    output = f->prev_output +
             f->alpha * (input - f->prev_output);

    f->prev_output = output;

    return output;
}

/* Khoi tao bo loc thong dai 0.5 - 4.5 Hz chuẩn y tế (khớp tần số fs = 25Hz của MAX30102 SMP_AVE = 4) */
void BandPass_Init(void)
{
    HPF_Init(&hpf_ir,  0.5f, 25.0f);
    LPF_Init(&lpf_ir,  4.5f, 25.0f);

    HPF_Init(&hpf_red, 0.5f, 25.0f);
    LPF_Init(&lpf_red, 4.5f, 25.0f);
}

/* Reset trang thai bo loc khi nhac ngon tay */
void BandPass_Reset(void)
{
    hpf_ir.prev_input = 0.0f;
    hpf_ir.prev_output = 0.0f;
    lpf_ir.prev_output = 0.0f;

    hpf_red.prev_input = 0.0f;
    hpf_red.prev_output = 0.0f;
    lpf_red.prev_output = 0.0f;
}

/* Loc tin hieu IR */
float BandPass_IR(float input)
{
    float temp;

    temp = HPF_Update(&hpf_ir, input);

    return LPF_Update(&lpf_ir, temp);
}

/* Loc tin hieu Red */
float BandPass_Red(float input)
{
    float temp;

    temp = HPF_Update(&hpf_red, input);

    return LPF_Update(&lpf_red, temp);
}
