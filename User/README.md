# rebuildcpr project-bound tree

This directory holds code that belongs to the current product repository instead of the `rep` architecture repository.

## What belongs here

- Board and product binding for reusable `rep` modules and drivers
- Product-level `rep_config.h`
- Service orchestration in `manager/`
- Startup wiring and task layout in `system/`

## What must stay out of here

- Reusable module core
- Reusable driver core
- Generic tools with no board dependency
- Cross-project adapter logic that can live in `rep/`

## Directory contract

- `binding/`: the only place for project-side port and assembly files
- `manager/`: product service orchestration and coordination
- `system/`: task creation, startup order, and system mode collaboration

If a file mainly answers "how this board or product is wired", it belongs here.