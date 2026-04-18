@echo off

REM 设置 OpenOCD 路径（如果已经加入 PATH 可以删掉这行）
set OPENOCD=openocd

REM 启动 OpenOCD 并烧录程序
%OPENOCD% ^
 -f interface/cmsis-dap.cfg ^
 -f target/stm32f4x.cfg ^
 -c "adapter speed 20000" ^
 -c "program C:\\Users\\ische\\Desktop\\blackbox_v2_bootloader\\build\\Debug\\blackbox_v2_bootloader.elf verify reset exit"

pause   