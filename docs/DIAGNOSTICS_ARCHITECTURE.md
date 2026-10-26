# Diagnostics Architecture

## Overview

Lightpad provides on-the-fly diagnostics (errors, warnings, info, hints) powered
by Language Server Protocol (LSP) language servers.  The pipeline is designed to
be language-agnostic: each language registers a server configuration and the
framework handles document lifecycle, result aggregation, and UI fan-out.

## Data Flow

```
 ┌──────────┐    didOpen / didChange / didSave / didClose
 │  Editor   │ ──────────────────────────────────────────────►┌─────────────────────────┐
 │ (TextArea)│                                                │ LanguageFeatureManager  │
 └──────────┘◄──────────────────────────────────────────────  │  • resolve language id  │
       ▲      diagnostics for inline rendering                │  • lazy-start LspClient │
       │                                                      │  • route lifecycle calls│
       │                                                      └────────────┬────────────┘
       │                                                                   │
       │                                                      LspClient::diagnosticsReceived
       │                                                                   │
       │                                                                   ▼
       │                                                      ┌────────────────────────┐
       │                                                      │  DiagnosticsManager    │
       │  diagnosticsChanged(uri)                             │  • per-uri state store │
       └──────────────────────────────────────────────────────│  • version gating      │
                                                              │  • aggregate counts    │
                                                              └────────────┬───────────┘
                                                                           │
                                                          diagnosticsChanged / countsChanged
                                                                           │
                                                              ┌────────────▼───────────┐
                                                              │   ProblemsPanel        │
                                                              │   Status Bar           │
                                                              └────────────────────────┘
```

## Key Components

### DiagnosticsManager (`App/diagnostics/diagnosticsmanager.h/.cpp`)

Central owner of all diagnostic state.

- **Storage**: `QMap<QString, QList<LspDiagnostic>>` keyed by document URI.
- **Version gating**: Each document has a monotonic version counter.  Results
  from an older version are silently dropped to prevent stale squiggles.
- **Aggregate counts**: Maintained incrementally; exposed via
  `countsChanged(errors, warnings, infos)`.

### DiagnosticUtils (`App/diagnostics/diagnosticutils.h`)

Pure-function utilities shared across the pipeline:

| Function | Purpose |
|---|---|
| `filePathToUri` | Convert local path → `file://` URI (percent-encoded) |
| `uriToFilePath` | Convert `file://` URI → local path |
| `clampRange` | Ensure diagnostic range does not exceed document bounds |

### LanguageFeatureManager (`App/language/languagefeaturemanager.h/.cpp`)

Session manager that owns `LspClient` instances for each language and project.

- Resolves the effective language id for a file (override → extension-based).
- Looks up server command + arguments from a built-in config table.
- Initializes each client with its project root, working directory, environment,
  and server configuration. Python sessions also use the selected interpreter.
- Reuses the originating project session for library definitions, including
  files outside the project and server-provided virtual sources.
- Forwards `diagnosticsReceived` from each client to `DiagnosticsManager`.
- Keeps hover and navigation available when diagnostic display is disabled.

### Feature Flags (Settings)

| Key | Type | Default | Description |
|---|---|---|---|
| `diagnostics.enabled` | bool | `true` | Enable diagnostic display |
| `diagnostics.onType` | bool | `true` | Fire `didChange` while typing |
| `diagnostics.onSave` | bool | `true` | Fire `didSave` on save |
| `diagnostics.debounceMs` | int | `200` | Debounce interval for on-type |

## Supported Languages

| Language | Server | Command |
|---|---|---|
| C / C++ | clangd | `clangd` |
| Python  | Basedpyright (pylsp fallback) | `basedpyright-langserver --stdio` |
| Rust | rust-analyzer | `rust-analyzer` |
| Go | gopls | `gopls serve` |
| JavaScript / TypeScript | TypeScript language server | `typescript-language-server --stdio` |
| Java | Eclipse JDT LS | `jdtls` |
| Dockerfile | Docker language server | `docker-langserver --stdio` |

## Stale-Result Protection

Each open document tracks a monotonic `version` counter incremented on every
text change.  When `DiagnosticsManager::upsertDiagnostics` is called the
supplied version is compared against the stored version.  If the supplied
version is older the update is discarded.  This prevents a slow language server
response from overwriting newer results.

## Diagnostic Origin Tagging

Every diagnostic batch carries a `sourceId` string (e.g. `"lsp:clangd"`,
`"cli:ruff"`).  This enables:

- Clearing all diagnostics from a single source without affecting others.
- Future deduplication when both LSP and CLI analyzers are active.
