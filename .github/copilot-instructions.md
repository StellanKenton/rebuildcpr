# Project Guidelines

## Session Start

- At the start of every new session, read `rep/rule/rule.md` first.
- Then read `rep/rule/map.md` and `rep/rule/coderule.md` before exploring code.
- Narrow scope through the directory authority docs before reading source files: parent directory doc first, leaf directory doc next, `.h/.c` last.
- Do not sweep the whole `rep/` tree without using the rule documents to reduce scope.

## Architecture

- This workspace is split by reuse boundary.
- `rep/` contains reusable drivers, modules, services, tools, and the authority documentation for repo structure and coding rules.
- `User/` contains project-bound code for this product, including `port/`, `manager/`, `system/`, and `rep_config.h`.
- `Core/`, `Drivers/`, `Middlewares/`, `USB_DEVICE/`, and most of `MDK-ARM/` are STM32CubeMX or toolchain-generated content; preserve generated structure unless the task explicitly requires changes there.
- Respect the core-port boundary from `rep/rule/projectrule.md`: reusable core code must not include `_port.h` or call project binding functions directly.
- RTOS coupling is project-bound in `User/port/rtos_port.*`; reusable code should go through the `repRtos*` abstraction.

## Build And Debug

- Prefer the existing VS Code tasks over ad hoc shell commands.
- Build with `Keil: Build` or `Keil: Rebuild`.
- Flash with `J-Link: Flash`.
- Use `J-Link: RTT Open` or `Serial: Monitor` for runtime output when needed.
- The Keil project file is `MDK-ARM/CprSensor.uvprojx` and the active target is `CprSensor`.
- There is no dedicated automated test workflow documented in this repo; if validation is needed, use the build task and relevant device-facing monitor tasks.

## Conventions

- Code comments use English; Markdown documentation uses Chinese by default.
- Use UTF-8, LF line endings, 4-space indentation, lowercase file and directory names, and ensure each text file ends with exactly one trailing newline.
- Follow existing local patterns before introducing new structure.
- New `.c` and `.h` files should follow the templates in `rep/newfile/` unless the target directory already has a stronger local pattern.
- Keep `typedef` declarations and macros in headers.
- Use camelCase identifiers; prefix globals and file-scope statics with `g`, local temporaries with `l`, structs with `st`, and enums with `e`.

## Working Rules

- Link to existing docs instead of duplicating them in responses or new documentation.
- For repo rules and boundaries, use `rep/rule/rule.md`, `rep/rule/map.md`, `rep/rule/projectrule.md`, and `rep/rule/coderule.md` as the authority.
- For project-bound layout, use `README.md` and `User/README.md`.
- When working in `User/port/`, keep files flat under that directory; do not recreate nested driver or module folder trees there.
- Treat `manager` and `system` as project-bound unless the task explicitly refactors reusable patterns into `rep/`.