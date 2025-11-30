# AI Coding Style Guide for This Repo

This guide is for AI coding agents. It **extends** the rules in `.clinerules` and the `memory-bank/` docs — those are the source of truth.

**Always:**

1. Read the relevant `memory-bank/*.md` files.
2. Search the repo for similar functionality and copy the existing patterns.
3. Match the style and structure you find there before writing new code.

If there’s ever a conflict, **follow the existing code + memory-bank, then this guide.**

---

## 1. Core Principles

- **Single Responsibility (SRP) at every level**
    - Each function, class, and module should do **one thing** and do it clearly.
    - If you’re adding “and also” to a description of a function, it probably needs to be split.
- **Low cognitive load**
    - Prefer more, smaller, obvious units over fewer, clever ones.
    - Avoid ambiguity: it should be obvious *why* each file / function exists.
- **Follow existing patterns first**
    - Before introducing a pattern or approach, find the closest existing example and copy its style.
    - Do **not** introduce new frameworks or architectures unless explicitly asked.

---

## 2. Naming, API Shape, and Parameters

- **Domain-centric, human-readable names**
    - Use the language of the business/domain; avoid cryptic abbreviations.
    - Names should reflect intent: `calculateInvoiceTotal`, `scheduleRetryForPayment`, etc.
- **Functions with few parameters**
    - Prefer ≤ 3 parameters.
    - If you need more, use:
        - A domain object, or
        - A typed/config object (e.g. `{ userId, featureFlag, requestId }`).
    - Avoid boolean flags that change behavior (`doThing(x, true)`) — split into separate functions.
- **Consistency with the repo**
    - Match naming patterns and suffixes/prefixes already used (`Service`, `Repository`, `Handler`, etc.).

---

## 3. Design & Dependency Management

- **Use known design patterns when they clearly apply**
    - E.g. Strategy for pluggable behaviors, Factory for object creation, etc.
    - Prefer patterns that already appear in the repo or are clearly justified by the task.
- **Prefer Dependency Injection**
    - Don’t hardcode concrete dependencies inside classes/functions.
    - Accept dependencies via constructor or function parameters.
    - Favor interfaces/abstractions where the repo already does so.
    - Make code easy to test by injecting collaborators, not instantiating them internally.
- **Keep layers clean**
    - Controllers/handlers: input/output, validation, mapping.
    - Services: business logic.
    - Data access: repositories/gateways.
    - Avoid leaking infra concerns into business logic where possible.

---

## 4. Testing (UT Focus)

When adding or changing behavior, **think tests first**:

- **Case mapping**
    - Enumerate business cases:
        - Success paths
        - Failure paths (validation errors, external failures, edge conditions)
        - Boundary conditions (min/max, empty collections, time-related edges, etc.)
    - Map each case to at least one unit test.
- **Business-first, math-aware**
    - Express tests in business terms: “should reject expired token”, “should cap discount at X”.
    - Use basic mathematical reasoning to find edge cases (off-by-one, rounding, thresholds).
- **Test structure**
    - Use a clear Arrange–Act–Assert pattern.
    - One logical assertion per test (grouped assertions are fine if they express a single scenario).
    - Match the existing test layout and naming conventions in the repo.
- **Isolation**
    - Unit tests should mock or fake external dependencies (DB, APIs, queues) according to repo norms.
    - Do not hit real external services from unit tests.

---

## 5. Metrics & Monitoring

For new features / flows, **bake in observability**:

- **Define metrics from the business perspective**
    - Identify key metrics early:
        - Volume: how often the feature is invoked.
        - Success: number of successful operations.
        - Failure: counts by failure type / error code.
        - Key outcomes: e.g. conversion, accepted/rejected decisions.
    - Use metric names consistent with existing naming patterns.
- **Where to add metrics**
    - At boundaries of major operations (start/end of a flow, external calls, key decision points).
    - Avoid metric explosion: reuse existing metrics where possible, extend with labels/tags if consistent with the repo.
- **Launch dashboards**
    - For significant new features, ensure metrics can support:
        - Overall success rate
        - Failure breakdown (by type)
        - Volume over time
    - Expose enough metrics so a simple launch dashboard can be built or extended.

---

## 6. Logging

- **Purposeful, not noisy**
    - Log at subtle or critical points:
        - Important decisions
        - Unexpected but recoverable conditions
        - Integration failures
    - Avoid logging every tiny step without strong debugging value.
- **Clear, traceable logs**
    - Include identifiers that help correlate events:
        - Request/correlation ID
        - Key domain IDs (userId, orderId, etc.)
    - Log messages should be human-readable and actionable: what happened, where, why.
- **Consistent levels**
    - Follow existing repo conventions for `DEBUG` / `INFO` / `WARN` / `ERROR`.
    - Don’t log expected failures at `ERROR` if they’re part of normal control flow.

---

## 7. How to Behave When Unsure

- Prefer **small, incremental changes** matching existing patterns over reinvention.
- If you can’t find a good example:
    - State assumptions in comments or PR-style notes.
    - Suggest updates to `memory-bank/*` to capture any new patterns introduced.
- Never invent public APIs, DB schemas, or major new modules unless the task explicitly asks for a design/greenfield solution.

---

## 8. Presenting Options & Design Choices

When there is more than one reasonable way to implement something, **don’t just pick one silently**. Propose options and let the user choose.

### 8.1. Classify task complexity

Before coding, quickly classify the task:

