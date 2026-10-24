#include "max30102.h"

/* =========================================================================
   SOFTWARE I2C DRIVER (GPIOB PB6 = SCL, PB7 = SDA)
   ========================================================================= */

static inline void I2C_Delay(void) {
    for (volatile int i = 0; i < 15; i++) {
        __NOP();
    }
}

static inline void SCL_HIGH(void) {
    HAL_GPIO_WritePin(MAX30102_I2C_PORT, MAX30102_SCL_PIN, GPIO_PIN_SET);
}

static inline void SCL_LOW(void) {
    HAL_GPIO_WritePin(MAX30102_I2C_PORT, MAX30102_SCL_PIN, GPIO_PIN_RESET);
}

static inline void SDA_HIGH(void) {
    HAL_GPIO_WritePin(MAX30102_I2C_PORT, MAX30102_SDA_PIN, GPIO_PIN_SET);
}

static inline void SDA_LOW(void) {
    HAL_GPIO_WritePin(MAX30102_I2C_PORT, MAX30102_SDA_PIN, GPIO_PIN_RESET);
}

static inline uint8_t SDA_READ(void) {
    return (HAL_GPIO_ReadPin(MAX30102_I2C_PORT, MAX30102_SDA_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

static void I2C_Start(void) {
    SDA_HIGH();
    SCL_HIGH();
    I2C_Delay();
    SDA_LOW();
    I2C_Delay();
    SCL_LOW();
    I2C_Delay();
}

static void I2C_Stop(void) {
    SDA_LOW();
    I2C_Delay();
    SCL_HIGH();
    I2C_Delay();
    SDA_HIGH();
    I2C_Delay();
}

static bool I2C_WriteByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80) {
            SDA_HIGH();
        } else {
            SDA_LOW();
        }
        I2C_Delay();
        SCL_HIGH();
        I2C_Delay();
        SCL_LOW();
        byte <<= 1;
        I2C_Delay();
    }

    SDA_HIGH();
    I2C_Delay();
    SCL_HIGH();
    I2C_Delay();
    bool ack = (SDA_READ() == 0);
    SCL_LOW();
    I2C_Delay();

    return ack;
}

static uint8_t I2C_ReadByte(bool ack) {
    uint8_t byte = 0;
    SDA_HIGH();

    for (uint8_t i = 0; i < 8; i++) {
        byte <<= 1;
        I2C_Delay();
        SCL_HIGH();
        I2C_Delay();
        if (SDA_READ()) {
            byte |= 0x01;
        }
        SCL_LOW();
        I2C_Delay();
    }

    if (ack) {
        SDA_LOW();
    } else {
        SDA_HIGH();
    }
    I2C_Delay();
    SCL_HIGH();
    I2C_Delay();
    SCL_LOW();
    SDA_HIGH();
    I2C_Delay();

    return byte;
}

static void max30102_write_reg(uint8_t reg, uint8_t reg_val) {
    I2C_Start();
    if (!I2C_WriteByte(MAX30102_SLAVE_ADDR)) {
        I2C_Stop();
        return;
    }
    if (!I2C_WriteByte(reg)) {
        I2C_Stop();
        return;
    }
    if (!I2C_WriteByte(reg_val)) {
        I2C_Stop();
        return;
    }
    I2C_Stop();
}

static uint8_t max30102_read_reg(uint8_t reg) {
    uint8_t reg_val = 0;
    I2C_Start();
    if (!I2C_WriteByte(MAX30102_SLAVE_ADDR)) {
        I2C_Stop();
        return 0;
    }
    if (!I2C_WriteByte(reg)) {
        I2C_Stop();
        return 0;
    }

    I2C_Start();
    if (!I2C_WriteByte(MAX30102_SLAVE_ADDR | 0x01)) {
        I2C_Stop();
        return 0;
    }

    reg_val = I2C_ReadByte(false);
    I2C_Stop();
    return reg_val;
}

static void max30102_read_multi_reg(uint8_t reg, uint8_t *data, uint8_t len) {
    I2C_Start();
    if (!I2C_WriteByte(MAX30102_SLAVE_ADDR)) {
        I2C_Stop();
        return;
    }
    if (!I2C_WriteByte(reg)) {
        I2C_Stop();
        return;
    }

    I2C_Start();
    if (!I2C_WriteByte(MAX30102_SLAVE_ADDR | 0x01)) {
        I2C_Stop();
        return;
    }

    for (uint8_t i = 0; i < len; i++) {
        data[i] = I2C_ReadByte(i < (len - 1));
    }
    I2C_Stop();
}


