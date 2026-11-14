# Python backend projects

Open the **project root** in Lightpad. Common FastAPI and Flask applications,
including a `src/` layout and Flask `create_app` factories, are detected from
source without importing the application. Multiple applications require an
explicit entry in **Backend → Configure Backend…**.

The repository named `Flask-Blog` currently contains a FastAPI application in
`src/main.py`. Its detected application is `main:app`, its Python import root is
`src/`, and its default development URL is `http://127.0.0.1:8000`.

## First run

1. Open **Backend → Python Environment and Dependencies…** to select or create
   `.venv`. Development requirements are preferred when both
   `requirements-dev.txt` and `requirements.txt` exist.
2. Use **Install Development Dependencies** and **Install Python Debugger** as
   needed. The debugger is `debugpy`, installed into the selected environment.
3. Use **Run Backend Server**, or Run while editing application Python, HTML or
   CSS files. A saved, explicit file run-template assignment takes precedence
   over automatic backend detection. Scripts and tests keep separate targets.
4. Open **Website**, **API Documentation**, or **Health Endpoint** from Backend.
   API documentation and health paths default to `/docs` and `/health`; these
   endpoints must exist in the application. Flask applications can configure
   different paths.

The server gets its own terminal. Repeated Run requests focus the running
server. **Stop Backend Server** stops it; other backend commands get separate
tabs. Restart the server after changing its launch settings or environment.
Server output and failures remain visible in its terminal.

## Working directory, environment and databases

The default working directory is the **project root**. The import directory is
added to `PYTHONPATH`, so `src/` applications can run from the root without moving
their source files. This also makes repository-root `.env` files and relative
SQLite URLs agree between Run and Debug.

For `Flask-Blog`, `sqlite:///./blog_engine.db` therefore resolves to
`<project>/blog_engine.db`. Its existing Makefile runs from `src/`, which resolves
the same URL to `<project>/src/blog_engine.db`. Those can contain different data.
To deliberately follow the Makefile, set `workingDirectory` to `src` in backend
settings. Choose the desired database explicitly if both already exist.

**Backend → Open Project Database…**, also available under Database, discovers
existing SQLite files from configured URLs, literal SQLite URLs in Python
source, and the root, source, `instance/`, `data/` and `db/` directories. It shows
relative paths when several files exist. Discovery checks file headers, removes
duplicates and does not create an empty database. Databases with other filenames
or locations can be listed in `databaseFiles`. The existing database workbench
provides schema browsing, queries, editable table grids, exports and SQL
completion. Refresh its schema after application startup or a migration.

Environment precedence is `.env`, then inherited process variables, then
`backend.json`'s `env`, then explicit run-assignment overrides for a server run.
Dotenv supports `export`, comments, quoted and multiline values, and `${NAME}`
references to earlier values or inherited variables. Values are never executed
as shell code. Secrets remain in the environment rather than launch arguments.
Tests use their own test-configuration environment and the application's import
path; backend `.env` variables are not injected into pytest.

## Running and debugging tests

**Backend → Run Backend Tests** runs pytest in the Tests panel using the workspace
Python environment. **Discover Backend Tests** collects with that interpreter as
well. Run on a `test_*.py` or `*_test.py` file opens its results in the Tests panel.
The panel groups tests by file. Click a test, or select it and press **Enter**,
to open its definition at the exact source line, including class methods and
parameterized tests. **Ctrl+Enter** or **Run Selected** executes the selection;
Ctrl/Shift-click selects several tests or file groups. **Run Failed** reruns
the exact failing node IDs. Reruns retain the test list, selection, and other
results; selected-test details identify results from an earlier run.

Long names wrap, and the **Time** column appears when timing data is available.
Click a heading to sort, drag its boundary to resize, or right-click a heading
to choose columns. **Options → Fit Columns** restores the automatic layout.
Column widths, row heights, output, and controls follow the editor font size.
Use the dock's **Expand** control for more room.

**Run Details** has separate **Selected test** and **Run output** tabs. The first
shows the complete test ID, definition link, failure traceback, and captured
stdout/stderr. Traceback file locations are clickable. The second shows the
command, working directory, exit status, and live output. Discovery errors open
their full output, and **Stop** cancels discovery as well as execution.
**Clear Results** keeps the discovered test list.

Regular pytest runs load Lightpad's bundled reporter into the existing Python
environment; no additional package or project file is installed. Other test
runners keep their existing output parsers. Runners without source or timing
metadata may have fewer details available.

Set a breakpoint in a test, fixture, endpoint, repository function, or other
application code. Then:

- Use **Backend → Debug Backend Tests** for the suite.
- Use **Debug** in the Tests toolbar for selected tests, or all tests when none
  are selected.
- Right-click a test or suite and choose **Debug This Test** or **Debug Suite**.
- Debug an open `test_*.py` or `*_test.py` file to run that file under pytest.