- **Trivial**
    - Small, obvious changes
    - Tiny bugfixes
    - One-liner refactors
    - Purely following an existing pattern with no meaningful design choice
- **Medium**
    - New logic in an existing flow
    - Small features or localized refactors
    - Non-trivial branching or behavior changes
- **High**
    - New feature/endpoint/module
    - Data model changes
    - Cross-cutting behavior
    - Anything with multiple architectural choices

### 8.2. How many options to propose

- **Trivial tasks**
    - It’s OK to:
        - Propose **1 clear option**, or
        - At most **2 options** if there’s a real tiny trade-off.
    - You may say, for example:
        
        > This is trivial; I’ll take Option A by default unless you prefer B.
        > 
- **Medium to high complexity tasks**
    - Propose **2–4 options** *when it makes sense* (i.e. there are real trade-offs).
    - Each option should:
        - Follow existing repo patterns,
        - Be realistically implementable with current code,
        - Be distinct enough to matter (not cosmetic variations).

### 8.3. How to present options

For each option, briefly include:

- **Name**
    
    e.g. “Option A – New service method”, “Option B – Extend existing handler”.
    
- **Short description**
    
    1–3 sentences explaining the approach.
    
- **Pros / cons**
    
    Concise bullets focused on trade-offs (complexity, extensibility, risk, alignment with current patterns).
    
- **When to prefer it**
    
    e.g. “Best if we expect this feature to grow”, “Best if we want minimal diff now”.
    

Example structure:

> Option A – Reuse existing PaymentService
> 
> - Pros: Minimal changes, reuses battle-tested code.
> - Cons: Slightly increases coupling to existing flow.
> - Best when: We want the fastest, safest path with minimal refactor.

### 8.4. Ask the user to choose

After listing options for **medium/high** complexity work:

- Explicitly **ask for a choice** instead of assuming:
    
    > Which option would you like to go with (A/B/C/…)?
    > 
    > 
    > If you tell me your priorities (speed vs extensibility, etc.), I can recommend one.
    > 
- Do **not** start implementing a non-trivial design until:
    - The user chooses an option, or
    - The user explicitly says things like “you decide”, “pick the best one”, or similar.

For trivial tasks, it is acceptable to:

- Recommend a default option and proceed, while briefly noting the alternative if relevant.

---

## 9. Error Handling & Validation

- **Fail fast at boundaries**
    - Validate as close to input as possible (controllers/handlers, message consumers, etc.).
    - Keep service/business logic working with already-validated, well-typed data.
- **Domain‑meaningful errors**
    - Prefer domain or use‑case specific errors (with clear names / codes) over generic “Error” everywhere.
    - Map errors to:
        - appropriate HTTP status codes or protocol responses, and
        - meaningful metrics (e.g. `payment_declined`, `validation_failed`, `integration_timeout`).
- **Don’t swallow errors silently**
    - Either:
        - handle and convert them into a known domain outcome (with metrics + logs), or
        - rethrow / propagate with more context.
    - Avoid empty `catch` blocks or returning `null`/magic values without a clear reason.
- **Keep error + metrics + logging aligned**
    - For important failure modes:
        - domain error type
        - metric label / code
        - log message
    - should all tell the same story so debugging a production issue is straightforward.

---

## 10. Comments & Documentation Style

- **Comment the “why”, not the “what”**
    - Code + names should explain **what** is happening.
    - Use comments for:
        - non‑obvious business rules
        - tricky edge cases
        - invariants / assumptions that tests rely on
- **Docstrings for public-ish entry points**
    - For key services, handlers, or library-style helpers:
        - Briefly describe inputs, outputs, and key side effects.
        - Mention important preconditions or expectations (“assumes validated X”, “must be called after Y”).
- **Keep comments close to reality**
    - If the implementation changes, update or remove comments that no longer match.
    - Don’t add large, speculative design docs into comments — keep them in `memory-bank/` instead.

---

## 11. Formatting, Tooling & Safety

- **Respect existing formatting & linters**
    - Follow whatever format the repo is already using.
    - Don’t introduce new formatting styles or tools unless explicitly requested.
    - Avoid large “format only” diffs; keep changes focused.
- **Builds and clean runs**
    - By default, run **clean** builds and test pipelines so results don’t depend on stale artifacts.
    - Use faster incremental or partial builds only if the user explicitly asks for them or the repo’s docs clearly recommend them for local iteration.
- **Change / review overviews**
    - Keep code review overviews and change summaries concise: state the intent, main areas touched, and any notable risks or follow-ups.
    - Avoid dumping large code blocks or restating the full diff; reference key files and symbols instead.
- **Minimal‑diff mindset**
    - Prefer small, surgical edits that:
        - add new behavior,
        - fix bugs, or
        - refactor for SRP,
        without unnecessarily moving or renaming unrelated code.
    - This makes reviews easier and keeps cognitive load low.
- **Security & PII awareness**
    - Never log secrets, tokens, passwords, or sensitive personal data.
    - When in doubt, log stable identifiers (IDs) and high‑level states, not raw payloads.
    - Follow existing patterns for:
        - accessing secrets/config (env vars, secret managers)
        - permission/authorization checks (don’t invent new ad‑hoc checks)
- **Configuration and feature flags**
    - Reuse existing config/feature‑flag mechanisms where possible.
    - Avoid introducing new env vars or flags without a clear need and consistent naming.
    - Keep behavior deterministic in tests (no hidden reliance on “whatever is in the environment”).
