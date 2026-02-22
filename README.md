# Windows 10 Optimizer

A C program that restores Windows 10 responsiveness when the system becomes slow after extended use. Run it whenever your PC feels sluggish to quickly free resources and speed things up again.

## What It Does

| Optimization | Effect | Admin Required |
|-------------|--------|----------------|
| **Empty Standby Memory** | Frees cached RAM when memory pressure exists | Yes |
| **Clear Temp Files** | Deletes files in TEMP, TMP, Windows\Temp | No |
| **Empty Recycle Bin** | Frees disk space | No |
| **Flush DNS Cache** | Clears stale DNS entries | No |
| **Restart Explorer** | Fixes sluggish taskbar/desktop | Yes |
| **Optimize Screen** | Flush graphics, redraw desktop, restart DWM (fixes display lag) | DWM restart: Yes |
| **Low-level memory** | Purge low-priority standby, flush modified list to disk | Yes |
| **Filesystem flush** | Flush volume C: buffers to disk | Yes |
| **Clipboard** | Clear clipboard | No |
| **Trim background** | Empty working sets of non-critical processes | Yes |
| **Process priority** | Boost optimizer process priority | No |
| **Trim Working Set** | Reduces process memory footprint | No |

## Building

### Option 1: MinGW (gcc)
```bash
gcc -Wall -O2 -o win_optimizer.exe win_optimizer.c -lshell32 -ladvapi32 -lgdi32 -lpsapi
```

### Option 2: Visual Studio (cl)
```cmd
cl /O2 win_optimizer.c shell32.lib advapi32.lib /Fe:win_optimizer.exe
```

### Option 3: Build script
```cmd
build.bat
```

## Usage

```cmd
win_optimizer              # Run all optimizations
win_optimizer /quick       # Skip Explorer restart (less disruptive)
win_optimizer /memory      # Empty standby list only
win_optimizer /temp        # Clear temp files only
win_optimizer /recycle     # Empty Recycle Bin only
win_optimizer /dns         # Flush DNS cache only
win_optimizer /explorer    # Restart Explorer only
win_optimizer /screen      # Optimize display/screen only (fixes lag)
win_optimizer /help        # Show help
```

**For maximum effect, run as Administrator** (right-click → Run as administrator). Without admin rights, memory optimization and Explorer restart will fail, but temp cleanup, Recycle Bin, and DNS flush will still work.

## When to Use

- Windows feels slow after hours of use
- Taskbar or desktop is unresponsive
- RAM usage is high and programs are sluggish
- Before starting a demanding task (gaming, video editing)

## Notes

- **Standby List**: Windows caches recently used data in RAM. When RAM is tight, emptying this cache frees memory for active programs. On systems with 16GB+ RAM, this may have less impact.
- **Explorer Restart**: Briefly closes the taskbar and desktop. Windows restarts Explorer automatically—your open windows and files are not affected.
- **Screen Optimization**: Flushes GDI/DWM buffers, forces desktop and taskbar redraw, and can restart DWM (Desktop Window Manager) to fix display lag and stuttering. DWM restart causes a brief black screen; use `/quick` to skip it.
- **Temp Files**: Only clears files; does not delete subdirectories. Safe for normal use.

## License

Public domain. Use freely.
