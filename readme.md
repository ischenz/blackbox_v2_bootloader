# BlackBox V2 Bootloader

基于fh_stream传输协议的STM32 Bootloader项目。
BlackBox是一个航模黑盒子项目，为了使方便升级，便有了本项目。

## 项目描述

这是一个专为STM32微控制器设计的bootloader，支持通过串口或其他接口进行固件更新。项目使用STM32CubeMX生成的基础代码，并集成了自定义的fh_stream传输协议、环形缓冲区和bootloader逻辑。

## 特性

- 支持STM32微控制器
- 基于fh_stream协议的可靠数据传输

## 硬件要求

- STM32微控制器
- 串口通信接口（UART）

## 软件要求

- GCC ARM None EABI工具链
- CMake 3.16+
- OpenOCD（用于下载和调试）
- STM32CubeMX（用于配置修改）

## 使用方法


## fh_stream传输协议

协议格式：

| head | tag | length | value | crc |
|------|-----|--------|-------|-----|
| 0x55 | 命令标识 | 数据长度 | 数据内容 | CRC校验 |

### 示例

| head | tag | length | value | crc |
|------|-----|--------|-------|-----|
| 0x55 | 0x00 | 0x01   | 0x12  | 0xXX |

### 固件传输流程

1. **数据包传输**：
   - 上位机分批发送数据包，前4字节为包ID（递增）
   - Bootloader接收数据包，判断包ID是否正常递增
   - Bootloader校验数据包CRC，若成功则返回ACK响应

2. **ACK响应**：
   - TAG: 0x02 (ack)
   - Value: 4字节包ID
   - 上位机收到ACK后发送下一帧数据包，包ID加一

3. **传输完成**：
   - 上位机发送全部数据包后，发送完成命令
   - TAG: 0x01 (cmd)
   - Value: 4字节APP固件CRC校验值

4. **返回crc校验结果**
   - TAG: 0x02 (ack)
   - Value: 4字节(uint32_t 0(成功)/1(失败))


5. **校验与跳转**：
   - Bootloader接收完成命令，校验APP固件的CRC
   - 校验成功则跳转到APP程序执行

## 项目结构

- `Core/` - STM32 HAL库和用户代码
- `Drivers/` - CMSIS和HAL驱动
- `fh_bootloader/` - Bootloader核心逻辑
- `fh_ringbuff/` - 环形缓冲区实现
- `fh_stream/` - 流传输协议
- `cmake/` - CMake配置文件
- `build/` - 构建输出目录

## 许可证

本项目采用MIT许可证。详见LICENSE文件。

## 贡献

欢迎提交Issue和Pull Request来改进项目。

上位机链接https://github.com/LiHuashii/firmware_updater


