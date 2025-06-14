/* USER CODE BEGIN Header */
/**
  ****************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ****************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ****************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NUM_LEDS 64
#define BITS_PER_LED 24
#define PWM_BUFFER_SIZE (NUM_LEDS * BITS_PER_LED * 3) // 3 chu kỳ PWM/bit
#define HALF_LEDS 63
#define COLOR_COUNT 4
#define LEDS_PER_SEGMENT 8
#define NUM_SEGMENTS 4
#define TOTAL_STEPS (HALF_LEDS - COLOR_COUNT + 1)
#define NUM_MODES 5 // Thêm Strobe
#define OLED_ADDRESS 0x3C // Địa chỉ I2C của OLED
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGE_HEIGHT 8
#define ADC_BUFFER_SIZE 64 // Kích thước buffer, điều chỉnh để phù hợp
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
DMA_HandleTypeDef hdma_tim1_ch1;

/* USER CODE BEGIN PV */
uint8_t led_colors[NUM_LEDS * 3];
uint16_t pwm_buffer[PWM_BUFFER_SIZE];
volatile uint8_t dma_complete = 1;
volatile uint8_t oled_needs_update = 1;
uint32_t last_update = 0; // Thêm biến mới
uint32_t adc_buffer[ADC_BUFFER_SIZE];
volatile uint8_t adc_data_ready = 0;
volatile uint32_t avg_amplitude = 0; // Thêm biến toàn cục

volatile uint8_t current_mode = 0;
volatile uint8_t current_speed = 0; // 0: 120ms, 1: 90ms, 2: 70ms, 3: 50ms
volatile uint8_t current_brightness = 100;
volatile uint8_t brightness_index = 0; // Chỉ số cho mảng brightness_levels
uint8_t rainbow_offset = 0;
uint8_t fade_direction = 1;
uint8_t fade_hue = 0;
uint32_t animation_step = 0;
uint8_t strobe_state = 0;
uint8_t strobe_hue = 0;
uint8_t brightness_fade = 0;

const uint16_t speed_delays[] = {120, 90, 70, 50};
const uint8_t brightness_levels[] = {100, 150, 200, 250}; // Mảng chứa các mức độ sáng

// Font 8x8 cho OLED - Định dạng bitmap dọc
// Mỗi ký tự 8 bytes, mỗi byte đại diện cho 1 cột 8 pixel
const uint8_t Font8x8[][8] = {
    // Space (32)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // A (65)
    {0x7E, 0x11, 0x11, 0x11, 0x7E, 0x00, 0x00, 0x00},
    // B (66)
    {0x7F, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, 0x00},
    // C (67)
    {0x3E, 0x41, 0x41, 0x41, 0x22, 0x00, 0x00, 0x00},
    // D (68)
    {0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00, 0x00, 0x00},
    // E (69)
    {0x7F, 0x49, 0x49, 0x49, 0x41, 0x00, 0x00, 0x00},
    // F (70)
    {0x7F, 0x09, 0x09, 0x09, 0x01, 0x00, 0x00, 0x00},
    // G (71)
    {0x3E, 0x41, 0x49, 0x49, 0x7A, 0x00, 0x00, 0x00},
    // H (72)
    {0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x00, 0x00},
    // I (73)
    {0x00, 0x41, 0x7F, 0x41, 0x00, 0x00, 0x00, 0x00},
    // J (74)
    {0x20, 0x40, 0x41, 0x3F, 0x01, 0x00, 0x00, 0x00},
    // K (75)
    {0x7F, 0x08, 0x14, 0x22, 0x41, 0x00, 0x00, 0x00},
    // L (76)
    {0x7F, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00},
    // M (77)
    {0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x00, 0x00, 0x00},
    // N (78)
    {0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00, 0x00, 0x00},
    // O (79)
    {0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00, 0x00, 0x00},
    // P (80)
    {0x7F, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00},
    // Q (81)
    {0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00, 0x00, 0x00},
    // R (82)
    {0x7F, 0x09, 0x19, 0x29, 0x46, 0x00, 0x00, 0x00},
    // S (83)
    {0x46, 0x49, 0x49, 0x49, 0x31, 0x00, 0x00, 0x00},
    // T (84)
    {0x01, 0x01, 0x7F, 0x01, 0x01, 0x00, 0x00, 0x00},
    // U (85)
    {0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00, 0x00, 0x00},
    // V (86)
    {0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00, 0x00, 0x00},
    // W (87)
    {0x3F, 0x40, 0x38, 0x40, 0x3F, 0x00, 0x00, 0x00},
    // X (88)
    {0x63, 0x14, 0x08, 0x14, 0x63, 0x00, 0x00, 0x00},
    // Y (89)
    {0x07, 0x08, 0x70, 0x08, 0x07, 0x00, 0x00, 0x00},
    // Z (90)
    {0x61, 0x51, 0x49, 0x45, 0x43, 0x00, 0x00, 0x00},
    // 0 (48)
    {0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x00, 0x00},
    // 1 (49)
    {0x00, 0x42, 0x7F, 0x40, 0x00, 0x00, 0x00, 0x00},
    // 2 (50)
    {0x42, 0x61, 0x51, 0x49, 0x46, 0x00, 0x00, 0x00},
    // 3 (51)
    {0x21, 0x41, 0x45, 0x4B, 0x31, 0x00, 0x00, 0x00},
    // 4 (52)
    {0x18, 0x14, 0x12, 0x7F, 0x10, 0x00, 0x00, 0x00},
    // 5 (53)
    {0x27, 0x45, 0x45, 0x45, 0x39, 0x00, 0x00, 0x00},
    // 6 (54)
    {0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00, 0x00, 0x00},
    // 7 (55)
    {0x01, 0x71, 0x09, 0x05, 0x03, 0x00, 0x00, 0x00},
    // 8 (56)
    {0x36, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, 0x00},
    // 9 (57)
    {0x06, 0x49, 0x49, 0x29, 0x1E, 0x00, 0x00, 0x00},
    // : (58)
    {0x00, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00},
    // = (61)
    {0x14, 0x14, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00},
    // m (109)
    {0x7C, 0x04, 0x18, 0x04, 0x78, 0x00, 0x00, 0x00},
    // s (115)
    {0x48, 0x54, 0x54, 0x54, 0x20, 0x00, 0x00, 0x00}
};

