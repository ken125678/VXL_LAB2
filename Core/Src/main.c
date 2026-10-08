/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define MAX_LED 4

int index_led = 0;
volatile int led_buffer[MAX_LED] = {1, 2, 3, 0};

int hour = 15, minute = 8, second = 50;

// Bien dem cho cac bai quet LED bang ngat (EX1 -> EX5, EX7)
int seg_counter = 50;
int dot_counter = 100;
int led_status  = 0;

void display7SEG(int num) {
    const uint8_t segmentTable[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                      0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    if (num < 0 || num > 9) return;
    uint8_t pattern = segmentTable[num];
    for (int i = 0; i < 7; i++) {
        HAL_GPIO_WritePin(GPIOB, (uint16_t)(GPIO_PIN_0 << i),
                          (pattern & (1 << i)) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}

void turnOffAll7SEG(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9,
                      GPIO_PIN_SET);
}

// EX3: hien thi LED thu index voi gia tri led_buffer[index]
void update7SEG(int index) {
    turnOffAll7SEG();   // tat het truoc de chong bong ma
    switch (index) {
        case 0:
            display7SEG(led_buffer[0]);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
            break;
        case 1:
            display7SEG(led_buffer[1]);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
            break;
        case 2:
            display7SEG(led_buffer[2]);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
            break;
        case 3:
            display7SEG(led_buffer[3]);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
            break;
        default:
            break;
    }
}

// EX5: tach gio/phut thanh 4 chu so, them so 0 o dau neu can
void updateClockBuffer(void) {
    led_buffer[0] = hour / 10;
    led_buffer[1] = hour % 10;
    led_buffer[2] = minute / 10;
    led_buffer[3] = minute % 10;
}

void updateClock(void) {
    second++;
    if (second >= 60) { second = 0; minute++; }
    if (minute >= 60) { minute = 0; hour++; }
    if (hour >= 24)   { hour = 0; }
}

// EX6: software timer
// timer0: dong ho / LED do, timer1: quet 7SEG,
// timer2: quet LED matrix (EX9), timer3: animation matrix (EX10)
volatile int timer0_counter = 0;
volatile int timer0_flag = 0;
volatile int timer1_counter = 0;
volatile int timer1_flag = 0;
volatile int timer2_counter = 0;
volatile int timer2_flag = 0;
volatile int timer3_counter = 0;
volatile int timer3_flag = 0;
int TIMER_CYCLE = 10;   // chu ky ngat TIM2 = 10ms

void setTimer0(int duration) {
    timer0_flag = 0;                          // xoa flag TRUOC khi nap counter
    timer0_counter = duration / TIMER_CYCLE;
}

void setTimer1(int duration) {
    timer1_flag = 0;                          // xoa flag TRUOC khi nap counter
    timer1_counter = duration / TIMER_CYCLE;
}

void setTimer2(int duration) {
    timer2_flag = 0;                          // xoa flag TRUOC khi nap counter
    timer2_counter = duration / TIMER_CYCLE;
}

void setTimer3(int duration) {
    timer3_flag = 0;                          // xoa flag TRUOC khi nap counter
    timer3_counter = duration / TIMER_CYCLE;
}

void timer_run(void) {
    if (timer0_counter > 0) {
        timer0_counter--;
        if (timer0_counter == 0) timer0_flag = 1;
    }
    if (timer1_counter > 0) {
        timer1_counter--;
        if (timer1_counter == 0) timer1_flag = 1;
    }
    if (timer2_counter > 0) {
        timer2_counter--;
        if (timer2_counter == 0) timer2_flag = 1;
    }
    if (timer3_counter > 0) {
        timer3_counter--;
        if (timer3_counter == 0) timer3_flag = 1;
    }
}

// EX9: LED matrix 8x8
// ROW0..ROW7 = PB8..PB15 : du lieu cua 1 cot (bit i -> ROWi)
// ENM0..ENM7 = PA2, PA3, PA10, PA11, PA12, PA13, PA14, PA15 : chon cot qua ULN2803
//              (theo Figure 1.7: COL co dien tro keo len +3.3V,
//               ENM = 0 -> ULN2803 tat -> COL len 3.3V -> cot duoc cap dien;
//               ROW = 0 -> LED o hang do sang)
// PA13/PA14 mac dinh la SWD -> matrixGPIOInit() tat SWD de dung lam GPIO.
// Luu y: tren mach that, sau khi tat SWD phai nap lai bang "Connect under reset".
#define MAX_LED_MATRIX 8
#define MATRIX_ROW_ON   GPIO_PIN_SET   // hinh bi am ban (nen sang, chu toi): doi 2 dong nay
#define MATRIX_ROW_OFF  GPIO_PIN_RESET
#define MATRIX_COL_ON   GPIO_PIN_SET// sang ca matrix / sai cot: doi 2 dong nay
#define MATRIX_COL_OFF  GPIO_PIN_RESET
#define ENM_ALL (GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_10 | GPIO_PIN_11 | \
                 GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)
#define ROW_ALL (GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | \
                 GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)

