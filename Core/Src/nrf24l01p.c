/*
 *  nrf24l01_plus.c
 *
 *  Created on: 2021. 7. 20.
 *      Author: mokhwasomssi
 * 
 */


#include "nrf24l01p.h"

#include <string.h>

#include "stdio.h"


static void cs_high()
{
    HAL_GPIO_WritePin(NRF24L01P_SPI_CS_PIN_PORT, NRF24L01P_SPI_CS_PIN_NUMBER, GPIO_PIN_SET);
}

static void cs_low()
{
    HAL_GPIO_WritePin(NRF24L01P_SPI_CS_PIN_PORT, NRF24L01P_SPI_CS_PIN_NUMBER, GPIO_PIN_RESET);
}

static void ce_high()
{
    HAL_GPIO_WritePin(NRF24L01P_CE_PIN_PORT, NRF24L01P_CE_PIN_NUMBER, GPIO_PIN_SET);
}

static void ce_low()
{
    HAL_GPIO_WritePin(NRF24L01P_CE_PIN_PORT, NRF24L01P_CE_PIN_NUMBER, GPIO_PIN_RESET);
}

// 替换掉原作者的 read_register
static uint8_t read_register(uint8_t reg)
{
    // 读指令是 NRF24L01P_CMD_R_REGISTER (0x00)
    uint8_t tx_buf[2] = { NRF24L01P_CMD_R_REGISTER | reg, 0xFF };
    uint8_t rx_buf[2] = { 0, 0 };
    cs_low();
    HAL_SPI_TransmitReceive(NRF24L01P_SPI, tx_buf, rx_buf, 2, 2000); // 连续边发边收
    cs_high();
    return rx_buf[1];
}

// 替换掉原作者的 write_register
static uint8_t write_register(uint8_t reg, uint8_t value)
{
    // 写指令是 NRF24L01P_CMD_W_REGISTER (0x20)
    uint8_t tx_buf[2] = { NRF24L01P_CMD_W_REGISTER | reg, value };
    cs_low();
    HAL_SPI_Transmit(NRF24L01P_SPI, tx_buf, 2, 2000); // 连续发
    cs_high();
    return value;
}

// static uint8_t read_register(uint8_t reg)
// {
//     uint8_t command = NRF24L01P_CMD_R_REGISTER | reg;
//     uint8_t status;
//     uint8_t read_val;
//     uint8_t dummy = 0xFF;
//
//     cs_low();
//     HAL_SPI_TransmitReceive(NRF24L01P_SPI, &command, &status, 1, 2000);
//     HAL_SPI_TransmitReceive(NRF24L01P_SPI, &dummy, &read_val, 1, 2000);
//     cs_high();
//
//     return read_val;
// }
//
// static uint8_t write_register(uint8_t reg, uint8_t value)
// {
//     uint8_t command = NRF24L01P_CMD_W_REGISTER | reg;
//     uint8_t status;
//     uint8_t write_val = value;
//
//     cs_low();
//     HAL_SPI_TransmitReceive(NRF24L01P_SPI, &command, &status, 1, 2000);
//     HAL_SPI_TransmitReceive(NRF24L01P_SPI, &write_val, &status, 1, 2000);
//     cs_high();
//
//     return write_val;
// }


/* nRF24L01+ Main Functions */
void nrf24l01p_rx_init(channel MHz, air_data_rate bps)
{
    nrf24l01p_reset();

    nrf24l01p_prx_mode();
    nrf24l01p_power_up();

    nrf24l01p_rx_set_payload_widths(NRF24L01P_PAYLOAD_LENGTH);

    nrf24l01p_set_rf_channel(MHz);
    nrf24l01p_set_rf_air_data_rate(bps);
    nrf24l01p_set_rf_tx_output_power(_0dBm);

    nrf24l01p_set_crc_length(1);
    nrf24l01p_set_address_widths(5);

    nrf24l01p_auto_retransmit_count(3);
    nrf24l01p_auto_retransmit_delay(250);
    
    ce_high();
}