const uint8_t Font_8x6_width = 8;
const uint8_t Font_8x6_height = 6;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
void OLED_SetPosition(uint8_t x, uint8_t page);
static void MX_TIM1_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void SetAudioEffect(uint8_t *colors, uint32_t amplitude);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HSVtoRGB(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t region, remainder, p, q, t;
    v = v * current_brightness / 255; // Áp dụng độ sáng
    if (s == 0) {
        *r = v; *g = v; *b = v;
        return;
    }
    region = h / 43;
    remainder = (h - (region * 43)) * 6;
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        case 5: *r = v; *g = p; *b = q; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

void SetRainbowEffect(uint8_t *colors, uint8_t offset, uint8_t current_brightness) {
    uint8_t r, g, b;
    for (uint32_t i = 0; i < NUM_LEDS; i++) {
        uint8_t hue = (i * 4 + offset) % 256;
        HSVtoRGB(hue, 255, 255, &r, &g, &b);
        colors[i * 3 + 0] = g;
        colors[i * 3 + 1] = r;
        colors[i * 3 + 2] = b;
    }
}

void SetFadeInOutEffect(uint8_t *colors, uint8_t hue, uint8_t brightness_fade) {
    uint8_t r, g, b;
    HSVtoRGB(hue, 255, brightness_fade, &r, &g, &b);
    for (uint32_t i = 0; i < NUM_LEDS; i++) {
        colors[i * 3 + 0] = g;
        colors[i * 3 + 1] = r;
        colors[i * 3 + 2] = b;
    }
}

void SetPixelRunEffect(uint8_t *colors, uint32_t step, uint8_t current_brightness) {
    for (uint32_t i = 0; i < NUM_LEDS; i++) {
        colors[i * 3 + 0] = 0; // G
        colors[i * 3 + 1] = 0; // R
        colors[i * 3 + 2] = 0; // B
    }
    uint8_t colors_rgb[COLOR_COUNT][3] = {
        {31, 226, 211}, // Cyan
        {218, 83, 83},  // Đỏ
        {127, 94, 213},  // Tím
		{255, 228, 196}
    };
    int8_t right_path[HALF_LEDS] = {
        0, 1, 2, 3, 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8,
        16, 17, 18, 19, 20, 21, 22, 23, 31, 30, 29, 28, 27, 26, 25, 24,
        16, 8, 0,
        1, 9, 17, 25,
        26, 18, 10, 2,
        3, 11, 19, 27,
        28, 20, 12, 4,
        5, 13, 21, 29,
        30, 22, 14, 6,
        7, 15, 23, 31
    };
    int8_t left_path[HALF_LEDS] = {
        56, 57, 58, 59, 60, 61, 62, 63, 55, 54, 53, 52, 51, 50, 49, 48,
        40, 41, 42, 43, 44, 45, 46, 47, 39, 38, 37, 36, 35, 34, 33, 32,
        40, 48, 56,
        57, 49, 41, 33,
        34, 42, 50, 58,
        59, 51, 43, 35,
        36, 44, 52, 60,
        61, 53, 45, 37,
        38, 46, 54, 62,
        63, 55, 47, 39
    };
    if (step < TOTAL_STEPS) {
        for (uint32_t i = 0; i < COLOR_COUNT; i++) {
            int32_t pos = step + i;
            if (pos >= 0 && pos < HALF_LEDS) {
                uint32_t led_index = right_path[pos];
                colors[led_index * 3 + 0] = colors_rgb[i][1] * current_brightness / 255;
                colors[led_index * 3 + 1] = colors_rgb[i][0] * current_brightness / 255;
                colors[led_index * 3 + 2] = colors_rgb[i][2] * current_brightness / 255;
            }
        }
        for (uint32_t i = 0; i < COLOR_COUNT; i++) {
            int32_t pos = step + i;
            if (pos >= 0 && pos < HALF_LEDS) {
                uint32_t led_index = left_path[pos];
                colors[led_index * 3 + 0] = colors_rgb[i][1] * current_brightness / 255;
                colors[led_index * 3 + 1] = colors_rgb[i][0] * current_brightness / 255;
                colors[led_index * 3 + 2] = colors_rgb[i][2] * current_brightness / 255;
            }
        }
    }
}

void SetStrobeEffect(uint8_t *colors, uint8_t current_brightness) {
    uint8_t r, g, b;
    for (uint32_t i = 0; i < NUM_LEDS; i++) {
        if (strobe_state) {
            HSVtoRGB(strobe_hue, 255, 255, &r, &g, &b);
            colors[i * 3 + 0] = g;
            colors[i * 3 + 1] = r;
            colors[i * 3 + 2] = b;
        } else {
            colors[i * 3 + 0] = 0;
            colors[i * 3 + 1] = 0;
            colors[i * 3 + 2] = 0;
        }
    }
    strobe_state = !strobe_state;
    if (strobe_state) {
        strobe_hue = (strobe_hue + 10) % 256;
    }
}

void SetAudioEffect(uint8_t *colors, uint32_t amplitude) {
    uint8_t r, g, b;
    for (uint32_t i = 0; i < NUM_LEDS * 3; i++) {
        colors[i] = 0;
    }

    uint8_t num_leds = 0;
    if (amplitude <= 1100) {
        num_leds = 2;
        HSVtoRGB(0, 0, 255, &r, &g, &b); // Trắng
    } else if (amplitude <= 1600) {
        num_leds = 4;
        HSVtoRGB(40, 255, 255, &r, &g, &b); // Vàng
    } else if (amplitude <= 2100) {
        num_leds = 6;
        HSVtoRGB(85, 255, 255, &r, &g, &b); // Xanh lá
    } else {
        num_leds = 8;
        HSVtoRGB(0, 255, 255, &r, &g, &b); // Đỏ
    }

    for (uint8_t col = 0; col < 8; col++) {
        uint32_t start_idx = col * 8;
        for (uint8_t led = 0; led < num_leds; led++) {
            uint32_t led_idx = start_idx + led;
            if (led_idx < NUM_LEDS) {
                colors[led_idx * 3 + 0] = g;
                colors[led_idx * 3 + 1] = r;
                colors[led_idx * 3 + 2] = b;
            }
        }
    }
}


void WS2812B_SetColor(uint8_t *colors, uint16_t *pwm_buffer) {
    for (uint32_t i = 0; i < NUM_LEDS; i++) {
        for (uint32_t j = 0; j < 24; j++) {
            uint8_t bit = (colors[i * 3 + j / 8] >> (7 - (j % 8))) & 0x01;
            pwm_buffer[i * 24 + j] = bit ? 67 : 34;
        }
    }
}

void WS2812B_Reset(void) {
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    HAL_Delay(100); // Tăng lên 100ms
}

void OLED_WriteCommand(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd}; // 0x00 là Control byte cho lệnh
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS << 1, data, 2, 100); // Timeout 100ms
}