const uint16_t ENM_PIN[MAX_LED_MATRIX] = {GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_10,
                                          GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13,
                                          GPIO_PIN_14, GPIO_PIN_15};

int index_led_matrix = 0;
// Chu "A", moi byte la 1 cot (trai -> phai), bit0 = hang tren cung
//   ..XXXX..
//   .XX..XX.
//   XX....XX
//   XX....XX
//   XXXXXXXX
//   XX....XX
//   XX....XX
//   ........
uint8_t matrix_buffer[MAX_LED_MATRIX] = {0x7C, 0x7E, 0x13, 0x11,
                                         0x11, 0x13, 0x7E, 0x7C};

// Cac chan matrix chua cau hinh trong .ioc nen khoi tao o day
// (nam trong USER CODE nen khong bi mat khi generate lai)
void matrixGPIOInit(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_AFIO_REMAP_SWJ_DISABLE();   // giai phong PA13, PA14 (SWD) lam GPIO
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_WritePin(GPIOA, ENM_ALL, MATRIX_COL_OFF);
    GPIO_InitStruct.Pin = ENM_ALL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, ROW_ALL, MATRIX_ROW_OFF);
    GPIO_InitStruct.Pin = ROW_ALL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void setMatrixRow(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        HAL_GPIO_WritePin(GPIOB, (uint16_t)(GPIO_PIN_8 << i),
                          ((data >> i) & 1) ? MATRIX_ROW_ON : MATRIX_ROW_OFF);
    }
}

// EX9: hien thi cot thu index voi du lieu matrix_buffer[index]
void updateLEDMatrix(int index) {
    HAL_GPIO_WritePin(GPIOA, ENM_ALL, MATRIX_COL_OFF);   // tat het, chong bong ma
    switch (index) {
        case 0: case 1: case 2: case 3:
        case 4: case 5: case 6: case 7:
            setMatrixRow(matrix_buffer[index]);
            HAL_GPIO_WritePin(GPIOA, ENM_PIN[index], MATRIX_COL_ON);
            break;
        default:
            break;
    }
}

// EX10: dich chu sang trai 1 cot, cot dau vong ve cuoi (chay vong)
void shiftLeftMatrix(void) {
    uint8_t first = matrix_buffer[0];
    for (int i = 0; i < MAX_LED_MATRIX - 1; i++) {
        matrix_buffer[i] = matrix_buffer[i + 1];
    }
    matrix_buffer[MAX_LED_MATRIX - 1] = first;
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
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  // Dung chung: tat het 7SEG va cac doan, tat DOT va LED do truoc khi chay
  turnOffAll7SEG();
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                           GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, ENM_ALL, MATRIX_COL_OFF);   // tat LED matrix
  HAL_GPIO_WritePin(GPIOB, ROW_ALL, MATRIX_ROW_OFF);

  /* ===== EX3 [INIT]: doi led_buffer de unit test update7SEG =====
  led_buffer[0] = 1; led_buffer[1] = 2; led_buffer[2] = 3; led_buffer[3] = 4;
  // */

