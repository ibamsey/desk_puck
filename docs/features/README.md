# Features (documentation index)

Product features are implemented under **`src/features/<name>/`**. Each folder holds code, assets, tools, and **feature docs** together.

| Feature | Doc entry | Code & assets |
|---------|-----------|----------------|
| **Clock** | [src/features/clock/docs/README.md](../../src/features/clock/docs/README.md) | [src/features/clock/](../../src/features/clock/) |
| **Diary** | [src/features/diary/docs/README.md](../../src/features/diary/docs/README.md) | [src/features/diary/](../../src/features/diary/) |
| **Patterns** | (inline in source) | [src/features/patterns/](../../src/features/patterns/) |
| **Cube** | (inline in source) | [src/features/cube/](../../src/features/cube/) |
| **WhatsApp** | [src/features/whatsapp/docs/README.md](../../src/features/whatsapp/docs/README.md) | [src/features/whatsapp/](../../src/features/whatsapp/) (planning) |

Repo-wide docs stay here under [docs/](../): [overview.md](../overview.md), [hardware.md](../hardware.md), [bringup.md](../bringup.md). Architecture decisions: [adr/](../../adr/).

## New feature checklist

1. Copy [_template.md](_template.md) into `src/features/<kebab-name>/docs/README.md`.
2. Add a row to the table above.
3. Register the feature in `src/app/app_shell.cpp` ([ADR-01](../../adr/ADR-01-touch-navigation-and-app-shell.md)).
