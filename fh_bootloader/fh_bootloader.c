/*
 * @Author: ischen.x ischen.x@foxmail.com
 * @Date: 2026-04-17 10:21:07
 * @LastEditors: ischen.x ischen.x@foxmail.com
 * @LastEditTime: 2026-04-17 18:18:52
 * 
 * Copyright (c) 2026 by fhchengz, All Rights Reserved. 
 */
#include "fh_bootloader.h"
#include "fh_stream.h"
#include "ringbuff.h"
#include "string.h"
#include "usart.h"
#include "main.h"

#define FH_BL_APP_ADDR (0x08020000)

RingBuff_t Uart1_RingBuff;
uint8_t rxbuf_u1;
uint32_t pack_id = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        if(Write_RingBuff(&Uart1_RingBuff, rxbuf_u1) == RINGBUFF_ERR){
        }
        HAL_UART_Receive_IT(&huart1, &rxbuf_u1, 1);
    }
}

int fh_bl_update_check()
{
    return 1;
}

static int fh_bl_ack(uint32_t pack_id)
{
    int ret = 0;
    uint8_t ack_buf[sizeof(fh_stream_frame_t) + 256];
    ret = fh_stream_pack(ack_buf, FH_STREAM_TAG_ACK, &pack_id, 4);
    HAL_UART_Transmit(&huart1, ack_buf, ret, 100);
    return ret;
}

int stm32_flash_write(uint32_t addr, uint8_t *data, uint16_t len)
{
    HAL_FLASH_Unlock(); // 解锁FLASH
    for (uint16_t i = 0; i < len; i += 4) {
        uint32_t word = *(uint32_t *)(data + i); // 取4字节数据
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
            HAL_FLASH_Lock(); // 上锁FLASH
            return -1; // 写入失败
        }
    }
    HAL_FLASH_Lock(); // 上锁FLASH
    return 0; // 写入成功
}


static uint8_t write_buff[1024] = {0}; // 写入缓冲区
static uint16_t write_ptr = 0; // 写入缓冲区指针
static uint32_t flash_write_ptr = FH_BL_APP_ADDR; // FLASH写入指针，初始为APP地址
int fh_bl_flash_write(uint8_t *data, uint16_t len)
{
    uint32_t id = *(uint32_t *)data;
    if (len < 4) {
        return -1; // 数据长度不足4字节，无法解析id
    }
    if (id == pack_id) {//接收到正确的帧，写入缓冲区,缓冲区满写入flash
        memcpy(write_buff + write_ptr, data + 4, len - 4); // 减去4字节的id
        write_ptr += len - 4;
        if (write_ptr >= 512) { // 接收到大于512字节的数据，写入flash
            // 取4字节的整数倍长度写入flash，并更新写入指针和缓冲区指针
            uint16_t write_len = (write_ptr / 4) * 4; // 取4字节的整数倍长度
            if (stm32_flash_write(flash_write_ptr, write_buff, write_len) != 0) {
                return -1; // 写入flash失败
            }
            // 把剩下没写入的数据移到缓冲区开头
            flash_write_ptr += write_len; // 更新flash写入指针
            memmove(write_buff, write_buff + write_len, write_ptr - write_len); // 把剩下的数据移到缓冲区开头
            write_ptr -= write_len; // 更新缓冲区指针
        }
        fh_bl_ack(pack_id);
        pack_id++;
    }
    return 0;
}

// 传输完成后写入最后剩下的数据到flash
int fh_bl_flash_write_final(void)
{
    if (write_ptr > 0) { // 如果缓冲区还有剩余数据，写入flash
        uint16_t write_len = (write_ptr / 4) * 4 + 4; // 取4字节的整数倍长度
        if (stm32_flash_write(flash_write_ptr, write_buff, write_len) != 0) {
            return -1; // 写入flash失败
        }
        flash_write_ptr += write_len; // 更新flash写入指针
    }
    return 0;
}

// Sector 5,0x0802 0000 - 0x0803 FFFF 128 Kbytes
int fh_bl_clear_app(void)
{
    FLASH_EraseInitTypeDef FlashEraseInit;
    HAL_StatusTypeDef FlashStatus = HAL_OK;
    uint32_t SectorError = 0;
    HAL_FLASH_Unlock();            //解锁
    FlashEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;     //擦除类型，扇区擦除
    FlashEraseInit.Sector = FLASH_SECTOR_5; //要擦除的扇区
    FlashEraseInit.NbSectors = 1;                           //一次只擦除一个扇区
    FlashEraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;    //电压范围，VCC=2.7~3.6V之间!!
    if (HAL_FLASHEx_Erase(&FlashEraseInit, &SectorError) != HAL_OK)
    {
        return -1; //发生错误了
    }

    FlashStatus = FLASH_WaitForLastOperation(20000); //等待上次操作完成
    if (FlashStatus != HAL_OK)
    {
        return -1; //发生错误了
    }
    HAL_FLASH_Lock();              //上锁
    return 0;
}

int fh_bl_update()
{
    uint8_t mem[sizeof(fh_stream_frame_t) + 256];
    fh_stream_frame_t *freame = (fh_stream_frame_t *)mem; 
    fh_bl_clear_app(); //清除APP区域FLASH数据
    for (;;) {
        uint8_t buf;
        if (Read_RingBuff(&Uart1_RingBuff, &buf) == RINGBUFF_OK) {
            if (fh_stream_unpack(buf, freame) == FH_STREAM_EVENT_FRAME_RECEIVED) {
                if (freame->tag == FH_STREAM_TAG_DATA) {
                    fh_bl_flash_write(freame->value, freame->length);
                } else if (freame->tag == FH_STREAM_TAG_CMD) {
                    // 接收结束，跳转到APP
                    fh_bl_flash_write_final(); // 写入最后剩下的数据到flash
                    fh_bl_ack(pack_id);
                    break;
                }
            }
        }
    }
    
    return 0;
}

void fh_bl_jmp_to_app(int app_addr)
{
    typedef void (*app_fun_t)(void);
    app_fun_t app_fun = (app_fun_t)*(uint32_t *)(app_addr + 4);
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    __disable_irq();
    //停用 HAL 和系统节拍
    HAL_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    //清除 NVIC 状态
    for (uint32_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    //重定位中断向量
    SCB->VTOR = app_addr;
    // Set the MSP to the value at the start of the application
    __set_MSP(*(uint32_t*)app_addr);
    //再打开中断
    __enable_irq();
    // Jump to the application entry point
    app_fun();
    for (;;){

    }
}

void fh_bl_boot(void)
{
    int ret = 0;
    ret = fh_bl_update_check();
    if (ret != 0) // no update, boot to app
    {
        fh_bl_update();
    }
    fh_bl_jmp_to_app(FH_BL_APP_ADDR);
}