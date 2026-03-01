@echo off
REM Windows 10 Optimizer - Build script
REM Requires MinGW (gcc) or MSVC in PATH

echo Building Windows 10 Optimizer...

where gcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    gcc -Wall -Wextra -O2 -o win_optimizer.exe win_optimizer.c -lshell32 -lgdi32
    if %ERRORLEVEL% EQU 0 (
        echo Build successful: win_optimizer.exe
        echo Run as Administrator for full functionality.
    ) else (
        echo Build failed.
        exit /b 1
    )
) else (
    where cl >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        cl /nologo /W3 /O2 /DUNICODE /D_UNICODE win_optimizer.c shell32.lib gdi32.lib /Fe:win_optimizer.exe
        if %ERRORLEVEL% EQU 0 (
            echo Build successful: win_optimizer.exe
            del win_optimizer.obj 2>nul
        ) else (
            echo Build failed.
            exit /b 1
        )
    ) else (
        echo Error: Neither gcc nor cl found. Install MinGW or Visual Studio.
        exit /b 1
    )
)
