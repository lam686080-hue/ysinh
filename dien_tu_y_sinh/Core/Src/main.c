/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "ili9341.h"
#include "fonts.h"
#include "max30102.h"
#include "filter.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define IR_DC_THRESHOLD 100000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

/* USER CODE BEGIN PV */
// Tọa độ khung sóng nhịp tim (~4 s trên màn, mỗi nhịp đủ rộng để thấy dạng xung)
#define WAVE_X_MIN   10
#define WAVE_X_MAX   310
#define WAVE_Y_MIN   105
#define WAVE_Y_MAX   230
#define WAVE_Y_MID   167
#define WAVE_Y_BASE  205
#define WAVE_STEP    3

static uint16_t wave_x_curr = WAVE_X_MIN;
static uint16_t wave_y_prev = WAVE_Y_BASE;
static float wave_disp = 0.0f;

// Biến xử lý tín hiệu chuẩn y tế
static uint32_t ir = 0, red = 0;
static float dc_ir = 0.0f, dc_red = 0.0f;
static float ac_ir = 0.0f, ac_red = 0.0f;
static float alpha = 0.1f;

static float ir_filtered = 0.0f;
static float red_filtered = 0.0f;

static float ir_prev2 = 0.0f;
static float ir_prev1 = 0.0f;
static float ir_current = 0.0f;

static uint32_t last_peak_time = 0;
static uint8_t peak_detected = 0;

static float BPM = 0.0f;
static uint8_t display_bpm = 0;
static uint8_t display_spo2 = 0;
static uint8_t finger_detected = 0;

static float red_ac_peak = 0.0f;
static float ir_ac_peak = 0.0f;

// Trung bình 8 khoảng RR + làm mượt để BPM không nhảy vọt
#define BEAT_BUF_SIZE 8
#define SAMPLE_PERIOD_MS 40u /* MAX30102 100 Hz / SMP_AVE 4 = 25 Hz */
static uint16_t beat_intervals[BEAT_BUF_SIZE] = {0};
static uint8_t beat_idx = 0;
static uint8_t beat_cnt = 0;
static float peak_amplitude_recent = 50.0f;
static float wave_scale = 0.18f;
static uint32_t sample_tick_ms = 0;
static uint32_t last_beat_wall_ms = 0;
static float bpm_smooth = 0.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Khởi tạo khung giao diện cố định
void UI_InitLayout(void) {
    ILI9341_FillScreen(BLACK);

    // Tiêu đề
    ILI9341_DrawText("PATIENT MONITOR - MAX30102", 20, 5, CYAN, BLACK, Font_7x10);

    // Khung BPM (Màu đỏ)
    ILI9341_FillRectangleDMA(10, 25, 145, 70, RED);
    ILI9341_FillRectangleDMA(12, 27, 141, 66, BLACK);
    ILI9341_DrawText("HEART RATE", 18, 30, RED, BLACK, Font_7x10);
    ILI9341_DrawText("BPM", 115, 70, RED, BLACK, Font_7x10);

    // Khung SpO2 (Màu xanh lá)
    ILI9341_FillRectangleDMA(165, 25, 145, 70, GREEN);
    ILI9341_FillRectangleDMA(167, 27, 141, 66, BLACK);
    ILI9341_DrawText("SPO2", 175, 30, GREEN, BLACK, Font_7x10);
    ILI9341_DrawText("%", 290, 70, GREEN, BLACK, Font_7x10);

    // Khung sóng PPG (Màu vàng)
    ILI9341_FillRectangleDMA(WAVE_X_MIN - 2, WAVE_Y_MIN - 2, (WAVE_X_MAX - WAVE_X_MIN) + 4, (WAVE_Y_MAX - WAVE_Y_MIN) + 4, YELLOW);
    ILI9341_FillRectangleDMA(WAVE_X_MIN, WAVE_Y_MIN, (WAVE_X_MAX - WAVE_X_MIN), (WAVE_Y_MAX - WAVE_Y_MIN), BLACK);
    ILI9341_DrawText("HEART WAVE", WAVE_X_MIN + 5, WAVE_Y_MIN + 5, YELLOW, BLACK, Font_7x10);
}

// Cập nhật số liệu BPM
void UI_UpdateBPM(uint8_t bpm) {
    char buf[10];
    if (bpm == 0) {
        sprintf(buf, "---");
    } else {
        sprintf(buf, "%3d", bpm);
    }
    ILI9341_DrawText(buf, 30, 48, WHITE, BLACK, Font_16x26);
}

// Cập nhật số liệu SpO2
void UI_UpdateSpO2(uint8_t spo2) {
    char buf[10];
    if (spo2 == 0) {
        sprintf(buf, "---");
    } else {
        sprintf(buf, "%3d", spo2);
    }
    ILI9341_DrawText(buf, 185, 48, WHITE, BLACK, Font_16x26);
}

