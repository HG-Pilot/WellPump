@echo off
:: Welcome message
echo Welcome to ESP32-WellPump Flash Tool.
echo This Flash Tool is designed for ESP32-S3 16MB.
echo 2025 (Copyleft) Anton Polishchuk
echo.

:: Ask for COM port number
set /p COM="Enter your COM# port number (Use uppercase e.g., COM1): "
if "%COM%"=="" (
    echo Invalid COM port. Please restart the tool and provide a valid COM port.
    pause
    exit /b
)
echo.

:: Ask for board confirmation
echo Is your board ESP32 with 16MB Flash and 2 to 8MB of PSRAM?
set /p CONFIRM="Type Y for Yes or N for No: "

if /i "%CONFIRM%"=="Y" (
    echo Flashing the firmware. Please wait and do not close this window.
    esptool.exe --port %COM% --baud 115200 write_flash 0x0 WellPump.ino.merged.bin
    echo Flashing completed. You may close this window.
    pause
    exit /b
) else (
    echo You need to compile from source for your board or you can try flashing:
    echo WellPump.ino.bootloader.bin and WellPump.ino.bin with your custom partitions.bin.
    pause
    exit /b
)
