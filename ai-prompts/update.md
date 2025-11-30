Your task is to update the AI docs in `memory-bank/` to reflect the latest state of the codebase.

## Steps

1. Read all files in `memory-bank/`.
2. Scan the relevant parts of the codebase and any new docs I attach with `@` (tickets, specs, wiki exports).
3. For each file:
   - Fix anything that’s now wrong or outdated.
   - Add new sections for any new features, modules, or patterns.
   - Ensure every “golden example” still exists and is still the best reference.

## Special attention: `01-dev-commands.md`

- Treat `memory-bank/01-dev-commands.md` as the canonical source of truth for how to build, test, lint, and run the repo and its packages.

When updating it:

1. Re-derive the commands from the current repo state:
   - README / other human docs.
   - Package-level configs (`package.json`, `pyproject.toml`, `Makefile`, `go.mod`, etc.).
   - CI configs and scripts.
2. Fix anything that’s wrong or outdated:
   - Scripts that no longer exist.
   - New packages that aren’t documented yet.
   - New required env vars, flags, or steps (e.g. migrations).
3. If you’ve been using additional flags or env vars in practice:
   - Only promote them into the canonical commands after explaining the change to the user and getting approval.
   - Clearly label CI-only or debug-only variants.

## Important

- Do NOT change the overall structure of `memory-bank/` unless it’s obviously necessary.
- Never invent new APIs, methods, or folders just for documentation. Only describe what actually exists in the code.
- If something is unclear or contradictory, add a short “Open questions” section (or update `06-open-questions.md`) instead of guessing.
