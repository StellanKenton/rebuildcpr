# rebuildcpr

## Repository Layout

This repository is now split by reuse boundary instead of by raw layer directory.

- `rep/`: architecture submodule. Keep reusable core, reusable adapters, and stable layer contracts here.
- `project/rebuildcpr/`: current product repository content. Keep project binding, manager orchestration, system task wiring, and product configuration here.
- `Core/`, `Drivers/`, `Middlewares/`, `USB_DEVICE/`, `MDK-ARM/`: STM32CubeMX and toolchain generated project files.

## Boundary Rules

Use these rules when placing new code:

- Put stable module core, driver core, tools, and reusable adapter logic into `rep/`.
- Put board wiring, device instances, addresses, link IDs, task periods, startup order, and feature switches into `project/rebuildcpr/`.
- Treat `manager` and `system` as project-bound by default. Reuse their patterns, not their full product implementation.
- Treat `binding` as the only place for project-side port and assembly code.

## Project-Side Layout

`project/rebuildcpr/` is organized as follows:

- `binding/`: project-specific bindings for reusable `rep` components.
- `manager/`: product services and orchestration logic.
- `system/`: task creation, startup wiring, and mode-level orchestration.
- `rep_config.h`: product-level compile-time configuration that overrides reusable defaults.

## Migration Guidance

When you move a module to another project, copy the reusable component from `rep/` and only recreate the thin project binding in that target project's `binding/` directory. Do not copy the whole `manager/` or `system/` tree unless that project intentionally shares the same orchestration model.