Selections preserve complete pytest node IDs, including classes and parameters.
For example, `tests/test_api.py::TestEndpoints::test_post[author role]` is passed
as one argument. Running a discovered pytest test or suite also preserves these
IDs rather than relying on the displayed short name.

Debug runs launch **the pytest module**, not the test file as a script. A
`TestClient` request executes the application in the debugged process, so a
breakpoint inside its endpoint can inspect request parameters and locals. Use
the Debug panel to step, evaluate expressions and continue. Pytest debug output
appears in the debug console; regular test runs report results in the Tests
panel. Capture and inherited pytest `addopts` are disabled for debug runs so
output and tracing are available. Explicit arguments in the selected test
configuration remain in use.

**Debug Backend Server** instead launches Uvicorn or Flask without the reload
process; Flask's built-in debugger is also disabled. Send a request using the
browser, the API documentation, or an HTTP client to hit endpoint breakpoints.

## Project settings and tasks

`.lightpad/backend.json` is optional. Paths are relative to the project root,
except `databaseFiles`, which are relative to `workingDirectory`.

```json
{
  "framework": "fastapi",
  "application": "main:app",
  "entryFile": "src/main.py",
  "sourceDirectory": "src",
  "workingDirectory": ".",
  "envFile": ".env",
  "host": "127.0.0.1",
  "port": 8000,
  "docsPath": "/docs",
  "healthPath": "/health",
  "databaseFiles": ["blog_engine.db"],
  "env": {},
  "tasks": [
    {
      "name": "Check project",
      "command": "make",
      "args": ["check"],
      "workingDirectory": "."
    },
    {
      "name": "Apply Alembic migrations",
      "command": "${python}",
      "args": ["-m", "alembic", "upgrade", "head"],
      "workingDirectory": "."
    }
  ]
}
```

Only add the Alembic task to projects that actually use Alembic; `Flask-Blog`
initializes its schema at startup. Tasks run when selected and do not run during
discovery. Arguments are passed directly to the process. The `${python}` command
uses the selected workspace interpreter. Configure `framework: "flask"` and
`application: "blog:create_app"` for a Flask factory. Set `enabled: false` to
disable automatic backend handling.

The entry file's saved Python environment preference is shared by server,
debugger, test execution, discovery and backend commands. Project settings can
override it with `pythonMode`, `pythonInterpreter`, `pythonVenvPath` and
`pythonRequirementsFile`. Existing saved launch and test configurations remain
available through their configuration dialogs.

## Workflow audit

| Need | Support |
| --- | --- |
| Source editing, navigation, diagnostics, Git | Existing editor and language tooling |
| Workspace interpreter and development dependencies | Existing environment UI, shared backend preference and development-requirement selection |
| Run a framework application from root or `src/` | Backend detection and module launch |
| Application configuration | Shared dotenv/import path for Run and server Debug |
| SQLite browsing and SQL | Project discovery plus existing database workbench |
| PostgreSQL, MySQL and SQL Server | Existing explicit connections and Docker discovery |
| Tests, discovery, single-test and suite selection | Workspace pytest with complete node IDs |
| Debug tests, fixtures and endpoint requests | Pytest module launches and normal DAP breakpoints |
| Debug a live web server | Framework module launch without reloader |
| Lint and project-specific checks/migrations | Ruff action and explicit project tasks |
| Container startup and shutdown | Compose actions and dedicated terminals |
| API exploration | Browser links to application-provided OpenAPI documentation |

Remaining limitations: Lightpad does not yet have an embedded HTTP request
collection/client, an OpenAPI route explorer, an ORM migration designer, or
automatic database introspection through arbitrary Python settings code.
Remote databases still need connection profiles. SQLite files inside Docker
named volumes require a host mount or an exported snapshot before the local
SQLite workbench can open them. Unusual imports, application aliases, deeper
source trees and dynamically constructed apps should use explicit backend
settings. Discovery is bounded and intentionally does not import Python code.

## Verification

```bash
cmake --build build --target Lightpad test_pythonbackendproject \
  test_runtargetresolver test_dap test_dap_integration test_testpanel \
  test_testrunmanager test_testoutputparsers
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure \
  -R 'PythonBackendProject|RunTargetResolver|DapTests|TestPanel|TestRunManager|TestOutputParsers'
LIGHTPAD_TEST_PYTHON=/path/to/backend/.venv/bin/python \
  QT_QPA_PLATFORM=offscreen build/tests/bin/test_dap_integration \
  pytestEndpointBreakpointInspectsRequest
```

The endpoint integration test needs `debugpy`, `pytest`, `fastapi` and `httpx` in
that interpreter. It runs a parameterized test through the real debugpy adapter,
stops in a FastAPI handler called by `TestClient`, evaluates the request value,
continues and checks a successful exit. It skips when those dependencies are
unavailable.
