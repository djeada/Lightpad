# Test Explorer

Lightpad includes a built-in **Test Explorer** that can discover, run, and display test results for projects written in multiple languages. Instead of switching to a terminal and re-typing test commands, you can manage the full test lifecycle from a dockable panel inside the editor.

## Supported Frameworks

| Language   | Framework / Tool        | Discovery Adapter          | Output Parser     |
|------------|-------------------------|----------------------------|-------------------|
| Python     | pytest                  | `PytestDiscoveryAdapter`   | `PytestParser`    |
| C / C++    | CTest (CMake)           | `CTestDiscoveryAdapter`    | `CtestParser`     |
| C / C++    | GoogleTest (via CTest)  | `CTestDiscoveryAdapter`    | `CtestParser`     |
| Go         | `go test`               | `GoTestDiscoveryAdapter`   | `JsonTestParser`  |
| Rust       | `cargo test`            | `CargoTestDiscoveryAdapter`| `JsonTestParser`  |
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

## Running Tests

The toolbar provides several ways to run tests:

| Action         | Description                                           |
|----------------|-------------------------------------------------------|
| **Run All**    | Runs every test using the selected configuration.     |
| **Run Failed** | Re-runs only the tests that failed in the last run.   |
| **Stop**       | Kills the running test process.                       |
| **Clear**      | Removes all results from the tree.                    |

You can also right-click a test or suite in the tree for contextual actions:

- **Run This Test** — runs a single test by name.
- **Run Suite** — runs all tests in the selected suite.
- **Go to Source** — opens the source file at the relevant line.
- **Copy Name** — copies the test name to the clipboard.
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

Click or double-click a test in the tree to view its details in the **output pane** below the tree. For failed tests this includes:

- Error message
- Stack trace (when provided by the framework)
- Captured stdout / stderr

Double-clicking a test with a known source location opens the corresponding file and scrolls to the relevant line.

## Test Configurations

Lightpad ships with built-in templates stored in `test_templates.json`:

```jsonc
{
  "id": "pytest",
  "name": "pytest",
  "command": "python3",
  "args": ["-m", "pytest", "--tb=short", "-v", "${file}"],
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