// Vẽ sóng nhịp tim (xung PPG dạng tâm thu, nét 2px, quét 3px/mẫu)
void UI_DrawWavePoint(int16_t ppg_val) {
    int16_t y_new = WAVE_Y_BASE - ppg_val;
    if (y_new < WAVE_Y_MIN + 18) y_new = WAVE_Y_MIN + 18;
    if (y_new > WAVE_Y_MAX - 4)  y_new = WAVE_Y_MAX - 4;

    uint16_t x1 = wave_x_curr + WAVE_STEP;
    if (x1 > WAVE_X_MAX) x1 = WAVE_X_MAX;

    uint16_t erase_x = wave_x_curr + 1;
    uint16_t erase_w = WAVE_STEP + 8;
    if (erase_x + erase_w > WAVE_X_MAX) {
        erase_w = (WAVE_X_MAX > erase_x) ? (WAVE_X_MAX - erase_x) : 0;
    }
    if (erase_w > 0) {
        ILI9341_FillRectangleDMA(erase_x, WAVE_Y_MIN + 16, erase_w, (WAVE_Y_MAX - WAVE_Y_MIN - 17), BLACK);
    }

    if (x1 > wave_x_curr) {
        ILI9341_DrawLine(wave_x_curr, wave_y_prev, x1, y_new, GREEN);
        ILI9341_DrawLine(wave_x_curr, (uint16_t)(wave_y_prev - 1), x1, (uint16_t)(y_new - 1), GREEN);
    }

    wave_y_prev = y_new;
    wave_x_curr = x1;

    if (wave_x_curr >= WAVE_X_MAX) {
        wave_x_curr = WAVE_X_MIN + 1;
        wave_y_prev = y_new;
        ILI9341_FillRectangleDMA(WAVE_X_MIN, WAVE_Y_MIN + 16, 10, (WAVE_Y_MAX - WAVE_Y_MIN - 17), BLACK);
    }
}

static bool last_finger_state = true;

void UI_UpdateFingerStatus(bool finger) {
    if (finger != last_finger_state) {
        last_finger_state = finger;
        if (!finger) {
            UI_UpdateBPM(0);
            UI_UpdateSpO2(0);
            ILI9341_DrawText("PLACE FINGER ON SENSOR", 75, WAVE_Y_MID - 5, YELLOW, BLACK, Font_7x10);
        } else {
            // Xóa dòng thông báo khi đã đặt ngón tay
            ILI9341_FillRectangleDMA(65, WAVE_Y_MID - 8, 190, 16, BLACK);
        }
    }
}

