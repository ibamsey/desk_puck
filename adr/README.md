# Architecture Decision Records (ADR)

We capture **significant firmware and product architecture choices** here so feature specs ([docs/features/](../docs/features/README.md)) stay focused on *what* the gadget does, while ADRs explain *how* the codebase is structured and why.

## Naming

| Item | Convention |
|------|------------|
| Folder | `adr/` at repository root |
| Files | `ADR-nn-short-title.md` — two-digit sequence, kebab-case title |
| Template | [_template.md](_template.md) (not numbered) |

Next free number: assign the lowest unused `nn` (currently **02**).

## Status lifecycle

| Status | Meaning |
|--------|---------|
| Proposed | Under discussion; do not treat as binding for implementation |
| Accepted | Team agrees; implement unless superseded |
| Deprecated | No longer recommended; kept for history |
| Superseded | Replaced by another ADR (link the successor) |

When superseding, **do not delete** the old ADR — change its status and add a link.

## How to add an ADR

1. Copy [_template.md](_template.md) to `ADR-nn-your-title.md`.
2. Fill in Context, Decision, and Consequences; list options you rejected under **Alternatives considered**.
3. Set **Status** to Proposed or Accepted.
4. Add a row to the index below.
5. Link from [docs/overview.md](../docs/overview.md) if the decision affects how new code is laid out.

## Index

| ADR | Title | Status |
|-----|-------|--------|
| [ADR-00](ADR-00-arduino-runtime-and-freertos.md) | Arduino runtime and FreeRTOS | Accepted |
| [ADR-01](ADR-01-touch-navigation-and-app-shell.md) | Touch navigation and app shell | Accepted |

## Related docs

| Doc | Role |
|-----|------|
| [docs/overview.md](../docs/overview.md) | High-level data flow and extension checklist |
| [docs/features/](../docs/features/README.md) | Per-feature behaviour and UX |
| [AGENTS.md](../AGENTS.md) | Assistant-oriented coding conventions |
