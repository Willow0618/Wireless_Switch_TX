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
#include "adc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "OLED.h"
#include "nrf24l01p.h"
// 如果使用硬件I2C，取消注释下面这行
// #include "stm32f1xx_hal_i2c.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NRF_PAIR_ID 5u  // nrf配对编号

static const uint8_t nrf_pair_addrs[][6] = {
  {0x12, 0x34, 0x56, 0x78, 0x9A}, // ID0
  {0xA1, 0xB2, 0xC3, 0xD4, 0xE5}, // ID1
  {0x55, 0x66, 0x77, 0x88, 0x99}, // ID2
  {0xAB, 0xCD, 0xEF, 0x11, 0x22}, // ID3
  {0x13, 0x57, 0x9B, 0xDF, 0x24}, // ID4
  {0x14, 0x58, 0x97, 0xD1, 0x25}, // ID5
};

// -------- 1. 电池参数 --------
// 电压计算系数：(3.3V / 4095) * 分压比 * 10 (转为0.1V)
// 原始代码逻辑: round(adc * 132 / 4095) -> 这里的 132 是经验值
#define BATT_CALC_FACTOR      132u
#define BATT_ADC_MAX          4095u
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// adc_raw -> 0.1V 单位（四舍五入）
uint16_t Battery_AdcTo_0p1V(uint16_t adc_raw)
{
  return (uint16_t)(((uint32_t)adc_raw * BATT_CALC_FACTOR + (BATT_ADC_MAX / 2)) / BATT_ADC_MAX);//+2047是为了四舍五入
}

// 0.1V 单位 -> "x.yV"（手动拼字符，不用sprintf）
void Battery_0p1V_ToText(char *buf, uint16_t v_0p1)
{
  uint16_t v_int  = v_0p1 / 10u;  // 整数伏
  uint16_t v_frac = v_0p1 % 10u;  // 0.1伏

  // v_int 最大大概 0~13（单节锂电），所以最多两位
  int i = 0;

  if (v_int >= 10u) {
    buf[i++] = (char)('0' + (v_int / 10u));
    buf[i++] = (char)('0' + (v_int % 10u));
  } else {
    buf[i++] = (char)('0' + v_int);
  }

  buf[i++] = '.';
  buf[i++] = (char)('0' + v_frac);
  buf[i++] = 'V';
  buf[i++] = '\0';
}

//=====================电池end==================//

// 防弹版：将指令和数据打包成一个数组，一口气发出去，消除时钟间隙
void NRF_Force_Write_Reg(uint8_t reg, uint8_t value) {
  uint8_t buf[2] = { NRF24L01P_CMD_W_REGISTER | reg, value };
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, buf, 2, 100); // 连续发2个字节
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
}

// 防弹版：使用 TransmitReceive 边发伪数据(0xFF)边接收真实数据
uint8_t NRF_Force_Read_Reg(uint8_t reg) {
  uint8_t tx_buf[2] = { NRF24L01P_CMD_R_REGISTER | reg, 0xFF };
  uint8_t rx_buf[2] = { 0, 0 };
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, 2, 100); // 边发边收
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
  return rx_buf[1]; // 返回第二个字节（真实数据）
}

// 防弹版：打包发送 5 字节地址
void NRF_Force_Set_Addr(uint8_t reg, uint8_t* addr) {
  uint8_t tx_buf[6];
  tx_buf[0] = NRF24L01P_CMD_W_REGISTER | reg;
  for(int i=0; i<5; i++) tx_buf[i+1] = addr[i];

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, tx_buf, 6, 100); // 连续发6个字节
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
}

// 封装一个强力初始化函数，方便自愈调用
void Safe_NRF_Init(void) {
  nrf24l01p_tx_init(2500, _1Mbps);
  nrf24l01p_set_rf_tx_output_power(_12dBm); // 【关键修复1】降低功率，防止电池电压被拉垮导致单片机重启！

  uint8_t activate_cmd[2] = {0x50, 0x73};
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, activate_cmd, 2, 100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

  NRF_Force_Write_Reg(NRF24L01P_REG_FEATURE, 0x06);
  NRF_Force_Write_Reg(NRF24L01P_REG_SETUP_RETR, 0x5F);
  NRF_Force_Write_Reg(NRF24L01P_REG_EN_AA, 0x01);
  NRF_Force_Write_Reg(NRF24L01P_REG_DYNPD, 0x01);
  NRF_Force_Write_Reg(NRF24L01P_REG_SETUP_AW, 0x03);
  NRF_Force_Write_Reg(NRF24L01P_REG_EN_RXADDR, 0x01);
  NRF_Force_Write_Reg(NRF24L01P_REG_RX_PW_P0, 8);

  const uint8_t *tx_addr = nrf_pair_addrs[NRF_PAIR_ID];
  NRF_Force_Set_Addr(NRF24L01P_REG_TX_ADDR, (uint8_t *)tx_addr);
  NRF_Force_Set_Addr(NRF24L01P_REG_RX_ADDR_P0, (uint8_t *)tx_addr);

  uint8_t config = NRF_Force_Read_Reg(NRF24L01P_REG_CONFIG);
  config |= 0x70;
  NRF_Force_Write_Reg(NRF24L01P_REG_CONFIG, config);

  nrf24l01p_flush_rx_fifo();
  nrf24l01p_flush_tx_fifo();
  NRF_Force_Write_Reg(NRF24L01P_REG_STATUS, 0x70);
}

