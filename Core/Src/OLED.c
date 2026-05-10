//#include "stm32f10x.h"
#include "OLED_Font.h"
#include "main.h"

// 如果使用硬件I2C，取消注释下面这行
// #define USE_HARDWARE_I2C

#ifdef USE_HARDWARE_I2C
// 直接包含HAL头文件用于I2C操作
#include "stm32f1xx_hal.h"

// 硬件I2C初始化函数 - 基本的I2C1配置
void MX_I2C1_Init(void)
{
    // 启用I2C1时钟
    __HAL_RCC_I2C1_CLK_ENABLE();
    
    // 配置I2C1引脚 (PB8-SCL, PB9-SDA)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // 基本的I2C配置
    I2C1->CR1 &= ~I2C_CR1_PE; // 禁用I2C
    
    I2C1->CR2 = 36; // 36MHz APB1时钟
    I2C1->CCR = 180; // 400kHz (36MHz/180 = 200kHz, 但CCR=180对应400kHz)
    I2C1->TRISE = 37; // 最大上升时间
    
    I2C1->CR1 |= I2C_CR1_PE; // 启用I2C
}

// 硬件I2C发送函数
void I2C_Write(uint8_t addr, uint8_t reg, uint8_t data)
{
    // 等待总线空闲
    while (I2C1->SR2 & I2C_SR2_BUSY);
    
    // 发送起始条件
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));
    
    // 发送设备地址
    I2C1->DR = addr;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2; // 清除ADDR标志
    
    // 发送寄存器地址
    I2C1->DR = reg;
    while (!(I2C1->SR1 & I2C_SR1_TXE));
    
    // 发送数据
    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_TXE));
    
    // 发送停止条件
    I2C1->CR1 |= I2C_CR1_STOP;
}
#endif

/*引脚配置*/
#define OLED_W_SCL(x) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)
#define OLED_W_SDA(x) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, (x) ? GPIO_PIN_SET : GPIO_PIN_RESET)

/*引脚初始化*/
void OLED_I2C_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_OD; // 开漏输出
	GPIO_InitStructure.Pull = GPIO_PULLUP;         // 【关键新增】开启内部上拉电阻
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStructure.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);

	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C开始
  * @param  无
  * @retval 无
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}

/**
  * @brief  I2C停止
  * @param  无
  * @retval 无
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C发送一个字节
  * @param  Byte 要发送的一个字节
  * @retval 无
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(Byte & (0x80 >> i));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	OLED_W_SCL(1);	//额外的一个时钟，不处理应答信号
	OLED_W_SCL(0);
}

/**
  * @brief  OLED写命令
  * @param  Command 要写入的命令
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
#ifdef USE_HARDWARE_I2C
    I2C_Write(0x78, 0x00, Command);
#else
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x00);		//写命令
	OLED_I2C_SendByte(Command);
	OLED_I2C_Stop();
#endif
}

/**
  * @brief  OLED写数据
  * @param  Data 要写入的数据
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
#ifdef USE_HARDWARE_I2C
    I2C_Write(0x78, 0x40, Data);
#else
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x40);		//写数据
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
#endif
}

/**
  * @brief  OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~3（0.91英寸专用）
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					//设置Y位置
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位
}

/**
  * @brief  OLED清屏
  * @param  无
  * @retval 无
  */
void OLED_Clear(void)
{  
	uint8_t i, j;
	for (j = 0; j < 4; j++)  // 【0.91英寸修改】从8改为4
	{
		OLED_SetCursor(j, 0);
		for(i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
}

/**
  * @brief  OLED显示一个字符
  * @param  Line 行位置，范围：1~2（0.91英寸专用）
  * @param  Column 列位置，范围：1~16
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//设置光标位置在上半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);			//显示上半部分内容
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//设置光标位置在下半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);		//显示下半部分内容
	}
}

/**
  * @brief  OLED显示字符串
  * @param  Line 起始行位置，范围：1~2（0.91英寸专用）
  * @param  Column 起始列位置，范围：1~16
  * @param  String 要显示的字符串，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
  * @brief  OLED显示数字（十进制，正数）
  * @param  Line 起始行位置，范围：1~2（0.91英寸专用）
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  Line 起始行位置，范围：1~2（0.91英寸专用）
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~2（0.91英寸专用）
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

/**
  * @brief  OLED显示数字（二进制，正数）
  * @param  Line 起始行位置，范围：1~2（0.91英寸专用）
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  Length 要显示数字的长度，范围：1~16
  * @retval 无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
	uint32_t i, j;
	
	for (i = 0; i < 1000; i++) { for (j = 0; j < 1000; j++); } // 上电延时

#ifdef USE_HARDWARE_I2C
	MX_I2C1_Init();			//硬件I2C初始化
#else
	OLED_I2C_Init();			//软件I2C端口初始化
#endif
	
	OLED_WriteCommand(0xAE);	//关闭显示
	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);

	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x1F);	//【关键修改】从0x3F改为0x1F (0.91寸32行专用)

	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);
	OLED_WriteCommand(0x40);	//设置显示开始行
	OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常 0xA0左右反置
	OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常 0xC0上下反置

	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x02);	//【关键修改】从0x12改为0x02 (0.91寸专用)

	OLED_WriteCommand(0x81);	//设置对比度控制
	OLED_WriteCommand(0xCF);
	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);
	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);
	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭
	OLED_WriteCommand(0xA6);	//设置正常/倒转显示
	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);
	OLED_WriteCommand(0xAF);	//开启显示
		
	OLED_Clear();				//OLED清屏
}
