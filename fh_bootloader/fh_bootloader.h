#ifndef __FH_BOOTLOADER_H__
#define __FH_BOOTLOADER_H__

#include <stdint.h>

#define FH_BL_VERSION 0x00010000 // 版本号1.0.0，格式为0x00MMmmpp，MM主版本号，mm次版本号，pp补丁版本号
#define FH_BL_INFO_MAGIC (0x5A5A5A5A)
#define FH_BL_APP_ADDR  (0x08020000) // app存放地址，Sector 5,0x0802 0000 - 0x0803 FFFF 128 Kbytes
#define FH_BL_INFO_ADDR (0x08008000) // bootloader信息存放地址，Sector 16KByte,0x0800 8000 - 0x0800 BFFF 16 Kbytes
#define FH_BL_INFO_SECTOR FLASH_SECTOR_2 // bootloader信息存放扇区，Sector 16KByte,0x0800 8000 - 0x0800 BFFF 16 Kbytes
#define FH_BL_APP_SECTOR  FLASH_SECTOR_5 // app存放扇区，Sector 5,0x0802 0000 - 0x0803 FFFF 128 Kbytes

typedef enum { // 开机 或者升级
    FH_BL_UPGRADE_TYPE_NONE = 0, // 正常启动
    FH_BL_UPGRADE_TYPE_KEY,      // 按键触发升级
    FH_BL_UPGRADE_TYPE_FLAG,     // 升级标志触发升级
} fh_bl_upgrade_type_e;

typedef struct
{
    uint32_t magic;              // 标识 (固定值，如 0x5A5A5A5A)

    /* 基础信息 */
    uint32_t boot_version;       // Bootloader版本
    uint32_t app_version;        // 应用版本
    uint32_t app_build_time;      // 应用编译时间戳
    uint32_t app_size;           // 应用大小
    uint32_t app_addr;           // 应用地址

    /* 升级控制 */
    uint32_t upgrade_flag;       // 升级标志
    uint32_t upgrade_type;       // 升级方式
    uint32_t target_partition;   // 目标分区

    /* 运行状态 */
    uint32_t current_partition;  // 当前运行分区
    uint32_t upgrade_status;     // 升级状态
    uint32_t boot_count;         // 启动计数
    uint32_t last_boot_status;   // 上次启动状态

    /* 校验 */
    uint32_t app_crc;            // 固件CRC
    uint32_t info_crc;           // 本结构CRC

    /* 预留扩展 */
    uint32_t reserved[8];

} fh_bl_info_t;

void fh_bl_boot(void);

#endif /* __FH_BOOTLOADER_H__ */