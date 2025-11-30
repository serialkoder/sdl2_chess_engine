I want you to set up persistent AI-facing documentation for this repo so you can reuse it in every future task.

## Goal

Create a lightweight “AI docs / memory bank” inside this repo that explains:

- Where things live
- Which files are good examples
- How new code should be structured so it matches existing patterns
- How to build, test, lint, and run the different apps/packages in this repo

## What to create

1. Create a folder at the project root called `memory-bank/`.

2. Inside `memory-bank/`, create these markdown files:

   - `00-project-overview.md`
     - High-level description of what this repo is for.
     - Main apps/services/packages.
     - Key external dependencies (APIs, DBs, queues, third-party systems).
     - Any important constraints (performance, security, compliance, etc.).

   - `01-dev-commands.md`
     - **Source of truth** for how to run things.
     - Global commands (if any):
       - install / bootstrap (e.g. `pnpm install`, `make init`)
       - top-level build / test / lint commands, if they exist.
     - For each app / package / service you discover:
       - Name and path (e.g. “orders-service – `packages/orders-service`”).
       - Build command(s).
       - Test command(s) (unit/integration/CI variants).
       - Lint / typecheck commands.
       - Required environment variables or services for local tests (DBs, queues, docker-compose, etc.).
     - Keep this practical: these should be the commands real developers would actually run.

   - `02-architecture.md`
     - Major modules / layers and how they fit together.
     - For each area, point to key files and “golden example” implementations.
     - Describe important cross-cutting concerns (auth, error handling, logging, observability, etc.).

   - `03-domain-glossary.md`
     - Important domain concepts and their meanings (e.g. “Order”, “Invoice”, “Subscription”).
     - How they map to code (modules, classes, DB tables, events).

   - `04-patterns-and-pitfalls.md`
     - Common patterns that should be copied (factories, service layouts, error handling, etc.).
     - Known pitfalls and anti-patterns to avoid in this codebase.
     - Any “gotchas” around state, concurrency, performance, external systems, etc.

   - `05-best-examples.md`
     - A curated list of “golden example” files for common tasks.
     - For each example, explain why it’s good and when to copy it.

   - `06-open-questions.md`
     - Things that are unclear, contradictory, or need human clarification.
     - Use this instead of guessing when the code or docs don’t give a clear answer.

## How to discover dev commands for `01-dev-commands.md`

Dev commands must be discovered dynamically from this repo. Do **not** rely on prior experience with other projects when you can inspect configs and docs here.

When building `01-dev-commands.md`:

1. Identify all relevant apps/packages/services:
   - Look for common markers such as:
     - `package.json`, `pyproject.toml`, `setup.cfg`, `Cargo.toml`, `go.mod`, `pom.xml`, `Makefile`, docker-compose files, etc.
   - For monorepos, treat each package/app folder as a separate section.

2. For each app/package:
   - Inspect:
     - Its own config files (e.g. that folder’s `package.json` / `pyproject.toml` / `Makefile`, etc.).
     - Any CI configs or scripts that reference it (e.g. GitHub Actions, CircleCI, etc.).
     - README / docs that mention how to build/test it.
   - From those, infer:
     - Build command(s).
     - Test command(s) (including specific variants like unit, integration, CI).
     - Lint / typecheck command(s).
     - Required environment variables and services for running tests locally.

3. Prefer commands that:
   - Actually exist in the repo.
   - Are already used in CI or existing docs.
   - Reflect how developers are expected to work day to day.

4. Only ask the user if:
   - The repo genuinely has multiple conflicting options, OR
   - You cannot find any plausible command in configs/docs after a reasonable search.

5. When something is ambiguous:
   - Document the ambiguity in `06-open-questions.md`.
   - Use the safest reasonable default and clearly label it as “best guess”.

You should NOT ask the user to type out basic dev commands manually, except as a last resort when the repo genuinely does not contain enough information.

## How to work

- First, scan the repo structure (and any attached docs) to understand it:
  - Top-level layout.
  - Key apps/packages.
  - Build systems / tooling.

- Then propose a short plan to the user:
  - Which files under `memory-bank/` you will create.
  - Which parts of the repo you’ll inspect to fill them in.
  - How you’ll discover dev commands for `01-dev-commands.md`.

- After the user approves:
  - Create the `memory-bank/` folder.
  - Write the markdown files described above.
  - Use links/paths to **real** code and config files (e.g. `src/users/user.service.ts`, `packages/orders-service/package.json`).

- Be concise but specific:
  - Focus on information that will help you and other AI agents work effectively on future tasks.
  - Avoid restating the entire README or duplicating low-value details.

## Golden rule

When in doubt, don’t make things up.

If you’re unsure about a pattern, command, or architectural detail:

- Prefer to:
  - Mark it under `06-open-questions.md`, and
  - Clearly list your uncertainty in the markdown,
- Rather than inventing new APIs, methods, or commands that don’t actually exist in this repo.
