@echo off

REM 当前 bat 文件所在目录（自带结尾 \）
set SCRIPT_DIR=%~dp0

REM OpenOCD
set OPENOCD=openocd

REM 目标 ELF（相对于 bat 目录）
set ELF_PATH=%SCRIPT_DIR%build\Debug\blackbox_v2_bootloader.elf

REM 转成 Unix 风格路径（避免 OpenOCD 解析问题）
set ELF_PATH_UNIX=%ELF_PATH:\=/%

echo Programming file:
echo %ELF_PATH_UNIX%
echo.

REM 检查文件是否存在
if not exist "%ELF_PATH%" (
    echo ERROR: ELF file not found!
    echo Expected: %ELF_PATH%
    pause
    exit /b
)

REM 开始烧录
%OPENOCD% ^
 -f interface/cmsis-dap.cfg ^
 -f target/stm32f4x.cfg ^
 -c "adapter speed 20000" ^
 -c "program \"%ELF_PATH_UNIX%\" verify reset exit"

pause