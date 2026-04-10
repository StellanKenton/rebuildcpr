# Project Manager Layer

This directory is reserved for product services and service orchestration.

Place these here:

- Power, self-check, update, and other product services
- Product-specific service composition
- Health aggregation that depends on the current product behavior

Do not place reusable module core or raw board pin bindings here. Those belong in `rep/` and `binding/` respectively.

When a manager utility becomes stable across projects, extract the reusable part into `rep/manager/` or another reusable layer and keep only the product wiring here.