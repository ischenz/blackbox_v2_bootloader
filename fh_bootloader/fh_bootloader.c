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
#include <string.h>
#include "fh_sw_crc.h"
#include "ringbuff.h"
#include "usart.h"
#include "main.h"

#define  FH_BL_KEY_PRESSED  1  // 按键按下状态，GPIO读取到0
#define  FH_BL_KEY_RELEASED 0

#define FH_BL_PRINT printf

int fh_bl_packet_send(uint8_t *data, uint16_t len)
{
    return HAL_UART_Transmit(&huart1, data, len, 100);
}

int fh_key_get_state(void)
{
    return HAL_GPIO_ReadPin(key_GPIO_Port, key_Pin); // TODO: 实现按键状态读取函数，返回1表示按键按下，0表示按键未按下
}

int fh_bl_info_write(fh_bl_info_t *info)
{
    info->magic = 0x5A5A5A5A; // 设置magic
    HAL_FLASH_Unlock(); // 解锁FLASH
    // 先擦除info区所在的扇区，再写入数据
    FLASH_EraseInitTypeDef FlashEraseInit;
    HAL_StatusTypeDef FlashStatus = HAL_OK;
    uint32_t SectorError = 0;
    FlashEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;     //擦除类型，扇区擦除
    FlashEraseInit.Sector = FH_BL_INFO_SECTOR; //要擦除的扇区
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
    for (uint16_t i = 0; i < sizeof(fh_bl_info_t); i += 4) {
        uint32_t word = *(uint32_t *)((uint8_t *)info + i); // 取4字节数据
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FH_BL_INFO_ADDR + i, word) != HAL_OK) {
            HAL_FLASH_Lock(); // 上锁FLASH
            return -1; // 写入失败
        }
    }
    HAL_FLASH_Lock(); // 上锁FLASH
    return 0; // 写入成功
}