// 记录远端开关的状态（0=关，1=开）
uint8_t remote_switch_state = 0;

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
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  // 如果使用硬件I2C，取消注释下面这行
  // MX_I2C1_Init();  // 现在在OLED.c中定义
  /* USER CODE BEGIN 2 */
  // 1. OLED 初始化与界面框架
  OLED_Init();
  OLED_Clear();
  // // 0.91寸(128x32)屏幕只有2行显示：Line 1/2
  OLED_ShowString(1, 1, "Bat:     ");
  OLED_ShowString(2, 1, "SW:OFF Sig:WAIT"); // 初始状态

  // 显示编号
  char id_str[6];
  sprintf(id_str, "ID:%d", NRF_PAIR_ID);
  OLED_ShowString(1, 11, id_str);
  
  nrf24l01p_tx_init(2500, _1Mbps);

  // ===================== 终极安全底层配置 =====================
  // 1. 安全解锁特性寄存器 (防止二次反转锁定)
  uint8_t feature = NRF_Force_Read_Reg(NRF24L01P_REG_FEATURE);
  if ((feature & 0x06) != 0x06) {
    NRF_Force_Write_Reg(NRF24L01P_REG_FEATURE, 0x06);
    if ((NRF_Force_Read_Reg(NRF24L01P_REG_FEATURE) & 0x06) != 0x06) {
      uint8_t activate_cmd[2] = {0x50, 0x73};
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
      HAL_SPI_Transmit(&hspi1, activate_cmd, 2, 100);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
      NRF_Force_Write_Reg(NRF24L01P_REG_FEATURE, 0x06);
    }
  }

  // 2. 覆盖带 Bug 的库函数，强制写入正确的重发与应答参数
  NRF_Force_Write_Reg(NRF24L01P_REG_SETUP_RETR, 0x5F); // ARD=1500us, ARC=15 (至关重要)
  NRF_Force_Write_Reg(NRF24L01P_REG_EN_AA, 0x01);
  NRF_Force_Write_Reg(NRF24L01P_REG_DYNPD, 0x01);
  NRF_Force_Write_Reg(NRF24L01P_REG_SETUP_AW, 0x03);
  NRF_Force_Write_Reg(NRF24L01P_REG_EN_RXADDR, 0x01);
  NRF_Force_Write_Reg(NRF24L01P_REG_RX_PW_P0, 8);      // 强制 8 字节，防乱码

  // 3. 配置地址
  const uint8_t *tx_addr = nrf_pair_addrs[NRF_PAIR_ID];
  NRF_Force_Set_Addr(NRF24L01P_REG_TX_ADDR, (uint8_t *)tx_addr);
  NRF_Force_Set_Addr(NRF24L01P_REG_RX_ADDR_P0, (uint8_t *)tx_addr);

  // 4. 屏蔽硬件中断引脚，防止单片机被外部干扰
  uint8_t config = NRF_Force_Read_Reg(NRF24L01P_REG_CONFIG);
  config |= 0x70;
  NRF_Force_Write_Reg(NRF24L01P_REG_CONFIG, config);

  nrf24l01p_flush_rx_fifo();
  nrf24l01p_flush_tx_fifo();
  NRF_Force_Write_Reg(NRF24L01P_REG_STATUS, 0x70);
  printf("TX Initialization Complete.\r\n");

  uint8_t tx_data[8] = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xFF};
  char oled_buf[16];
  GPIO_PinState last_btn_state = GPIO_PIN_SET;
  uint32_t ping_counter = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    // =================== 读取并显示电池电量 ===================
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint16_t adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    uint16_t voltage_0p1 = Battery_AdcTo_0p1V(adc_value);
    sprintf(oled_buf, "%d.%dV ", voltage_0p1 / 10, voltage_0p1 % 10);
    OLED_ShowString(1, 5, oled_buf); // 更新电压显示

    // 将电压数据打包，准备发送
    tx_data[1] = (uint8_t)(voltage_0p1 >> 8);
    tx_data[2] = (uint8_t)(voltage_0p1 & 0xFF);

    // =================== 3. 按键检测 与 数据发送逻辑 ===================
    GPIO_PinState current_btn_state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
    uint8_t need_to_send = 0;

    if (current_btn_state != last_btn_state) {
      HAL_Delay(20); // 软件消抖
      current_btn_state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);

      if (current_btn_state != last_btn_state) {
        tx_data[3] = (current_btn_state == GPIO_PIN_RESET) ? 0x00 : 0x01;
        printf("[TX] Button Changed: %d\r\n", tx_data[3]);
        need_to_send = 1;
        ping_counter = 0;
        last_btn_state = current_btn_state;
      }
    } else {
      // 如果没有按键动作，每过约 1 秒钟主动发一次"心跳"查询功率
      ping_counter++;
      if (ping_counter >= 50) { // 50 * 20ms = 1000ms
        ping_counter = 0;
        tx_data[3] = 0xFF; // 0xFF 代表纯查询，RX收到后不改变开关状态
        need_to_send = 1;
      }
    }

    // 执行发送操作
     if (need_to_send) {
       // 【关键修复2】发送前健康检查，如果 NRF 跑飞了瞬间重启它
       if ((NRF_Force_Read_Reg(NRF24L01P_REG_SETUP_AW) & 0x03) != 0x03) {
         Safe_NRF_Init();
       }
      NRF_Force_Write_Reg(NRF24L01P_REG_STATUS, 0x70); // 清除状态位
      nrf24l01p_flush_rx_fifo(); // 发送前强清 RX 避免幽灵数据
      nrf24l01p_flush_tx_fifo();
      nrf24l01p_tx_transmit(tx_data);

      // CE 脉冲触发发射
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
      HAL_Delay(1);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

      // ============ 循环死等发送完毕 ============
      uint32_t timeout = 0;
      uint8_t status = 0;
      while (1) {
          status = NRF_Force_Read_Reg(NRF24L01P_REG_STATUS);
          if (status & 0x30) break; // TX_DS 或 MAX_RT 置位说明发完了
          if (++timeout > 50) break;    // 50ms 超时
          HAL_Delay(1);
      }


        // ============ 检查是否带回了功率数据 ============
        if (status & 0x40)
        {
          // RX_DR 表明收到了 ACK Payload
          uint8_t wid_cmd = 0x60; // 读取真实长度指令
          uint8_t wid = 0;
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
          HAL_SPI_Transmit(&hspi1, &wid_cmd, 1, 100);
          HAL_SPI_Receive(&hspi1, &wid, 1, 100);
          HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

          if (wid > 0 && wid <= 32) {
            uint8_t rx_payload[32] = {0};
            uint8_t read_cmd = 0x61;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
            HAL_SPI_Transmit(&hspi1, &read_cmd, 1, 100);
            HAL_SPI_Receive(&hspi1, rx_payload, wid, 100);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

            // 打印 TX 收到的 RX 返回数据 (带负载ACK)
            printf("[TX] Recv ACK Payload (Len=%d): %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                   wid, rx_payload[0], rx_payload[1], rx_payload[2], rx_payload[3],
                   rx_payload[4], rx_payload[5], rx_payload[6], rx_payload[7]);

            if (rx_payload[0] == 0xBB) {
              // 此时解析出来的 power 是毫瓦 (mW)
              uint16_t power_mw = (rx_payload[1] << 8) | rx_payload[2];
              // 提取第 3 字节的 MOS 管状态
              uint8_t rx_mos_state = rx_payload[3];
              char power_str[16];
              // %d 表示整数部分 (W)
              // %03d 表示小数部分，保留3位不够补零 (比如 20mW 显示为 .020)
              // 最后的 4 个空格用来彻底覆盖掉屏幕残留的 ig:WAIT
              // 根据状态拼接不同的字符串，同时保证末尾对齐覆盖
              if (rx_mos_state == 1) { // 1 代表高电平 (ON)
                sprintf(power_str, "P=%d.%03dW  ON    ", power_mw / 1000, power_mw % 1000);
              } else {                 // 0 代表低电平 (OFF)
                sprintf(power_str, "P=%d.%03dW  OFF   ", power_mw / 1000, power_mw % 1000);
              }

              OLED_ShowString(2, 1, power_str);
            }
          }
        }
        NRF_Force_Write_Reg(NRF24L01P_REG_STATUS, 0x70); // 彻底打扫战场
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV8;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
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
#ifdef USE_FULL_ASSERT
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