void OLED_WriteData(uint8_t *data, uint16_t size) {
    uint8_t buffer[129];
    buffer[0] = 0x40; // 0x40 là Control byte cho dữ liệu
    for (uint16_t i = 0; i < size && i < 128; i++) {
        buffer[i + 1] = data[i];
    }
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS << 1, buffer, size + 1, 100); // Timeout 100ms
}

void OLED_Init(void) {
    HAL_Delay(100);
    OLED_WriteCommand(0xAE); // Tắt màn hình
    OLED_WriteCommand(0xD5); // Clock
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8); // Multiplex
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3); // Offset
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40); // Start line
    OLED_WriteCommand(0x8D); // Charge pump
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xA0); // Segment remap bình thường
    OLED_WriteCommand(0xC0); // COM scan từ trên xuống
    OLED_WriteCommand(0xDA); // Pins
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x20); // Memory mode
    OLED_WriteCommand(0x02); // Page addressing mode
    OLED_WriteCommand(0xAF); // Bật màn hình
}

void OLED_Fill(uint8_t pixel) {
    uint8_t data[OLED_WIDTH];
    memset(data, pixel ? 0xFF : 0x00, OLED_WIDTH);
    for (uint8_t page = 0; page < OLED_HEIGHT / OLED_PAGE_HEIGHT; page++) {
        OLED_SetPosition(0, page);
        OLED_WriteData(data, OLED_WIDTH);
    }
}