void max30102_init(void)
{
    // Khởi tạo chân GPIO: PB6 (SCL), PB7 (SDA), PB5 (INT)
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = MAX30102_SCL_PIN | MAX30102_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(MAX30102_I2C_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = MAX30102_INT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(MAX30102_INT_PORT, &GPIO_InitStruct);

    SCL_HIGH();
    SDA_HIGH();
    HAL_Delay(20);

    // Đợi cảm biến sẵn sàng
    uint16_t timeout = 500;
    while ((max30102_read_reg(REG_PART_ID) != MAX30102_ID) && timeout > 0) {
        HAL_Delay(2);
        timeout--;
    }

    max30102_reset();

    max30102_enable_intterupt(_ppg_rdy);

    max30102_write_reg(REG_FIFO_CONF, 0x50); // SMP_AVE = 4 (Trung bình 4 mẫu chuẩn y-sinh-master, sóng mượt mà), FIFO_ROLLOVER_EN = 1
    max30102_write_reg(REG_SPO2_CONF, 0x27);

    max30102_write_reg(REG_LED_PA1, 0x24);
    max30102_write_reg(REG_LED_PA2, 0x24);

    max30102_write_reg(REG_MODE_CONF, 0x03); // SpO2 mode
}

void max30102_spo2_set_adc_range(max30102_spo2_adc_rge_e _adc_range)
{
    uint8_t new_spo2_conf = max30102_read_reg(REG_SPO2_CONF);
    new_spo2_conf = (new_spo2_conf & (0x3<<5)) | (_adc_range<<5);
    max30102_write_reg(REG_SPO2_CONF, new_spo2_conf);
}

void max30102_spo2_set_sample_rate(max30102_sr_e _sample_rate)
{
    uint8_t new_spo2_conf = max30102_read_reg(REG_SPO2_CONF);
    new_spo2_conf = (new_spo2_conf & (0x7<<2)) | (_sample_rate<<2);
    max30102_write_reg(REG_SPO2_CONF, new_spo2_conf);
}

void max30102_spo2_set_adc_resolution(max30102_led_pw_e _led_pw)
{
    uint8_t new_spo2_conf = max30102_read_reg(REG_SPO2_CONF);
    new_spo2_conf = (new_spo2_conf & (~0x03)) | _led_pw;
    max30102_write_reg(REG_SPO2_CONF, new_spo2_conf);
}

void max30102_enable_temp(void)
{
    max30102_write_reg(REG_DIE_TEMP_CONF, 0x01);
}

float max30102_read_temp(void)
{
    max30102_enable_temp();
    int8_t t_int = (int8_t)max30102_read_reg(REG_DIE_TEMP_INT);
    float t_frac = (float)(max30102_read_reg(REG_DIE_TEMP_FRAC) & 0x0F) * 0.0625f;
    return (float)t_int + t_frac;
}

void max30102_set_mode(max30102_mode_e _mode)
{
    max30102_write_reg(REG_MODE_CONF, _mode);
}

void max30102_reset(void)
{
    max30102_set_mode(_reset_mode);
    uint16_t timeout = 500;
    while ((max30102_read_reg(REG_MODE_CONF) & 0x40) && timeout > 0) {
        HAL_Delay(2);
        timeout--;
    }

    max30102_set_mode(_shutdown_mode);

    // reset fifo
    max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
    max30102_write_reg(REG_OVF_COUNTER, 0x00);
    max30102_write_reg(REG_FIFO_RD_PTR, 0x00);

    // reset interrupt
    max30102_write_reg(REG_INT_ENABLE_1, 0x00);
    max30102_write_reg(REG_INT_ENABLE_2, 0x00);
    max30102_read_reg(REG_INT_STATUS_1);
    max30102_read_reg(REG_INT_STATUS_2);
}

void max30102_enable_intterupt(max30102_interrupt_e no_int)
{
    uint8_t reg = REG_INT_ENABLE_1;
    if (no_int == _die_temp_rdy) reg = REG_INT_ENABLE_2;
    max30102_write_reg(reg, no_int);
}

void max30102_read_fifo(uint8_t *data)
{
    max30102_read_multi_reg(REG_FIFO_DATA, data, 6);
}

void max30102_read_spo2(uint32_t *red, uint32_t *ir)
{
    uint8_t data[6];
    max30102_read_fifo(data);
    *red = (((uint32_t)data[0] << 16) |
            ((uint32_t)data[1] << 8)  |
            data[2]) & 0x3FFFF;
    *ir =  (((uint32_t)data[3] << 16) |
            ((uint32_t)data[4] << 8)  |
            data[5]) & 0x3FFFF;

    // Đọc xóa cờ ngắt
    max30102_read_reg(REG_INT_STATUS_1);
}

uint8_t max30102_part_id(void)
{
    uint8_t id = max30102_read_reg(REG_PART_ID);
    return id;
}

uint8_t max30102_is_data_ready(void)
{
    return (HAL_GPIO_ReadPin(MAX30102_INT_PORT, MAX30102_INT_PIN) == GPIO_PIN_RESET);
}

uint8_t max30102_available_samples(void)
{
    uint8_t wr_ptr = max30102_read_reg(REG_FIFO_WR_PTR) & 0x1F;
    uint8_t rd_ptr = max30102_read_reg(REG_FIFO_RD_PTR) & 0x1F;
    if (wr_ptr >= rd_ptr) {
        return wr_ptr - rd_ptr;
    } else {
        return (32 + wr_ptr) - rd_ptr;
    }
}
