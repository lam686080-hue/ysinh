/* vim: set ai et ts=4 sw=4: */
#include "main.h"
#include "ili9341.h"
#include <stdlib.h>
static volatile bool is_dma_busy = false;

#define DMA_BUFFER_SIZE 1024 // Kích thước buffer đệm (1024 bytes = 512 pixels)
static uint8_t dma_buf[DMA_BUFFER_SIZE];

static void ILI9341_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

void ILI9341_Select(void) {
    TFT_CS_LOW();
}

void ILI9341_Unselect(void) {
    TFT_CS_HIGH();
}

void ILI9341_WaitDMA(void) {
    while (is_dma_busy) {
        // Chờ ngắt HAL_SPI_TxCpltCallback được kích hoạt
    }
}

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if ((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT)) return;
    ILI9341_WaitDMA();
    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x, y);
    uint8_t data[2] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    TFT_DC_DATA();
    HAL_SPI_Transmit(&ILI9341_SPI_PORT, data, 2, HAL_MAX_DELAY);
    ILI9341_Unselect();
}

void ILI9341_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy, e2;

    while (1) {
        ILI9341_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void ILI9341_DrawText(const char *str, uint16_t x, uint16_t y, uint16_t color, uint16_t bgcolor, FontDef font) {
    ILI9341_WriteString(x, y, str, font, color, bgcolor);
}


void ILI9341_FillRectangleDMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if ((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT)) return;
    if ((x + w - 1) >= ILI9341_WIDTH) w = ILI9341_WIDTH - x;
    if ((y + h - 1) >= ILI9341_HEIGHT) h = ILI9341_HEIGHT - y;
    if (w == 0 || h == 0) return;

    ILI9341_WaitDMA();
    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    uint32_t total_pixels = (uint32_t)w * h;
    uint8_t msb = color >> 8;
    uint8_t lsb = color & 0xFF;

    // Nạp sẵn giá trị màu RGB565 vào mảng đệm
    for (uint32_t i = 0; i < DMA_BUFFER_SIZE; i += 2) {
        dma_buf[i]   = msb;
        dma_buf[i+1] = lsb;
    }

    TFT_DC_DATA();

    // Chia nhỏ dữ liệu gửi qua DMA nếu diện tích hình lớn hơn kích thước buffer
    while (total_pixels > 0) {
        uint32_t pixels_to_send = total_pixels;
        if (pixels_to_send > (DMA_BUFFER_SIZE / 2)) {
            pixels_to_send = DMA_BUFFER_SIZE / 2;
        }

        ILI9341_WaitDMA();
        is_dma_busy = true;

        // Kích hoạt DMA bắn dữ liệu ra SPI1
        HAL_SPI_Transmit_DMA(&ILI9341_SPI_PORT, dma_buf, pixels_to_send * 2);

        total_pixels -= pixels_to_send;
    }

    ILI9341_WaitDMA();
    ILI9341_Unselect();
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        is_dma_busy = false;  // Đánh dấu DMA đã hoàn thành
    }
}


static void ILI9341_Reset() {
    TFT_RST_LOW();
    HAL_Delay(20);
    TFT_RST_HIGH();
    HAL_Delay(150);
}

static void ILI9341_WriteCommand(uint8_t cmd) {
    ILI9341_WaitDMA();
    TFT_DC_CMD();
    HAL_SPI_Transmit(&ILI9341_SPI_PORT, &cmd, 1, HAL_MAX_DELAY);
}

static void ILI9341_WriteData(uint8_t* buff, size_t buff_size) {
    ILI9341_WaitDMA();
    TFT_DC_DATA();
    HAL_SPI_Transmit(&ILI9341_SPI_PORT, buff, buff_size, HAL_MAX_DELAY);
}

