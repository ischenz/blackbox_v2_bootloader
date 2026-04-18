#ifndef __RING_BUFF_H
#define __RING_BUFF_H

#include "main.h"

#define  RINGBUFF_LEN          (256)     //
#define  RINGBUFF_OK           1     
#define  RINGBUFF_ERR          0   

typedef struct
{
    uint16_t Head;           
    uint16_t Tail;
    uint16_t Lenght;
    uint8_t  Ring_data[RINGBUFF_LEN];
}RingBuff_t;


void RingBuff_Init(RingBuff_t *ringbuff);
uint8_t Write_RingBuff(RingBuff_t *ringbuff, uint8_t data);
uint8_t Read_RingBuff(RingBuff_t *ringbuff, uint8_t *rData);

#endif /*__RING_BUFF_H*/
