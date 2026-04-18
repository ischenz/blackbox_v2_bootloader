/*
 * @Author: ischen.x ischen.x@foxmail.com
 * @Date: 2024-08-21 15:59:23
 * @LastEditors: ischen.x ischen.x@foxmail.com
 * @LastEditTime: 2024-08-22 09:06:00
 * 
 * Copyright (c) 2024 by fhchengz, All Rights Reserved. 
 */
#include "ringbuff.h"

/**
* @brief  RingBuff_Init
* @param  RingBuff_t *ringbuff
* @return void
* @note   初始化环形缓冲区
*/
void RingBuff_Init(RingBuff_t *ringbuff)
{
  //初始化相关信息
  ringbuff->Head = 0;
  ringbuff->Tail = 0;
  ringbuff->Lenght = 0;
}

/**
* @brief  Write_RingBuff
* @param  uint8_t data
* @return FLASE:环形缓冲区已满，写入失败;TRUE:写入成功
* @note   往环形缓冲区写入uint8_t类型的数据
*/
uint8_t Write_RingBuff(RingBuff_t *ringbuff, uint8_t data)
{
  if(ringbuff->Lenght >= RINGBUFF_LEN) //判断缓冲区是否已满
  {
    return RINGBUFF_ERR;
  }
  ringbuff->Ring_data[ringbuff->Tail]=data;
  ringbuff->Tail = (ringbuff->Tail+1)%RINGBUFF_LEN;//防止越界非法访问
  ringbuff->Lenght++;
  return RINGBUFF_OK;
}

/**
* @brief  Read_RingBuff
* @param  uint8_t *rData，用于保存读取的数据
* @return FLASE:环形缓冲区没有数据，读取失败;TRUE:读取成功
* @note   从环形缓冲区读取一个u8类型的数据
*/
uint8_t Read_RingBuff(RingBuff_t *ringbuff, uint8_t *rData)
{
  if(ringbuff->Lenght == 0)//判断非空
  {
    return RINGBUFF_ERR;
  }
  *rData = ringbuff->Ring_data[ringbuff->Head];//先进先出FIFO，从缓冲区头出
  ringbuff->Head = (ringbuff->Head+1)%RINGBUFF_LEN;//防止越界非法访问
  ringbuff->Lenght--;
  return RINGBUFF_OK;
}
