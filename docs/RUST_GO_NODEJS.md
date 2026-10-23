# Rust, Go, and Node.js

Lightpad combines syntax highlighting, run/format templates, the Tests panel,
and language servers for these toolchains. Install the tools you want to use
and make their executables available on `PATH` before starting Lightpad.

## Setup

| Toolchain | Tools |
|-----------|-------|
| Rust | Rust and Cargo; `rustup component add rust-analyzer rustfmt clippy` |
| Go | Go and gofmt; `go install golang.org/x/tools/gopls@latest` |
| Node.js | Node.js and npm; `npm install -g typescript typescript-language-server` |

Go-installed tools normally live in `$GOPATH/bin` or `$HOME/go/bin`; include
that directory on `PATH`. Install Prettier or ESLint in the npm package to use
the corresponding formatter templates. See [Language servers](LANGUAGE_SERVERS.md)
for server settings, status, and troubleshooting.

## Running a project

Without a file-specific run assignment, Run selects a command from the file's
language and its nearest project:

| File context | Default command | Working directory |
|--------------|-----------------|-------------------|
| Rust file under `Cargo.toml` | `cargo run` | Nearest Cargo package |
| Standalone Rust file | Compile with `rustc`, then run | File directory |
| Go file under `go.mod` or `go.work` | `go run .` | File's package directory, including sibling source files |
| Go `_test.go` file in a module/workspace | `go test -v ./...` | File's package directory |
| Standalone Go file | `go run <file>` | File directory |
| JavaScript/TypeScript under a `package.json` with a nonempty `scripts.start` | `npm start` | Nearest npm package |
| JavaScript without a start script | `node <file>` | File directory |

Project detection stops at the open workspace or a Git repository boundary.
Nested npm packages and Cargo crates use the nearest manifest. An invalid
`package.json` or a missing start script falls back to the file's usual runner.
Explicit run assignments, arguments, environment variables, and working
directory overrides take priority.

A Go library package cannot be launched with `go run .`; choose a Go Build,
Go Vet, or Go Test template instead. For a Cargo library, choose Cargo Test or
Cargo Check. For packages with multiple Rust binaries, assign the Cargo run
template and add `--bin <name>` as custom arguments.

The run template selector also includes:

- Rust: Cargo Run, Test, Check, Build, and Clippy.
- Go: single-file Run, package Run, Build Module, Vet, and Test.
- Node.js: direct Run, npm start/build/test, and the built-in Node.js test runner.
- TypeScript: tsx and ts-node runners, including `.mts` and `.cts` files.

For example, a saved `.lightpad/run_config.json` can select a Rust binary:

```json
{
  "version": "1.0",
  "assignments": [
    {
      "file": "src/main.rs",
      "template": "rust_cargo_run",
      "customArgs": ["--bin", "server"]
    }
  ]
}
```

Project-based run templates declare `projectMarkers` and use `${projectRoot}`
in their `workingDirectory`. The marker search falls back to the file directory
when no project is found. `${workspaceFolder}` remains the open workspace,
which may contain several packages.

## Language features and file types

Rust uses rust-analyzer; Go uses gopls. JavaScript and TypeScript use
typescript-language-server with `--stdio` by default. Completion, hover,
definitions, references, rename, and diagnostics use the existing LSP features.

JavaScript recognizes `.js`, `.jsx`, `.mjs`, and `.cjs`. TypeScript recognizes
`.ts`, `.tsx`, `.mts`, and `.cts`. React files send the standard
`javascriptreact`/`typescriptreact` language IDs to the language server.
Prettier and ESLint formatter templates include the module extensions.

## Testing

Choose **Cargo Test** in the Tests panel for Rust tests. It uses stable Rust's
pretty output, reports passed, failed, and ignored tests, and attaches panic
output to failed results. Single-test reruns use the full name with `--exact`.
Existing custom nightly configurations can still use the `cargo_json` parser.

Go's Tests panel template uses `go test -v -json ./...`. For Node.js, use npm
test to run the package's chosen framework, or the **Node.js Test** run template
to run the current file using `node --test`. JavaScript framework configurations
such as Jest remain available in the Tests panel.

## References

- [Cargo commands](https://doc.rust-lang.org/cargo/commands/index.html)
- [Rust test harness options](https://doc.rust-lang.org/rustc/tests/index.html)
- [Go commands](https://pkg.go.dev/cmd/go)
- [TypeScript language server](https://github.com/typescript-language-server/typescript-language-server)
- [Node.js test runner](https://nodejs.org/api/test.html)