void nrf24l01p_tx_init(channel MHz, air_data_rate bps)
{
    nrf24l01p_reset();

    nrf24l01p_ptx_mode();
    nrf24l01p_power_up();

    nrf24l01p_set_rf_channel(MHz);
    nrf24l01p_set_rf_air_data_rate(bps);
    nrf24l01p_set_rf_tx_output_power(_0dBm);

    nrf24l01p_set_crc_length(1);
    nrf24l01p_set_address_widths(5);

    nrf24l01p_auto_retransmit_count(3);
    nrf24l01p_auto_retransmit_delay(250);

    ce_high();
}

void nrf24l01p_rx_receive(uint8_t* rx_payload)
{
    nrf24l01p_read_rx_fifo(rx_payload);
    nrf24l01p_clear_rx_dr();

    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}

void nrf24l01p_tx_transmit(uint8_t* tx_payload)
{
    nrf24l01p_write_tx_fifo(tx_payload);
}

void nrf24l01p_tx_irq()
{
    uint8_t tx_ds = nrf24l01p_get_status();
    tx_ds &= 0x20;

    if(tx_ds)
    {   
        // TX_DS
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        nrf24l01p_clear_tx_ds();
    }

    else
    {
        // MAX_RT
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, SET);
        nrf24l01p_clear_max_rt();
        printf("TX: MAX_RT (Failed)!\n"); // 发送失败，没收到 ACK
    }
}

/* nRF24L01+ Sub Functions */
void nrf24l01p_reset()
{
    // Reset pins
    cs_high();
    ce_low();

    // Reset registers
    write_register(NRF24L01P_REG_CONFIG, 0x08);
    write_register(NRF24L01P_REG_EN_AA, 0x3F);
    write_register(NRF24L01P_REG_EN_RXADDR, 0x03);
    write_register(NRF24L01P_REG_SETUP_AW, 0x03);
    write_register(NRF24L01P_REG_SETUP_RETR, 0x03);
    write_register(NRF24L01P_REG_RF_CH, 0x02);
    write_register(NRF24L01P_REG_RF_SETUP, 0x07);
    write_register(NRF24L01P_REG_STATUS, 0x7E);
    write_register(NRF24L01P_REG_RX_PW_P0, 0x00);
    write_register(NRF24L01P_REG_RX_PW_P0, 0x00);
    write_register(NRF24L01P_REG_RX_PW_P1, 0x00);
    write_register(NRF24L01P_REG_RX_PW_P2, 0x00);
    write_register(NRF24L01P_REG_RX_PW_P3, 0x00);
    write_register(NRF24L01P_REG_RX_PW_P4, 0x00);
    write_register(NRF24L01P_REG_RX_PW_P5, 0x00);
    write_register(NRF24L01P_REG_FIFO_STATUS, 0x11);
    write_register(NRF24L01P_REG_DYNPD, 0x00);
    write_register(NRF24L01P_REG_FEATURE, 0x00);

    // Reset FIFO
    nrf24l01p_flush_rx_fifo();
    nrf24l01p_flush_tx_fifo();
}

void nrf24l01p_prx_mode()
{
    uint8_t new_config = read_register(NRF24L01P_REG_CONFIG);
    new_config |= 1 << 0;

    write_register(NRF24L01P_REG_CONFIG, new_config);
    // 【关键】开启 Pipe0 接收地址使能 (EN_RXADDR)
    uint8_t en_rxaddr = read_register(NRF24L01P_REG_EN_RXADDR);
    en_rxaddr |= 0x01; // 开启 Pipe0
    write_register(NRF24L01P_REG_EN_RXADDR, en_rxaddr);
}

