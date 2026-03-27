# Lightpad TODO: Diagnostics and On-the-Fly Static Analysis

## Objective
Deliver robust, per-language diagnostics (errors, warnings, info, hints) that update on-the-fly while editing and are integrated into:
- Problems panel
- Status bar counts
- Editor visuals (squiggles and gutter markers)
- Per-file language assignment pipeline

This document replaces broad backlog noise with a detailed execution plan for one high-impact feature area.

## Scope
In scope:
- LSP-backed diagnostics lifecycle (open/change/save/close)
- Central language feature assignment per file
- Diagnostics aggregation and fan-out to UI
- On-the-fly updates with debounce and stale-result protection
- Per-language server management and settings
- Tests and instrumentation

Out of scope (separate epics):
- Marketplace and plugin store
- Remote development
- AI features
- Advanced refactorings beyond code actions

## Current Status (Audited)
- [x] LSP client has protocol support for `didOpen`, `didChange`, `didSave`, `didClose`, and `publishDiagnostics`.
- [x] Problems panel can store/display diagnostics and severity counts.
- [x] Per-file language override exists and is persisted in `.lightpad/highlight_config.json`.
- [x] Language-specific server configs exist for C/C++, Python, Rust, Go, TS/JS, Java.
- [x] Diagnostics are wired from active LSP sessions into Problems panel.
- [x] Central diagnostics manager exists.
- [x] Editor inline diagnostics rendering (squiggles/gutter diagnostics) exists.
- [x] Problems panel `refreshRequested` is connected to a save-triggered refresh pipeline.
- [x] On-the-fly diagnostics pipeline is connected to editor text changes and honors `diagnostics.debounceMs`.
- [x] LSP completion provider is registered in the completion setup path.

## Definition of Done
Feature is done when all are true:
- [ ] Opening a supported file starts the correct language service and sends `didOpen`.
- [ ] Typing triggers debounced `didChange` and diagnostics refresh without manual save.
- [ ] Save triggers `didSave` and diagnostics are refreshed if server requires it.
- [ ] Closing a file sends `didClose` and clears stale diagnostics for that document.
- [ ] Problems panel updates live with accurate counts by severity and file.
- [ ] Status bar reflects current global counts from central diagnostics state.
- [ ] Clicking a problem navigates to exact file/line/column.
- [ ] Editor shows inline diagnostics (underline) and gutter markers.
- [ ] Switching language override for a file rebinds diagnostics to the new provider.
- [ ] Unit/integration tests pass for lifecycle, mapping, debounce, and stale result handling.

## Milestones

### M0 - Foundation and Guardrails
- [ ] Add `docs/DIAGNOSTICS_ARCHITECTURE.md` with data flow diagram.
- [ ] Define canonical URI/path conversion utilities and use one implementation everywhere.
- [ ] Add feature flags in settings:
  - [ ] `diagnostics.enabled`
  - [ ] `diagnostics.onType`
  - [ ] `diagnostics.onSave`
  - [ ] `diagnostics.debounceMs`
- [ ] Add structured diagnostic logging categories and levels.

Acceptance criteria:
- [ ] Architecture doc reviewed.
- [ ] Settings loaded with defaults and can be toggled safely.

### M1 - Central Diagnostics Core
Create a central service (suggested: `DiagnosticsManager`) to own diagnostic state and routing.

- [ ] Add `App/diagnostics/diagnosticsmanager.h/.cpp`.
- [ ] Define model:
  - [ ] `QMap<QString, QList<LspDiagnostic>> byUri`
  - [ ] reverse index by file path
  - [ ] aggregate counts (error/warning/info/hint)
- [ ] Public API:
  - [ ] `upsertDiagnostics(uri, diagnostics, sourceId)`
  - [ ] `clearDiagnostics(uri)`
  - [ ] `clearAllForSource(sourceId)`
  - [ ] `diagnosticsForFile(filePath)`
- [ ] Signals:
  - [ ] `diagnosticsChanged(uri)`
  - [ ] `countsChanged(errors, warnings, infos)`
  - [ ] `fileCountsChanged(filePath, ...)`
- [ ] Implement stale-result protection by version/epoch per document.

Acceptance criteria:
- [ ] Unit tests for merge/replace/clear semantics.
- [ ] Unit tests for URI and file path matching.

### M2 - Language Service Session Management
Add a reusable session manager (suggested: `LanguageFeatureManager`) that manages per-language LSP clients for diagnostics and future features.

- [ ] Add `App/language/languagefeaturemanager.h/.cpp`.
- [ ] Session responsibilities:
  - [ ] resolve language id per file (override + detection)
  - [ ] choose configured server command/args per language
  - [ ] lazy-start and reuse client per language/workspace
  - [ ] expose `clientForFile(filePath)`
