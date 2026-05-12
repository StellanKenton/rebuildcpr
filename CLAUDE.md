# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## First Rule for Every Session

Always read `rep/rule/rule.md` first, then follow its prescribed reading order before touching any code.

## Project Overview

STM32F103RCT6 (Cortex-M3) CPR sensor firmware. Built with CMake + arm-none-eabi-gcc, also has Keil MDK-ARM project files. Uses FreeRTOS (CMSIS-RTOS V2) and STM32 HAL.

## Repository Layout

- `rep/` — reusable architecture submodule (drivers, modules, services, tools, libs)
- `User/` — project-specific code (manager, system, bsp, port)
- `Core/`, `Drivers/`, `Middlewares/`, `USB_DEVICE/` — STM32CubeMX generated files
- `MDK-ARM/` — Keil project files
- `SEGGER/` — SEGGER RTT sources
- `cmake/` — CMake toolchain + linker script
- `scripts/` — build/flash helper scripts (VS Code portable scripts + port_pack.py)

## Build

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
  -DARM_GNU_TOOLCHAIN_BIN_DIR=/path/to/arm-gnu-toolchain/bin
cmake --build build
```

Output: `build/CprSensor.elf`, `.hex`, `.bin`, `.map`. MDK-ARM project also available at `MDK-ARM/CprSensor.uvprojx`.

## Boundary Rules (where to put code)

- **`rep/`**: stable core, drivers, tools, reusable adapters. Do NOT put project binding (manager/system orchestration, board wiring, task wiring) here. Keep lightweight for MCU resource constraints.
- **`User/`**: board wiring, device instances, addresses, task periods, startup order, feature switches.
- **`User/port/`**: project-side port/assembly binding (the ONLY place for project-side port code).
- **`User/manager/`**: product services and orchestration.
- **`User/system/`**: task creation, startup wiring, mode-level orchestration.
- **`User/bsp/`**: board support package implementations.

## Key Coding Rules

1. **Typedefs and macros MUST go in `.h` files, NEVER in `.c` files.** No exceptions — even if only used internally.
2. **No `printf`/`puts`/`putchar`.** Use `LOG_I`, `LOG_W`, `LOG_E`, `LOG_T` macros from `rep/sys/log/log.h`.
3. **No direct RTOS API calls.** Except in `User/port/rtos_port.*`, always use `rep/sys/rtos/rtos.h` `repRtos*` functions.
4. **New directories MUST include a main `.md` document** (with design overview, interface contracts, usage examples) AND update the parent directory's main `.md`.
5. **Naming**: camelCase. Prefixes — `g` global, `l` local, `st` struct, `e` enum, `pf` function pointer. Function names prefix with module name (e.g., `drvSpiInit`).
6. **Brace style**: same line (`void func() {`).
7. **New files** must follow templates at `rep/newfile/example.h` and `rep/newfile/example.c` (unified header/footer comments).
8. **Encoding**: UTF-8, 4-space indentation, LF line endings, files end with exactly one newline.
9. **When adding features**: do NOT modify `rep/` unless explicitly required; prefer adapting other files to match `rep/` contracts.
10. **Code style**: keep it simple — minimal complexity, minimal branches. Reusability > efficiency > readability > resource usage.

## Documentation Reading Order (from rule.md)

For any new task:
1. `rep/rule/rule.md` → `map.md` → `coderule.md`
2. Parent directory summary doc (e.g., `rep/driver/drvrule.md`)
3. Target leaf directory main doc
4. Then and only then, read `.h/.c` files

## RTOS Abstraction

All code outside `User/port/rtos_port.*` must use `rep/sys/rtos/rtos.h`:
- `repRtosDelayMs()`, `repRtosGetTickMs()`, `repRtosTaskCreate()`, `repRtosTaskDelayUntilMs()`
- `repRtosStatsInit()`, `repRtosIsSchedulerRunning()`

## RTT Debug Output

When reading RTT output: first check if another process is occupying RTT, kill it if so. Use a one-shot TCP read to pull the RTT buffer rather than the interactive RTT terminal. If no logs appear after starting RTT service, the firmware may have stopped sending — try rebooting the device.

## Port Pack Tool

Switch project port configurations:
```bash
python scripts/port_pack.py
```

## Pre-Coding Checklist (from rule.md §10)

Before writing any `.c/.h`:
1. Are there new macros/typedefs/enums/function pointer types? → place them in `.h`
2. New directories/files? → create main `.md` doc and update parent `.md`
3. Changing compiled files? → modify the real build entry (CMakeLists.txt), not a compatibility layer
4. Project-binding logic going into `rep/`? → put it in `User/` or `User/port/` instead