void nrf24l01p_ptx_mode()
{
    uint8_t new_config = read_register(NRF24L01P_REG_CONFIG);
    new_config &= 0xFE;

    write_register(NRF24L01P_REG_CONFIG, new_config);
}

uint8_t nrf24l01p_read_rx_fifo(uint8_t* rx_payload)
{
    uint8_t command = NRF24L01P_CMD_R_RX_PAYLOAD;
    uint8_t status;
    // 准备一个空数组用于发时钟
    uint8_t dummy[NRF24L01P_PAYLOAD_LENGTH];
    memset(dummy, 0xFF, NRF24L01P_PAYLOAD_LENGTH);

    cs_low();
    // 发送读命令并获取 STATUS
    HAL_SPI_TransmitReceive(NRF24L01P_SPI, &command, &status, 1, 2000);
    // 发送空字节的同时读回 payload (非常关键！)
    HAL_SPI_TransmitReceive(NRF24L01P_SPI, dummy, rx_payload, NRF24L01P_PAYLOAD_LENGTH, 2000);
    cs_high();

    return status;
}

uint8_t nrf24l01p_write_tx_fifo(uint8_t* tx_payload)
{
    uint8_t command = NRF24L01P_CMD_W_TX_PAYLOAD;
    uint8_t status;

    cs_low();
    HAL_SPI_TransmitReceive(NRF24L01P_SPI, &command, &status, 1, 2000);
    HAL_SPI_Transmit(NRF24L01P_SPI, tx_payload, NRF24L01P_PAYLOAD_LENGTH, 2000);
    cs_high(); 

    return status;
}

void nrf24l01p_flush_rx_fifo()
{
    uint8_t command = NRF24L01P_CMD_FLUSH_RX;
    uint8_t status;

    cs_low();
    HAL_SPI_TransmitReceive(NRF24L01P_SPI, &command, &status, 1, 2000);
    cs_high();
}

void nrf24l01p_flush_tx_fifo()
{
    uint8_t command = NRF24L01P_CMD_FLUSH_TX;
    uint8_t status;

    cs_low();
    HAL_SPI_TransmitReceive(NRF24L01P_SPI, &command, &status, 1, 2000);
    cs_high();
}

uint8_t nrf24l01p_get_status()
{
    uint8_t command = NRF24L01P_CMD_NOP;
    uint8_t status;

    cs_low();
    HAL_SPI_TransmitReceive(NRF24L01P_SPI, &command, &status, 1, 2000);
    cs_high(); 

    return status;
}

uint8_t nrf24l01p_get_fifo_status()
{
    return read_register(NRF24L01P_REG_FIFO_STATUS);
}

void nrf24l01p_rx_set_payload_widths(widths bytes)
{
    write_register(NRF24L01P_REG_RX_PW_P0, bytes);
}

void nrf24l01p_clear_rx_dr()
{
    uint8_t new_status = nrf24l01p_get_status();
    new_status |= 0x40;

    write_register(NRF24L01P_REG_STATUS, new_status);
}

void nrf24l01p_clear_tx_ds()
{
    uint8_t new_status = nrf24l01p_get_status();
    new_status |= 0x20;

    write_register(NRF24L01P_REG_STATUS, new_status);     
}

void nrf24l01p_clear_max_rt()
{
    uint8_t new_status = nrf24l01p_get_status();
    new_status |= 0x10;

    write_register(NRF24L01P_REG_STATUS, new_status); 
}

void nrf24l01p_power_up()
{
    uint8_t new_config = read_register(NRF24L01P_REG_CONFIG);
    new_config |= 1 << 1;

    write_register(NRF24L01P_REG_CONFIG, new_config);
}

void nrf24l01p_power_down()
{
    uint8_t new_config = read_register(NRF24L01P_REG_CONFIG);
    new_config &= 0xFD;

    write_register(NRF24L01P_REG_CONFIG, new_config);
}