int fh_bl_clear_app(void)
{
    FLASH_EraseInitTypeDef FlashEraseInit;
    HAL_StatusTypeDef FlashStatus = HAL_OK;
    uint32_t SectorError = 0;
    HAL_FLASH_Unlock();            //解锁
    FlashEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;     //擦除类型，扇区擦除
    FlashEraseInit.Sector = FH_BL_APP_SECTOR; //要擦除的扇区
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

//需要4字节对齐写入flash
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
static uint16_t write_ptr = 0;          // 写入缓冲区指针 写入缓冲区已有的数据长度
static uint32_t flash_write_ptr = FH_BL_APP_ADDR; // FLASH写入指针，初始为APP地址
static int fh_bl_flash_write(uint8_t *data, uint16_t len)
{
    memcpy(write_buff + write_ptr, data, len); // 
    write_ptr += len;
    if (write_ptr >= 512) { // 接收到大于512字节的数据，写入flash
        // 取4字节的整数倍长度写入flash，并更新写入指针和缓冲区指针
        uint16_t write_len = (write_ptr / 4) * 4; // 取4字节的整数倍长度，舍去不足4字节的部分，留在缓冲区里等待下次写入时补齐到4字节再写入flash
        if (stm32_flash_write(flash_write_ptr, write_buff, write_len) != 0) {
            return -1; // 写入flash失败
        }
        // 把剩下没写入的数据移到缓冲区开头
        flash_write_ptr += write_len; // 更新flash写入指针
        memmove(write_buff, write_buff + write_len, write_ptr - write_len); // 把剩下的数据移到缓冲区开头
        write_ptr -= write_len; // 更新缓冲区指针
    }
    return 0;
}

// 传输完成后写入最后剩下的数据到flash
static int fh_bl_flash_write_final(void)
{
    if (write_ptr > 0) { // 如果缓冲区还有剩余数据，写入flash
        uint16_t write_len = (write_ptr / 4) * 4 + 4; // 取4字节的整数倍长度，如果不足4字节，补齐到4字节
        if (stm32_flash_write(flash_write_ptr, write_buff, write_len) != 0) {
            return -1; // 写入flash失败
        }
        flash_write_ptr += write_len; // 更新flash写入指针
    }
    return 0;
}

/*****************************************fh_bootloader 内部函数*********************************************** */

static int fh_bl_crc_check(uint32_t app_addr, uint32_t app_size, uint32_t app_crc)
{
    uint32_t crc = fh_sw_crc32((uint8_t *)app_addr, app_size);
    return crc == app_crc ? 0 : -1; // 返回0表示校验通过，-1表示校验失败
}

static void fh_bl_info_read(fh_bl_info_t *info)
{
    memcpy(info, (void *)FH_BL_INFO_ADDR, sizeof(fh_bl_info_t));
    if (info->magic != FH_BL_INFO_MAGIC) { // 烧录bootloader后第一次上电没有app，info区数据无效，magic不正确，清零info区
        FH_BL_PRINT("bootloader info invalid, initialize it\r\n");
        memset(info, 0x00, sizeof(fh_bl_info_t)); // 无效信息，全0xFF表示
        info->magic = FH_BL_INFO_MAGIC; // 设置magic，表示info区已初始化
        info->upgrade_flag = 1; // 需要升级
    }
}

static fh_bl_upgrade_type_e fh_bl_update_check(fh_bl_info_t *info)
{

    if (fh_key_get_state() == FH_BL_KEY_PRESSED) { // 按住按键开机
        FH_BL_PRINT("key pressed, enter upgrade mode\r\n");
        return FH_BL_UPGRADE_TYPE_KEY; // 进入升级模式
    } else if (info->upgrade_flag == 1) { 
        FH_BL_PRINT("upgrade flag set, enter upgrade mode\r\n");
        return FH_BL_UPGRADE_TYPE_FLAG; // 进入升级模式
    } else {
        return FH_BL_UPGRADE_TYPE_NONE; // 正常启动
    }
}

static int fh_bl_ack(uint32_t pack_id)
{
    int ret = 0;
    uint8_t ack_buf[sizeof(fh_stream_frame_t) + sizeof(uint32_t) + sizeof(crc_type)]; // ACK帧缓冲区，包含帧头、tag、length、value和crc
    ret = fh_stream_pack(ack_buf, FH_STREAM_TAG_ACK, (value_type *)&pack_id, sizeof(uint32_t)); // 打包ACK帧，tag为ACK，value为pack_id
    fh_bl_packet_send(ack_buf, ret);
    return ret;
}

static int fh_bl_update(size_t *app_size)
{
    uint8_t mem[sizeof(fh_stream_frame_t) + (1 << sizeof(length_type) * 8)];
    fh_stream_frame_t *freame = (fh_stream_frame_t *)mem; 
    fh_bl_clear_app();                  //清除APP区域FLASH数据
    static uint32_t pack_id = 0;
    flash_write_ptr = FH_BL_APP_ADDR;   //重置FLASH写入指针
    write_ptr = 0;                      //重置写入缓冲区指针
    pack_id = 0;                        //重置数据包ID

    for (;;) {
        uint8_t buf;
        if (Read_RingBuff(&Uart1_RingBuff, &buf) == RINGBUFF_OK) {
            if (fh_stream_unpack(buf, freame) == FH_STREAM_EVENT_FRAME_RECEIVED) {
                uint32_t id = 0;
                switch (freame->tag) {
                    case FH_STREAM_TAG_DATA: // id在这里判断
                        id = *(uint32_t *)freame->value;//前4字节是id
                        if (id != pack_id || freame->length < 4) { // id不对，丢弃数据，
                            FH_BL_PRINT("packet id mismatch, expected %lu but got %lu\r\n", pack_id, id);
                            continue;
                        }
                        fh_bl_flash_write(freame->value + 4, freame->length - 4);
                        fh_bl_ack(pack_id); // 发送ACK
                        pack_id++; // 数据包ID加1
                        break;
                    case FH_STREAM_TAG_CMD: // 接收结束，校验crc跳转到APP
                        fh_bl_flash_write_final(); // 写入最后剩下的数据到flash
                        if (fh_bl_crc_check(FH_BL_APP_ADDR, flash_write_ptr - FH_BL_APP_ADDR, *(uint32_t *)freame->value) == 0) { // 校验APP的CRC
                            fh_bl_ack(0); // 发送升级成功ACK
                            *app_size = flash_write_ptr - FH_BL_APP_ADDR; // 计算APP大小
                            return 0; // 升级成功
                        } else { // 校验失败，清除APP区域FLASH数据，重置写入指针，继续等待数据
                            FH_BL_PRINT("app crc check failed\r\n"); 
                            fh_bl_clear_app();
                            fh_bl_ack(1); // 发送升级成功ACK
                            flash_write_ptr = FH_BL_APP_ADDR; //重置FLASH写入指针
                            write_ptr = 0;           //重置写入缓冲区指针
                            pack_id = 0;             //重置数据包ID
                            return -1; 
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
    return 0;
}

static void fh_bl_hello(fh_bl_info_t *info)
{
    FH_BL_PRINT("=================fh bootloader v1.0=====================\r\n");
    FH_BL_PRINT("bootloader version: %lx\r\n", info->boot_version);
    FH_BL_PRINT("upgrade flag: %lu\r\n", info->upgrade_flag);
    FH_BL_PRINT("app version: %lx\r\n", info->app_version);
    FH_BL_PRINT("app build time: %lu\r\n", info->app_build_time);
    FH_BL_PRINT("app size: %lu B\r\n", info->app_size);
    FH_BL_PRINT("boot count: %lu\r\n", info->boot_count);
    FH_BL_PRINT("=========================================================\r\n");
}

static void fh_bl_jmp_to_app(int app_addr)
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
    size_t app_size = 0;
    fh_bl_info_t bl_info;
    fh_bl_info_read(&bl_info);
    bl_info.boot_count++; // 启动计数加1
    bl_info.boot_version = FH_BL_VERSION; // 设置bootloader版本
    fh_bl_hello(&bl_info);
    ret = fh_bl_update_check(&bl_info);
    fh_bl_info_write(&bl_info);
    if (ret != FH_BL_UPGRADE_TYPE_NONE) // 需要升级
    {
        if (fh_bl_update(&app_size) != 0) {
            // 处理升级失败的情况 软重启
            NVIC_SystemReset();
        }
        bl_info.app_size = app_size; // 设置app大小
        bl_info.upgrade_flag = 0; // 下载固件后清除升级标志
        fh_bl_info_write(&bl_info);
    }
    fh_bl_jmp_to_app(FH_BL_APP_ADDR);
}