void OLED_SetPosition(uint8_t x, uint8_t page) {
    OLED_WriteCommand(0xB0 | page); // Đặt page
    OLED_WriteCommand(0x00 | (x & 0x0F)); // Đặt cột thấp
    OLED_WriteCommand(0x10 | ((x >> 4) & 0x0F)); // Đặt cột cao
}

// Hàm lấy font data theo ký tự
const uint8_t* GetFontData(char c) {
    if (c == ' ') return Font8x8[0];      // Space
    else if (c >= 'A' && c <= 'Z') return Font8x8[c - 'A' + 1];
    else if (c >= '0' && c <= '9') return Font8x8[c - '0' + 27];
    else if (c == ':') return Font8x8[37];
    else if (c == '=') return Font8x8[38];
    else if (c == 'm') return Font8x8[39];
    else if (c == 's') return Font8x8[40];
    else return Font8x8[0]; // Default to space
}

// Cập nhật hàm vẽ ký tự
void OLED_DrawChar(uint8_t x, uint8_t page, char c) {
    const uint8_t* font_data = GetFontData(c);
    OLED_SetPosition(x, page);
    OLED_WriteData((uint8_t*)font_data, 8);
}

// Cập nhật hàm vẽ chuỗi
void OLED_DrawString(uint8_t x, uint8_t page, const char *str) {
    uint8_t pos_x = x;
    while (*str && pos_x + 8 <= OLED_WIDTH) {
        OLED_DrawChar(pos_x, page, *str);
        pos_x += 8;
        str++;
    }
}

// Cập nhật hàm hiển thị trạng thái
void OLED_ShowStatus(uint8_t mode, uint8_t speed, uint8_t brightness) {
    char buf[20];
    const char *mode_names[] = {"RAINBOW", "FADE", "PIXELRUN", "STROBE"};

    OLED_Fill(0); // Xóa màn hình

    // Hiển thị chế độ
    sprintf(buf, "MODE: %s", mode_names[mode]);
    OLED_DrawString(0, 0, buf);

    // Hiển thị tốc độ
    sprintf(buf, "SPEED: %dms", speed_delays[speed]);
    OLED_DrawString(0, 2, buf);

    // Hiển thị độ sáng
    sprintf(buf, "BRIGHT: %d", brightness_levels[brightness]);
    OLED_DrawString(0, 4, buf);
}

// Cập nhật hàm hiển thị khởi động
void OLED_ShowStartup(void) {
    OLED_Init();
    OLED_Fill(0); // Xóa màn hình
    OLED_DrawString(0, 1, "LED CONTROLLER");
    OLED_DrawString(0, 3, "INIT COMPLETE");
    HAL_Delay(1000);
}