void nrf24l01p_set_crc_length(length bytes)
{
    uint8_t new_config = read_register(NRF24L01P_REG_CONFIG);
    
    switch(bytes)
    {
        // CRCO bit in CONFIG resiger set 0
        case 1:
            new_config &= 0xFB;
            break;
        // CRCO bit in CONFIG resiger set 1
        case 2:
            new_config |= 1 << 2;
            break;
    }

    write_register(NRF24L01P_REG_CONFIG, new_config);
}

void nrf24l01p_set_address_widths(widths bytes)
{
    write_register(NRF24L01P_REG_SETUP_AW, bytes - 2);
}

void nrf24l01p_auto_retransmit_count(count cnt)
{
    uint8_t new_setup_retr = read_register(NRF24L01P_REG_SETUP_RETR);
    
    // Reset ARC register 0
    new_setup_retr |= 0xF0;
    new_setup_retr |= cnt;
    write_register(NRF24L01P_REG_SETUP_RETR, new_setup_retr);
}

void nrf24l01p_auto_retransmit_delay(delay us)
{
    uint8_t new_setup_retr = read_register(NRF24L01P_REG_SETUP_RETR);

    // Reset ARD register 0
    new_setup_retr |= 0x0F;
    new_setup_retr |= ((us / 250) - 1) << 4;
    write_register(NRF24L01P_REG_SETUP_RETR, new_setup_retr);
}

void nrf24l01p_set_rf_channel(channel MHz)
{
	uint16_t new_rf_ch = MHz - 2400;
    write_register(NRF24L01P_REG_RF_CH, new_rf_ch);
}

void nrf24l01p_set_rf_tx_output_power(output_power dBm)
{
    uint8_t new_rf_setup = read_register(NRF24L01P_REG_RF_SETUP) & 0xF9;
    new_rf_setup |= (dBm << 1);

    write_register(NRF24L01P_REG_RF_SETUP, new_rf_setup);
}

void nrf24l01p_set_rf_air_data_rate(air_data_rate bps)
{
    // Set value to 0
    uint8_t new_rf_setup = read_register(NRF24L01P_REG_RF_SETUP) & 0xD7;
    
    switch(bps)
    {
        case _1Mbps: 
            break;
        case _2Mbps: 
            new_rf_setup |= 1 << 3;
            break;
        case _250kbps:
            new_rf_setup |= 1 << 5;
            break;
    }
    write_register(NRF24L01P_REG_RF_SETUP, new_rf_setup);
}

// 添加这个函数到 nrf24l01p.c
void nrf24l01p_set_addr(uint8_t reg, uint8_t* addr) {
    cs_low();
    uint8_t command = NRF24L01P_CMD_W_REGISTER | reg;
    HAL_SPI_Transmit(NRF24L01P_SPI, &command, 1, 2000);
    HAL_SPI_Transmit(NRF24L01P_SPI, addr, 5, 2000); // 写入 5 字节地址
    cs_high();
}

void nrf24l01p_dump_registers(void)
{
    uint8_t cfg = read_register(NRF24L01P_REG_CONFIG);
    uint8_t status = read_register(NRF24L01P_REG_STATUS);
    uint8_t rf_ch = read_register(NRF24L01P_REG_RF_CH);
    uint8_t rf_setup = read_register(NRF24L01P_REG_RF_SETUP);
    uint8_t setup_retr = read_register(NRF24L01P_REG_SETUP_RETR);
    uint8_t en_aa = read_register(NRF24L01P_REG_EN_AA);
    uint8_t en_rxaddr = read_register(NRF24L01P_REG_EN_RXADDR);

    printf("NRF: CONFIG=0x%02X STATUS=0x%02X RF_CH=%u RF_SETUP=0x%02X SETUP_RETR=0x%02X EN_AA=0x%02X EN_RXADDR=0x%02X\r\n",
           cfg, status, rf_ch, rf_setup, setup_retr, en_aa, en_rxaddr);
}