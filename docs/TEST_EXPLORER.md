# Test Explorer

Lightpad includes a built-in **Test Explorer** that can discover, run, and display test results for projects written in multiple languages. Instead of switching to a terminal and re-typing test commands, you can manage the full test lifecycle from a dockable panel inside the editor.

## Supported Frameworks

| Language   | Framework / Tool        | Discovery Adapter          | Output Parser     |
|------------|-------------------------|----------------------------|-------------------|
| Python     | pytest                  | `PytestDiscoveryAdapter`   | `PytestParser`    |
| C / C++    | CTest (CMake)           | `CTestDiscoveryAdapter`    | `CtestParser`     |
| C / C++    | GoogleTest (via CTest)  | `CTestDiscoveryAdapter`    | `CtestParser`     |
| Go         | `go test`               | `GoTestDiscoveryAdapter`   | `JsonTestParser`  |
| Rust       | `cargo test` (stable)   | `CargoTestDiscoveryAdapter`| `CargoTestParser` |
| JavaScript | Jest                    | `JestDiscoveryAdapter`     | `JsonTestParser`  |

Additional output formats are available through the generic parsers (TAP, JUnit XML, generic regex).

## Opening the Test Explorer

Toggle the panel from the menu:

**View → Toggle Test Panel**

The panel docks at the bottom of the window by default and can also be placed on the left or right side.

## Discovering Tests

1. Select a test configuration from the **configuration dropdown** in the toolbar. When you change the configuration, the matching discovery adapter is set automatically.
2. Click the **Discover** button (magnifying-glass icon) to list the tests found in the workspace.
3. The tree view populates with suites and individual test cases.

The adapter runs the framework-specific list command in the background (for example `pytest --collect-only -q`, `ctest --show-only=json-v1`, `go test -list .* ./...`, or `cargo test -- --list`) and parses the output into a structured tree.

For C projects, select **CTest (C)**. CMake projects should have a configured
`build/` directory before discovery; Run All also configures and builds it.
CTest registration locations, C function definitions, GoogleTest macros, and
Rust module-qualified functions support source navigation. Ambiguous source
matches are left unresolved.

Jest discovery uses the project's installed Jest and its source parser to list
static `describe`/`test`/`it` declarations without running their bodies. Dynamic
declarations remain available through their file group; running that file adds
the exact generated cases and locations from Jest. No package is downloaded by
Discover or Run.

## Running Tests

The toolbar provides several ways to run tests:

| Action         | Description                                           |
|----------------|-------------------------------------------------------|
| **Run All**    | Runs every test using the selected configuration.     |
| **Run Selected** | Runs the selected cases or groups; Ctrl+Enter also runs the selection. |
| **Run Failed** | Re-runs failed cases currently shown in the explorer. |
| **Stop**       | Cancels execution or discovery.                       |
| **Clear Results** | Clears outcomes while retaining the discovered list. |

You can also right-click a test or suite in the tree for contextual actions:

- **Run This Test** — runs the selected test using its framework selector.
- **Run Suite** — runs all tests in the selected suite.
- **Go to Source** — opens the source file at the relevant line.
- **Copy Name** — copies the test name to the clipboard.
- **Copy Test ID** — copies its full identity.
- **Show Output** — displays the full output for the selected test.

### Running Tests for a File

When you trigger a test run from the file tree context menu or from a keyboard shortcut, Lightpad will:

1. Look up configurations that match the file extension.
2. Switch the configuration dropdown to that framework.
3. Run the tests scoped to the file.

## Filtering Results

Use the **filter dropdown** in the toolbar to show only a subset of results:

- **All** — show every test.
- **Failed** — show only failed and errored tests.
- **Passed** — show only passing tests.
- **Skipped** — show only skipped tests.

## Viewing Failure Details

Click a test, or select it and press **Enter**, to open its source definition.
Ctrl/Shift-click selects multiple cases. The **Run Details** area has separate
**Selected test** and **Run output** tabs, placed beside the list in wide panels
and below it in narrow panels. Selected-test details include:

- Error message
- Stack trace (when provided by the framework)
- Captured stdout / stderr

Definition and recognized traceback links open source locations. Run output
shows the command, working directory, exit status, and live output separately.
Reruns preserve the catalog, selection, and other outcomes. Details identify
outcomes from earlier runs.

Names wrap, row heights and controls follow the editor font, and **Time** is
hidden until timing is reported. Stable Cargo output has no per-case timing.
Sort by clicking a heading, resize by dragging its boundary, and choose columns
from the heading's context menu. **Options → Fit Columns** restores automatic
sizing. Manual column sizes are saved. The dock's **Expand** control gives the
panel more room.

Repeatable C/C++/Rust/Node.js UI checks and their dependencies are documented in
[the integration fixtures](../tests/fixtures/test_ui/README.md).

## Test Configurations

Lightpad ships with built-in templates stored in `test_templates.json`:

```jsonc
{
  "id": "pytest",
  "name": "pytest",
  "command": "${python}",
  "args": ["-m", "pytest", "--tb=short", "-v"],
  "outputFormat": "pytest",
  ...
}
```

### Variable Substitution

Configuration arguments support the following variables:

| Variable                | Description                              |
|-------------------------|------------------------------------------|
| `${file}`               | Absolute path to the current file.       |
| `${fileDir}`            | Directory containing the current file.   |
| `${fileBasename}`       | File name with extension.                |
| `${fileBasenameNoExt}`  | File name without extension.             |
| `${fileExt}`            | File extension.                          |
| `${workspaceFolder}`    | Root of the current workspace / project. |
| `${testName}`           | Name of the test being run.              |

### Custom Configurations

You can define workspace-specific overrides by creating `.lightpad/test/config.json` inside your project:

```json
{
  "configurations": [
    {
      "name": "My Custom Tests",
      "command": "python3",
      "args": ["-m", "pytest", "-x", "--tb=long", "${file}"],
      "workingDirectory": "${workspaceFolder}",
      "outputFormat": "pytest",
      "extensions": ["py"]
    }
  ],
  "defaultConfiguration": "My Custom Tests"
}
```

Custom configurations appear alongside the built-in templates in the configuration dropdown.

## Architecture

The test explorer is built from four layers:

1. **Test Model** (`testconfiguration.h`) — `TestResult`, `TestStatus`, `TestConfiguration`, and `TestConfigurationManager`.
2. **Discovery Adapters** (`testdiscovery.h`) — `ITestDiscoveryAdapter` interface with concrete adapters per framework plus `TestDiscoveryAdapterFactory` for automatic adapter selection.
3. **Output Parsers** (`testoutputparser.h`) — `ITestOutputParser` interface with parsers for TAP, JUnit XML, JSON (Go / Jest / Cargo), pytest, CTest, and a configurable generic regex parser.
4. **Run Manager & UI** (`testrunmanager.h`, `testpanel.h`) — `TestRunManager` orchestrates process execution and parser wiring; `TestPanel` provides the Qt widget with a toolbar, tree view, detail pane, and status bar.

When the user changes the selected configuration in the dropdown, `TestPanel` uses `TestDiscoveryAdapterFactory::createForConfiguration()` to create the matching discovery adapter automatically, so the **Discover** button always works with the right tool.