- [ ] Hook `LspClient::diagnosticsReceived` to `DiagnosticsManager`.
- [ ] Implement document lifecycle methods:
  - [ ] `openDocument(filePath, languageId, text)`
  - [ ] `changeDocument(filePath, version, text)`
  - [ ] `saveDocument(filePath)`
  - [ ] `closeDocument(filePath)`
- [ ] Handle server unavailable state and report actionable messages.

Acceptance criteria:
- [ ] Switching between multiple files in same language reuses one session.
- [ ] Unsupported language does not crash and logs clear reason.

### M3 - MainWindow and Editor Wiring
Connect editor and window events to service lifecycle.

- [ ] On open file:
  - [ ] send `didOpen` with current text and effective language id.
- [ ] On text change:
  - [ ] increment per-document version counter.
  - [ ] send debounced `didChange` when `diagnostics.onType = true`.
- [ ] On save:
  - [ ] send `didSave` when `diagnostics.onSave = true`.
- [ ] On tab close:
  - [ ] send `didClose` and clear diagnostics for file.
- [ ] On language override change:
  - [ ] close old language session binding for that file.
  - [ ] open document on new language session.
  - [ ] clear stale diagnostics from previous source.

Acceptance criteria:
- [ ] End-to-end diagnostics update on typing for at least one language server.
- [ ] No duplicate `didOpen` for same document/session pair.

### M4 - Problems Panel Integration
Wire central diagnostics state to Problems panel and status bar.

- [ ] Replace ad-hoc panel updates with manager-driven updates.
- [ ] Connect `DiagnosticsManager::countsChanged` to status label update.
- [ ] Connect per-file updates to file tree badges (if enabled).
- [ ] Wire `ProblemsPanel::refreshRequested(filePath)` to on-demand reanalysis.
- [ ] Add panel actions:
  - [ ] clear current file
  - [ ] clear all
  - [ ] copy problem message

Acceptance criteria:
- [ ] Problems panel reflects manager state exactly.
- [ ] Auto-refresh on save actually triggers reanalysis.

### M5 - Editor Rendering (Inline + Gutter)
Introduce visual diagnostics directly in code editor.

- [ ] Extend `TextArea` to render diagnostic underlines via `ExtraSelection`.
- [ ] Severity styles:
  - [ ] Error: red underline
  - [ ] Warning: yellow underline
  - [ ] Info: blue underline
  - [ ] Hint: subtle dotted underline
- [ ] Extend `LineNumberArea` for diagnostic markers and tooltips.
- [ ] Add hover tooltip on marked lines with message/source/code.
- [ ] Ensure coexistence with breakpoint and git-diff visuals.

Acceptance criteria:
- [ ] Marker layering is deterministic (breakpoint, diff, diagnostics).
- [ ] Performance remains acceptable on large files (no full redraw loops).

### M6 - Per-Language Analyzer Strategy
Define language strategy as "LSP-first, CLI fallback optional".

- [ ] Language support matrix in settings/docs:
  - [ ] C/C++: clangd
  - [ ] Python: pylsp (or basedpyright/pylance-compatible setup if added later)
  - [ ] Rust: rust-analyzer
  - [ ] Go: gopls
  - [ ] TS/JS: typescript-language-server
  - [ ] Java: jdtls
- [ ] Add optional fallback analyzers (off by default):
  - [ ] Python: ruff
  - [ ] JS/TS: eslint
  - [ ] C/C++: clang-tidy/cppcheck
- [ ] Normalize fallback output into `LspDiagnostic` shape.
- [ ] De-duplicate diagnostics when both LSP and fallback are enabled.

Acceptance criteria:
- [ ] Each configured language emits diagnostics in same UI pipeline.
- [ ] Fallback analyzers cannot block UI thread.

## Detailed Task Board

### A. Data Model and Utilities
- [ ] Add `DiagnosticOrigin` enum (`Lsp`, `CliAnalyzer`, `Manual`).
- [ ] Add `DocumentState` store: uri, filePath, languageId, version, isOpen.
- [ ] Add utility for URI normalization and percent-decoding safety.
- [ ] Add utility to clamp diagnostic ranges to valid document bounds.

### B. Concurrency and Performance
- [ ] Debounce on-type diagnostics (`150-300ms`, configurable).
- [ ] Coalesce rapid updates per file.
- [ ] Ignore responses older than active document version.
- [ ] Avoid rebuilding full Problems tree for single-file changes.
- [ ] Add metric counters:
  - [ ] `diagnostics_updates_total`
  - [ ] `diagnostics_latency_ms`
  - [ ] `diagnostics_dropped_stale_total`

### C. UX and Behavior
- [ ] Add status text variants:
  - [ ] `No problems`
  - [ ] `<E> errors, <W> warnings`
  - [ ] `Analyzing...`
- [ ] Add command palette actions:
  - [ ] Toggle diagnostics on type
  - [ ] Re-run diagnostics for current file
  - [ ] Clear diagnostics
- [ ] Add preference UI for diagnostics settings.

