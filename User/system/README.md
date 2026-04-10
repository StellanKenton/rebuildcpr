# Project System Layer

This directory is reserved for startup wiring, task creation, and system mode orchestration for the current product.

Place these here:

- Task entry registration
- Task period and priority selection
- Startup sequencing and boot-time orchestration
- Product-level console and debug command wiring

Do not place long-term reusable module or driver logic here. `system/` is project-bound by default.