static void ILI9341_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // column address set
    ILI9341_WriteCommand(0x2A); // CASET
    {
        uint8_t data[] = { (x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF };
        ILI9341_WriteData(data, sizeof(data));
    }

    // row address set
    ILI9341_WriteCommand(0x2B); // RASET
    {
        uint8_t data[] = { (y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF };
        ILI9341_WriteData(data, sizeof(data));
    }

    // write to RAM
    ILI9341_WriteCommand(0x2C); // RAMWR
}

void ILI9341_Init() {
    ILI9341_Select();
    ILI9341_Reset();

    // command list is based on https://github.com/martnak/STM32-ILI9341

    // SOFTWARE RESET
    ILI9341_WriteCommand(0x01);
    HAL_Delay(1000);
        
    // POWER CONTROL A
    ILI9341_WriteCommand(0xCB);
    {
        uint8_t data[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER CONTROL B
    ILI9341_WriteCommand(0xCF);
    {
        uint8_t data[] = { 0x00, 0xC1, 0x30 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // DRIVER TIMING CONTROL A
    ILI9341_WriteCommand(0xE8);
    {
        uint8_t data[] = { 0x85, 0x00, 0x78 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // DRIVER TIMING CONTROL B
    ILI9341_WriteCommand(0xEA);
    {
        uint8_t data[] = { 0x00, 0x00 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER ON SEQUENCE CONTROL
    ILI9341_WriteCommand(0xED);
    {
        uint8_t data[] = { 0x64, 0x03, 0x12, 0x81 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // PUMP RATIO CONTROL
    ILI9341_WriteCommand(0xF7);
    {
        uint8_t data[] = { 0x20 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER CONTROL,VRH[5:0]
    ILI9341_WriteCommand(0xC0);
    {
        uint8_t data[] = { 0x23 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER CONTROL,SAP[2:0];BT[3:0]
    ILI9341_WriteCommand(0xC1);
    {
        uint8_t data[] = { 0x10 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // VCM CONTROL
    ILI9341_WriteCommand(0xC5);
    {
        uint8_t data[] = { 0x3E, 0x28 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // VCM CONTROL 2
    ILI9341_WriteCommand(0xC7);
    {
        uint8_t data[] = { 0x86 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // MEMORY ACCESS CONTROL
    ILI9341_WriteCommand(0x36);
    {
        uint8_t data[] = { 0x48 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // PIXEL FORMAT
    ILI9341_WriteCommand(0x3A);
    {
        uint8_t data[] = { 0x55 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // FRAME RATIO CONTROL, STANDARD RGB COLOR
    ILI9341_WriteCommand(0xB1);
    {
        uint8_t data[] = { 0x00, 0x18 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // DISPLAY FUNCTION CONTROL
    ILI9341_WriteCommand(0xB6);
    {
        uint8_t data[] = { 0x08, 0x82, 0x27 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // 3GAMMA FUNCTION DISABLE
    ILI9341_WriteCommand(0xF2);
    {
        uint8_t data[] = { 0x00 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // GAMMA CURVE SELECTED
    ILI9341_WriteCommand(0x26);
    {
        uint8_t data[] = { 0x01 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POSITIVE GAMMA CORRECTION
    ILI9341_WriteCommand(0xE0);
    {
        uint8_t data[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                           0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // NEGATIVE GAMMA CORRECTION
    ILI9341_WriteCommand(0xE1);
    {
        uint8_t data[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                           0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F };
        ILI9341_WriteData(data, sizeof(data));
    }

    // EXIT SLEEP
    ILI9341_WriteCommand(0x11);
    HAL_Delay(120);

    // TURN ON DISPLAY
    ILI9341_WriteCommand(0x29);

    // MADCTL
    ILI9341_WriteCommand(0x36);
    {
        uint8_t data[] = { ILI9341_ROTATION };
        ILI9341_WriteData(data, sizeof(data));
    }

    ILI9341_Unselect();
}

//void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
//    if((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT))
//        return;
//
//    ILI9341_Select();
//
//    ILI9341_SetAddressWindow(x, y, x+1, y+1);
//    uint8_t data[] = { color >> 8, color & 0xFF };
//    ILI9341_WriteData(data, sizeof(data));
//
//    ILI9341_Unselect();
//}

static void ILI9341_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor) {
    uint32_t i, b, j;
    uint32_t pixel_idx = 0;
    uint16_t total_pixels = font.width * font.height;

    if ((uint32_t)total_pixels * 2 > DMA_BUFFER_SIZE) return;

    ILI9341_SetAddressWindow(x, y, x + font.width - 1, y + font.height - 1);

    uint8_t c_msb = color >> 8, c_lsb = color & 0xFF;
    uint8_t bg_msb = bgcolor >> 8, bg_lsb = bgcolor & 0xFF;

    for (i = 0; i < font.height; i++) {
        b = font.data[(ch - 32) * font.height + i];
        for (j = 0; j < font.width; j++) {
            if ((b << j) & 0x8000) {
                dma_buf[pixel_idx++] = c_msb;
                dma_buf[pixel_idx++] = c_lsb;
            } else {
                dma_buf[pixel_idx++] = bg_msb;
                dma_buf[pixel_idx++] = bg_lsb;
            }
        }
    }
    TFT_DC_DATA();
    HAL_SPI_Transmit(&ILI9341_SPI_PORT, dma_buf, pixel_idx, HAL_MAX_DELAY);
}

void ILI9341_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor) {
    ILI9341_Select();

    while(*str) {
        if(x + font.width >= ILI9341_WIDTH) {
            x = 0;
            y += font.height;
            if(y + font.height >= ILI9341_HEIGHT) {
                break;
            }

            if(*str == ' ') {
                // skip spaces in the beginning of the new line
                str++;
                continue;
            }
        }

        ILI9341_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }

    ILI9341_Unselect();
}

void ILI9341_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // clipping
    if((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT)) return;
    if((x + w - 1) >= ILI9341_WIDTH) w = ILI9341_WIDTH - x;
    if((y + h - 1) >= ILI9341_HEIGHT) h = ILI9341_HEIGHT - y;

    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x+w-1, y+h-1);

    uint8_t data[] = { color >> 8, color & 0xFF };
    HAL_GPIO_WritePin(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin, GPIO_PIN_SET);
    for(y = h; y > 0; y--) {
        for(x = w; x > 0; x--) {
            HAL_SPI_Transmit(&ILI9341_SPI_PORT, data, sizeof(data), HAL_MAX_DELAY);
        }
    }

    ILI9341_Unselect();
}

void ILI9341_FillScreen(uint16_t color) {
    ILI9341_FillRectangleDMA(0, 0, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

void ILI9341_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t* data) {
    if((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT)) return;
    if((x + w - 1) >= ILI9341_WIDTH) return;
    if((y + h - 1) >= ILI9341_HEIGHT) return;

    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x+w-1, y+h-1);
    ILI9341_WriteData((uint8_t*)data, sizeof(uint16_t)*w*h);
    ILI9341_Unselect();
}

void ILI9341_InvertColors(bool invert) {
    ILI9341_Select();
    ILI9341_WriteCommand(invert ? 0x21 /* INVON */ : 0x20 /* INVOFF */);
    ILI9341_Unselect();
}