void UI_DrawHeartBeat(bool active) {
    uint16_t color = active ? RED : BLACK;
    ILI9341_FillRectangleDMA(95, 30, 8, 8, color);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  ILI9341_Init();
  UI_InitLayout();
  UI_UpdateBPM(0);
  UI_UpdateSpO2(0);

  // Khởi tạo bộ lọc dải thông (0.5 - 5Hz) và cảm biến MAX30102 theo repo thuyhoh/y-sinh
  BandPass_Init();
  max30102_init();
  dc_ir = dc_red = 0;
  ir_prev1 = ir_prev2 = 0;
  UI_UpdateFingerStatus(false);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_ui_tick = 0;
  uint32_t last_fifo_poll = 0;
  uint32_t beat_anim_tick = 0;
  bool heart_on = false;
  uint8_t prev_ui_bpm = 255;
  uint8_t prev_ui_spo2 = 255;

  while (1)
  {
      bool data_ready = max30102_is_data_ready();
      if (!data_ready && (HAL_GetTick() - last_fifo_poll >= 15)) {
          last_fifo_poll = HAL_GetTick();
          if (max30102_available_samples() > 0) {
              data_ready = true;
          }
      }

      if (data_ready) {
          uint8_t samples = max30102_available_samples();
          if (samples == 0) samples = 1;
          if (samples > 16) samples = 16; // Giới hạn an toàn tránh nghẽn CPU

          for (uint8_t s = 0; s < samples; s++) {
              max30102_read_spo2(&red, &ir);

              // 1. Tách thành phần DC (DC tracking)
              dc_ir  += alpha * ((float)ir  - dc_ir);
              dc_red += alpha * ((float)red - dc_red);

              // Chuẩn y tế: Nghịch đảo quang học (dc - raw) để khi tim tống máu, xung vọt lên thành đỉnh dương (Positive Systolic Peak)
              ac_ir  = dc_ir - (float)ir;
              ac_red = dc_red - (float)red;

              // 2. Kiểm tra tiếp xúc ngón tay chuẩn y tế (DC nằm trong dải sinh lý hợp lệ, không bão hòa quang)
              if (dc_ir > 40000.0f && dc_ir < 250000.0f) {
                  finger_detected = 1;
              } else {
                  finger_detected = 0;
                  BPM = 0;
                  display_bpm = 0;
                  display_spo2 = 0;
                  peak_detected = 0;
                  beat_cnt = 0;
                  beat_idx = 0;
                  bpm_smooth = 0.0f;
                  peak_amplitude_recent = 50.0f;
                  wave_disp = 0.0f;
                  BandPass_Reset();
              }

              // 3. Bộ lọc dải thông 0.5 - 4.5Hz chuẩn y tế (BandPass_IR & BandPass_Red)
              ir_filtered = BandPass_IR(ac_ir);
              red_filtered = BandPass_Red(ac_red);

              ir_current = ir_filtered;
              sample_tick_ms += SAMPLE_PERIOD_MS;

              // 4. Bắt đỉnh tâm thu:
              // Thời gian RR đếm theo mẫu 25 Hz (không dùng HAL_GetTick) để FIFO xử lý hàng loạt
              // không làm khoảng RR bị rút ngắn giả → BPM nhảy vọt.
              float dynamic_threshold = 0.6f * peak_amplitude_recent;
              if (dynamic_threshold < 20.0f) dynamic_threshold = 20.0f;
              if (dynamic_threshold > 80.0f) dynamic_threshold = 80.0f;

              uint32_t now = sample_tick_ms;
              uint32_t min_rr_ms = 400; /* trần ~150 BPM, loại khe dicrotic */
              if (beat_cnt >= 2) {
                  uint32_t sum_prev = 0;
                  for (uint8_t i = 0; i < beat_cnt; i++) {
                      sum_prev += beat_intervals[i];
                  }
                  uint32_t avg_prev = sum_prev / beat_cnt;
                  uint32_t adaptive_rr = (uint32_t)(0.55f * (float)avg_prev);
                  if (adaptive_rr > min_rr_ms) min_rr_ms = adaptive_rr;
                  if (min_rr_ms > 700) min_rr_ms = 700;
              }

              if (finger_detected && (ir_prev2 < ir_prev1) && (ir_prev1 >= ir_current) && (ir_prev1 > dynamic_threshold) &&
                  (!peak_detected || (now - last_peak_time >= min_rr_ms)))
              {
                  peak_amplitude_recent = 0.7f * peak_amplitude_recent + 0.3f * ir_prev1;

                  if (peak_detected == 0)
                  {
                      last_peak_time = now;
                      last_beat_wall_ms = HAL_GetTick();
                      peak_detected = 1;
                      beat_cnt = 0;
                      beat_idx = 0;
                      bpm_smooth = 0.0f;
                  }
                  else
                  {
                      uint32_t delta_t = now - last_peak_time;

                      uint8_t accept_rr = 0;
                      if (delta_t >= 400 && delta_t <= 1500) {
                          if (beat_cnt < 2) {
                              accept_rr = 1;
                          } else {
                              uint32_t sum_dt0 = 0;
                              for (uint8_t i = 0; i < beat_cnt; i++) {
                                  sum_dt0 += beat_intervals[i];
                              }
                              uint32_t avg0 = sum_dt0 / beat_cnt;
                              /* Bỏ đỉnh thừa (RR quá ngắn). Nhịp bị miss (RR ~2 lần) không đưa vào trung bình. */
                              if (delta_t < (avg0 * 75u) / 100u) {
                                  accept_rr = 0; /* không dịch last_peak: đây là đỉnh giả */
                              } else if (delta_t > (avg0 * 16u) / 10u) {
                                  last_peak_time = now; /* miss beat: neo lại, không làm BPM tụt */
                                  accept_rr = 0;
                              } else {
                                  accept_rr = 1;
                              }
                          }
                      }

                      if (accept_rr) {
                          last_peak_time = now;
                          last_beat_wall_ms = HAL_GetTick();
                          beat_intervals[beat_idx] = (uint16_t)delta_t;
                          beat_idx = (beat_idx + 1) % BEAT_BUF_SIZE;
                          if (beat_cnt < BEAT_BUF_SIZE) beat_cnt++;

                          uint32_t sum_dt = 0;
                          for (uint8_t i = 0; i < beat_cnt; i++) {
                              sum_dt += beat_intervals[i];
                          }
                          uint32_t avg_dt = sum_dt / beat_cnt;
                          float instant_bpm = 60000.0f / (float)avg_dt;

                          if (instant_bpm >= 40.0f && instant_bpm <= 150.0f && beat_cnt >= 3) {
                              if (bpm_smooth < 1.0f) {
                                  bpm_smooth = instant_bpm;
                              } else {
                                  bpm_smooth = 0.8f * bpm_smooth + 0.2f * instant_bpm;
                                  float max_step = 3.0f;
                                  if (bpm_smooth > (float)display_bpm + max_step && display_bpm > 0) {
                                      bpm_smooth = (float)display_bpm + max_step;
                                  }
                                  if (bpm_smooth < (float)display_bpm - max_step && display_bpm > 0) {
                                      bpm_smooth = (float)display_bpm - max_step;
                                  }
                              }
                              display_bpm = (uint8_t)(bpm_smooth + 0.5f);

                              UI_DrawHeartBeat(true);
                              heart_on = true;
                              beat_anim_tick = HAL_GetTick();
                          }
                      }
                  }

                  // 5. Tính SpO2 chuẩn y tế tại đỉnh tâm thu (Ratio of Ratios)
                  red_ac_peak = fabsf(red_filtered);
                  ir_ac_peak  = fabsf(ir_filtered);
                  if (ir_ac_peak > 5.0f && dc_ir > 10000.0f && dc_red > 10000.0f) {
                      float R = (red_ac_peak / dc_red) / (ir_ac_peak / dc_ir);

                      // Đường cong hiệu chuẩn y tế chuẩn hoá Maxim / Nellcor
                      float calc_spo2 = 110.0f - 25.0f * R;
                      if (calc_spo2 > 99.0f) calc_spo2 = 99.0f;
                      if (calc_spo2 < 85.0f) calc_spo2 = 85.0f;

                      if (display_spo2 == 0) {
                          display_spo2 = (uint8_t)(calc_spo2 + 0.5f);
                      } else {
                          display_spo2 = (uint8_t)(0.8f * display_spo2 + 0.2f * calc_spo2 + 0.5f);
                      }
                  }
              }

              ir_prev2 = ir_prev1;
              ir_prev1 = ir_current;

              // 6. Vẽ sóng nhịp tim: làm mượt hiển thị, nén nửa âm để thành xung (đỉnh tâm thu)
              if (finger_detected) {
                  wave_disp = 0.55f * wave_disp + 0.45f * ir_current;
                  float pulse = wave_disp;
                  if (pulse < 0.0f) pulse *= 0.22f;

                  if (peak_amplitude_recent > 15.0f) {
                      float target_scale = 58.0f / peak_amplitude_recent;
                      if (target_scale > 0.55f) target_scale = 0.55f;
                      if (target_scale < 0.10f) target_scale = 0.10f;
                      wave_scale += 0.03f * (target_scale - wave_scale);
                  }

                  int16_t wave_val = (int16_t)(pulse * wave_scale);
                  if (wave_val > 72)  wave_val = 72;
                  if (wave_val < -12) wave_val = -12;
                  UI_DrawWavePoint(wave_val);
              } else {
                  wave_disp = 0.0f;
                  UI_DrawWavePoint(0);
              }
          }

          // Cập nhật thông báo ngón tay
          UI_UpdateFingerStatus(finger_detected);
      }

      // Tắt biểu tượng nhịp tim sau 120ms
      if (heart_on && (HAL_GetTick() - beat_anim_tick > 120)) {
          UI_DrawHeartBeat(false);
          heart_on = false;
      }

      // Reset BPM nếu quá 3.5 giây không bắt được nhịp
      if (finger_detected && peak_detected && (HAL_GetTick() - last_beat_wall_ms > 3500)) {
          display_bpm = 0;
          bpm_smooth = 0.0f;
          beat_cnt = 0;
          beat_idx = 0;
          peak_detected = 0;
      }

      // Cập nhật số liệu BPM & SpO2 lên màn hình mỗi 250ms (chỉ gửi lệnh vẽ khi giá trị thay đổi để tối ưu bus SPI)
      if (HAL_GetTick() - last_ui_tick >= 250) {
          last_ui_tick = HAL_GetTick();
          if (finger_detected) {
              if (display_bpm > 0 && display_bpm != prev_ui_bpm) {
                  prev_ui_bpm = display_bpm;
                  UI_UpdateBPM(display_bpm);
              }
              if (display_spo2 > 0 && display_spo2 != prev_ui_spo2) {
                  prev_ui_spo2 = display_spo2;
                  UI_UpdateSpO2(display_spo2);
              }
          } else {
              prev_ui_bpm = 0;
              prev_ui_spo2 = 0;
          }
      }
  /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, TFT_DC_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, TFT_RST_Pin|TFT_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : TFT_DC_Pin TFT_RST_Pin TFT_CS_Pin */
  GPIO_InitStruct.Pin = TFT_DC_Pin|TFT_RST_Pin|TFT_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
