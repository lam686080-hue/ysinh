#ifndef _MAX20102_H__
#define _MAX20102_H__

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX30102_I2C_PORT       GPIOB
#define MAX30102_SCL_PIN        GPIO_PIN_6
#define MAX30102_SDA_PIN        GPIO_PIN_7

#define MAX30102_INT_PORT       GPIOB
#define MAX30102_INT_PIN        GPIO_PIN_5
#define MAX30102_SLAVE_ADDR     (0x57<<1)
#define MAX30102_ID             0x15

typedef enum{
    _heart_mode = 0x02,
    _spo2_mode = 0x03,
    _multi_led_mode = 0x07,
    _reset_mode = 0x40,
    _shutdown_mode = 0x80
}max30102_mode_e;

typedef enum{
    _sr_50 = 0,
    _sr_100,
    _sr_200,
    _sr_400,
    _sr_800,
    _sr_1000,
    _sr_1600,
    _sr_3200
}max30102_sr_e;

typedef enum{
    _15bit,
    _16bit,
    _17bit,
    _18bit
}max30102_led_pw_e; // adc resolution

typedef enum{
    _2048nA = 0,
    _4096nA,
    _8192nA,
    _16384nA
}max30102_spo2_adc_rge_e;

typedef enum{
    _a_full = (1<<7),       // FIFO Almost Full Flag
    _ppg_rdy = (1<<6),      // New FIFO Data Ready
    _alc_ovf = (1<<5),      // Ambient Light Cancellation Overflow
    _prox_int = (1<<4),     // Proximity Threshold Triggered
    _die_temp_rdy = (1<<1)  // Internal Temperature Ready Flag
}max30102_interrupt_e;

/* Register Maps */
#define REG_INT_STATUS_1        0x00
#define REG_INT_STATUS_2        0x01
#define REG_INT_ENABLE_1        0x02
#define REG_INT_ENABLE_2        0x03
#define REG_FIFO_WR_PTR         0x04
#define REG_OVF_COUNTER         0x05
#define REG_FIFO_RD_PTR         0x06
#define REG_FIFO_DATA           0x07
#define REG_FIFO_CONF           0x08
#define REG_MODE_CONF           0x09
#define REG_SPO2_CONF           0x0A
#define REG_LED_PA1             0x0C
#define REG_LED_PA2             0x0D
#define REG_PROX_MODE           0x10
#define REG_MULTI_LED_CONTROL   0x11
#define REG_DIE_TEMP_INT        0x1F
#define REG_DIE_TEMP_FRAC       0x20
#define REG_DIE_TEMP_CONF       0x21
#define REG_PROX_INT_THRESH     0x30
#define REG_REV_ID              0xFE
#define REG_PART_ID             0xFF

/* main function */
void max30102_init(void);
void max30102_read_fifo(uint8_t *data);
void max30102_read_spo2(uint32_t *red, uint32_t *ir);
float max30102_read_temp(void);

/* sub function */
uint8_t max30102_part_id(void);
void max30102_set_mode(max30102_mode_e _mode);
void max30102_enable_intterupt(max30102_interrupt_e no_int);
void max30102_enable_temp(void);
void max30102_reset(void);
void max30102_spo2_set_adc_range(max30102_spo2_adc_rge_e _adc_range);
void max30102_spo2_set_sample_rate(max30102_sr_e _sample_rate);
void max30102_spo2_set_adc_resolution(max30102_led_pw_e _led_pw);
uint8_t max30102_is_data_ready(void);
uint8_t max30102_available_samples(void);

#endif // _MAX20102_H__