// Cập nhật hàm update OLED
void update_oled(void) {
    OLED_ShowStatus(current_mode, current_speed, brightness_index);
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
  MX_TIM1_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
    OLED_Init();
    OLED_ShowStartup();
    update_oled();

    for (uint32_t i = 0; i < NUM_LEDS; i++) {
        led_colors[i * 3 + 0] = 0;
        led_colors[i * 3 + 1] = 0;
        led_colors[i * 3 + 2] = 0;
    }
    WS2812B_SetColor(led_colors, pwm_buffer);
    if (HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)pwm_buffer, PWM_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
    dma_complete = 0;
    HAL_Delay(1000);

    HAL_TIM_Base_Start(&htim3);
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, ADC_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        if (dma_complete && HAL_DMA_GetState(&hdma_tim1_ch1) == HAL_DMA_STATE_READY) {
            dma_complete = 0;
            HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
            HAL_Delay(1);
            WS2812B_Reset();

            // Chọn hiệu ứng dựa trên current_mode
            switch (current_mode) {
                case 0:
                    SetRainbowEffect(led_colors, rainbow_offset++, current_brightness);
                    break;
                case 1:
                    SetFadeInOutEffect(led_colors, fade_hue, brightness_fade);
                    if (fade_direction) {
                        brightness_fade += 5;
                        if (brightness_fade >= 255) {
                            brightness_fade = 255;
                            fade_direction = 0;
                        }
                    } else {
                        brightness_fade -= 5;
                        if (brightness_fade <= 0) {
                            brightness_fade = 0;
                            fade_direction = 1;
                            fade_hue = (fade_hue + 85) % 256;
                        }
                    }
                    break;
                case 2:
                    if (animation_step < TOTAL_STEPS) {
                        SetPixelRunEffect(led_colors, animation_step++, current_brightness);
                    } else {
                        animation_step = 0;
                        SetPixelRunEffect(led_colors, animation_step++, current_brightness);
                    }
                    break;
                case 3:
                    SetStrobeEffect(led_colors, current_brightness);
                    break;
                case 4:
                    SetAudioEffect(led_colors, avg_amplitude); // Sử dụng giá trị toàn cục
                    break;
            }

            WS2812B_SetColor(led_colors, pwm_buffer);
            if (HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)pwm_buffer, PWM_BUFFER_SIZE) != HAL_OK) {
                HAL_DMA_Abort(&hdma_tim1_ch1);
                HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
            }
            if (oled_needs_update) {
                update_oled();
                oled_needs_update = 0;
            }
        }
        // Điều khiển tốc độ bằng thời gian không chặn
            if (HAL_GetTick() - last_update >= speed_delays[current_speed]) {
                last_update = HAL_GetTick();
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    sConfig.Channel = ADC_CHANNEL_3;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) Error_Handler();
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 104;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 2624;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pins : MODE_BTN_Pin SPEED_BTN_Pin BRIGHTNESS_BTN_Pin */
  GPIO_InitStruct.Pin = MODE_BTN_Pin|SPEED_BTN_Pin|BRIGHTNESS_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static uint32_t last_press = 0;
    if (HAL_GetTick() - last_press < 500) return;
    last_press = HAL_GetTick();

    if (GPIO_Pin == MODE_BTN_Pin) {
        current_mode = (current_mode + 1) % NUM_MODES; // Tăng mode lên 5
        animation_step = 0;
        strobe_state = 0;
        strobe_hue = 0;
        oled_needs_update = 1;
    } else if (GPIO_Pin == SPEED_BTN_Pin) {
        current_speed = (current_speed + 1) % 4;
        oled_needs_update = 1;
    } else if (GPIO_Pin == BRIGHTNESS_BTN_Pin) {
        brightness_index = (brightness_index + 1) % 4;
        current_brightness = brightness_levels[brightness_index];
        oled_needs_update = 1;
    }
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM1) {
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
        dma_complete = 1;
    }
}

void HAL_DMA_ErrorCallback(DMA_HandleTypeDef *hdma) {
    if (hdma->Instance == DMA2_Stream1) {
        HAL_DMA_Abort(&hdma_tim1_ch1);
        HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        uint32_t sum_squares = 0;
        const uint32_t offset = 2048; // DC offset của MAX9814 (~1.65V)
        for (uint32_t i = 0; i < ADC_BUFFER_SIZE; i++) {
            int32_t sample = (int32_t)adc_buffer[i] - offset;
            sum_squares += sample * sample;
        }
        float rms = sqrt((float)sum_squares / ADC_BUFFER_SIZE);
        avg_amplitude = (uint32_t)(rms * 2.0f); // Nhân 2 để ước lượng peak
        if (avg_amplitude > 2000) avg_amplitude = 3500; // Giới hạn
        adc_data_ready = 1;
    }
}
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