### D. Error Handling
- [ ] If server executable is missing, show one-time actionable message with install hint.
- [ ] If server crashes, mark session degraded and auto-retry with backoff.
- [ ] If invalid diagnostics payload is received, log and continue.

### E. Telemetry and Debugging
- [ ] Add debug log line per lifecycle event (`open/change/save/close`).
- [ ] Add debug log line per diagnostics publish with counts and file.
- [ ] Add runtime command to dump diagnostics state to logs.

## File-Level Implementation Plan

### New files
- [ ] `App/diagnostics/diagnosticsmanager.h`
- [ ] `App/diagnostics/diagnosticsmanager.cpp`
- [ ] `App/language/languagefeaturemanager.h`
- [ ] `App/language/languagefeaturemanager.cpp`
- [ ] `tests/unit/test_diagnosticsmanager.cpp`
- [ ] `tests/unit/test_languagefeaturemanager.cpp`
- [ ] `docs/DIAGNOSTICS_ARCHITECTURE.md`

### Existing files to modify
- [ ] `App/ui/mainwindow.h`
- [ ] `App/ui/mainwindow.cpp`
- [ ] `App/core/textarea.h`
- [ ] `App/core/textarea.cpp`
- [ ] `App/core/editor/linenumberarea.h`
- [ ] `App/core/editor/linenumberarea.cpp`
- [ ] `App/ui/panels/problemspanel.h`
- [ ] `App/ui/panels/problemspanel.cpp`
- [ ] `App/lsp/lspclient.h`
- [ ] `App/lsp/lspclient.cpp`
- [ ] `App/CMakeLists.txt`
- [ ] `tests/CMakeLists.txt`

## Test Plan

### Unit tests
- [ ] `DiagnosticsManager`: upsert/replace/clear/counts.
- [ ] `DiagnosticsManager`: URI to file matching edge cases.
- [ ] `LanguageFeatureManager`: language resolution with override precedence.
- [ ] `LanguageFeatureManager`: server startup failure handling.
- [ ] Versioning: stale diagnostics dropped.

### Integration tests
- [ ] Open file -> `didOpen` sent once.
- [ ] Type -> debounced `didChange` -> diagnostics update.
- [ ] Save -> `didSave` path updates diagnostics.
- [ ] Close tab -> `didClose` + cleanup.
- [ ] Change language override -> rebind session and refresh diagnostics.

### Manual QA checklist
- [ ] C++ diagnostics appear while typing and clear when fixed.
- [ ] Python diagnostics appear while typing and clear when fixed.
- [ ] TS/JS diagnostics appear while typing and clear when fixed.
- [ ] Problems panel filtering works under continuous updates.
- [ ] Editor remains responsive in large files.

## Rollout Plan

### Stage 1 (safe baseline)
- [ ] M0 + M1 + M2 with diagnostics visible in Problems panel only.

### Stage 2 (interactive)
- [ ] M3 + M4 with live status and refresh actions.

### Stage 3 (full UX)
- [ ] M5 inline/gutter rendering enabled by default.

### Stage 4 (language depth)
- [ ] M6 optional CLI fallback analyzers and deduplication.

## Risks and Mitigations
- Risk: high-frequency diagnostics flood UI.
  - [ ] Mitigation: debounce + coalescing + partial tree updates.
- Risk: conflicting diagnostics from multiple sources.
  - [ ] Mitigation: source tagging + dedup rules.
- Risk: server startup instability.
  - [ ] Mitigation: retry with capped backoff and visible state.
- Risk: URI/path mismatch across platforms.
  - [ ] Mitigation: single normalization utility and unit tests.

## Open Decisions
- [ ] Decide default debounce (`150ms` vs `250ms`).
- [ ] Decide whether `onType` should be enabled by default for all languages.
- [ ] Decide whether fallback CLI analyzers ship enabled or experimental.
- [ ] Decide whether diagnostics are workspace-scoped or global across windows.

## Immediate Next 10 Tasks
1. [ ] Add `DiagnosticsManager` skeleton and tests.
2. [ ] Add `LanguageFeatureManager` skeleton and server config plumbing.
3. [ ] Wire `diagnosticsReceived` from `LspClient` into manager.
4. [ ] Wire Problems panel to manager signals.
5. [ ] Add per-document version tracking in `MainWindow`.
6. [ ] Send `didOpen` on file open.
7. [ ] Send debounced `didChange` on text edits.
8. [ ] Send `didSave` on save and connect panel refresh action.
9. [ ] Send `didClose` on tab close and cleanup diagnostics.
10. [ ] Add inline underline rendering for error/warning first.

## Progress Tracking
- [ ] M0 complete
- [ ] M1 complete
- [ ] M2 complete
- [ ] M3 complete
- [ ] M4 complete
- [ ] M5 complete
- [ ] M6 complete

Owner: TBD
Target branch: `feature/diagnostics-pipeline`
Last updated: 2026-03-01