/* ===== EX5 [INIT]: nap gio ban dau vao buffer =====
  updateClockBuffer();
  // */

  /* ===== EX6 [INIT] =====
  setTimer0(1000);      // Report 1/2/3: thu xoa dong nay, doi thanh 1 hoac 10
  // */

 /* ===== EX7 [INIT] =====
  updateClockBuffer();
  setTimer0(1000);      // timer0: dong ho + DOT, 1s
  // */

 /* ===== EX8 [INIT] =====
  updateClockBuffer();
  setTimer0(1000);      // timer0: dong ho + DOT, 1s
  setTimer1(250);       // timer1: quet 7SEG, 250ms moi LED -> 1Hz
  // */

 //* ===== EX9 [INIT]: bat THEM cac block EX8 =====
  matrixGPIOInit();     // cau hinh chan matrix (ca PA13, PA14)
  setTimer2(10);        // timer2: quet matrix, 10ms moi cot
  // */

 //* ===== EX10 [INIT]: bat THEM cac block EX8 + EX9 =====
  setTimer3(300);       // timer3: dich chu moi 300ms
  // */

  HAL_TIM_Base_Start_IT(&htim2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* ===== EX5 [WHILE]: dong ho dung HAL_Delay (blocking) =====
      updateClock();
      updateClockBuffer();
      HAL_Delay(1000);
      // */

     /* ===== EX6 [WHILE]: nhay LED do PA5 bang software timer =====
      if (timer0_flag == 1) {
          HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
          setTimer0(2000);
      }
      // */

     /* ===== EX7 [WHILE]: dong ho + DOT o main, quet 7SEG van o ngat =====
      if (timer0_flag == 1) {
          setTimer0(1000);
          updateClock();
          updateClockBuffer();
          HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);   // DOT
      }
      // */

     /* ===== EX8 [WHILE]: moi xu ly deu o main =====
      if (timer1_flag == 1) {
          setTimer1(250);
          update7SEG(index_led++);
          if (index_led >= MAX_LED) index_led = 0;
      }
      if (timer0_flag == 1) {
          setTimer0(1000);
          updateClock();
          updateClockBuffer();
          HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);   // DOT
      }
      // */

    //* ===== EX9 [WHILE]: quet LED matrix hien chu "A" =====
      if (timer2_flag == 1) {
          setTimer2(10);
          updateLEDMatrix(index_led_matrix++);
          if (index_led_matrix >= MAX_LED_MATRIX) index_led_matrix = 0;
      }
      // */

    //* ===== EX10 [WHILE]: animation - chu "A" chay sang trai =====
      if (timer3_flag == 1) {
          setTimer3(300);
          shiftLeftMatrix();
      }
      // */
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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA2 PA3 PA4 PA5
                           PA6 PA7 PA8 PA9
                           PA10 PA11 PA12 PA13
                           PA14 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9
                          |GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 PB10
                           PB11 PB12 PB13 PB14
                           PB15 PB3 PB4 PB5
                           PB6 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10
                          |GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance != TIM2) return;

    /* ===== EX1 [ISR]: 2 LED, "1" va "2", doi moi 500ms (f quet = 1Hz) =====
    seg_counter--;
    if (seg_counter <= 0) {
        seg_counter = 50;
        turnOffAll7SEG();
        if (led_status == 0) {
            display7SEG(1);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);   // EN0
            led_status = 1;
        } else {
            display7SEG(2);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);   // EN1
            led_status = 0;
        }
    }
    // */

    //* ===== EX2 [ISR]: 4 LED "1 2 3 0", moi LED 500ms (f = 0.5Hz), DOT 1s =====
    dot_counter--;
    if (dot_counter <= 0) {
        dot_counter = 100;
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);
    }
    seg_counter--;
    if (seg_counter <= 0) {
        seg_counter = 50;
        turnOffAll7SEG();
        switch (led_status) {
            case 0: display7SEG(1); HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET); break;
            case 1: display7SEG(2); HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET); break;
            case 2: display7SEG(3); HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET); break;
            case 3: display7SEG(0); HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET); break;
        }
        led_status = (led_status + 1) % 4;
    }
    // */

    /* ===== EX3 [ISR]: dung update7SEG, moi LED 500ms, DOT 1s =====
    dot_counter--;
    if (dot_counter <= 0) {
        dot_counter = 100;
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);
    }
    seg_counter--;
    if (seg_counter <= 0) {
        seg_counter = 50;
        update7SEG(index_led++);
        if (index_led >= MAX_LED) index_led = 0;
    }
    // */

 /* ===== EX4 + EX5 [ISR]: moi LED 250ms -> 4 LED = 1Hz, DOT 1s =====
    dot_counter--;
    if (dot_counter <= 0) {
        dot_counter = 100;
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);
    }
    seg_counter--;
    if (seg_counter <= 0) {
        seg_counter = 25;
        update7SEG(index_led++);
        if (index_led >= MAX_LED) index_led = 0;
    }
    // */

 //* ===== EX6 + EX8 [ISR]: ngat chi chay software timer (EX9, EX10 dung chung) =====
    timer_run();
    // */

  /* ===== EX7 [ISR]: software timer + quet 7SEG (DOT da chuyen len main) =====
    timer_run();
    seg_counter--;
    if (seg_counter <= 0) {
        seg_counter = 25;
        update7SEG(index_led++);
        if (index_led >= MAX_LED) index_led = 0;
    }
    // */
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
