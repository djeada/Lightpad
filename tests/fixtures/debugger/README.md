# Debugger integration fixtures

These scenes run through Lightpad's real DAP client and value inspector, rather
than a simulated debugger. Both finish graphics work before the inspection line
breakpoint, then mutate data so stepping and stale snapshots can be checked.

`vtk_scene.py` creates and renders a VTK sphere offscreen, attaches a three
component velocity array, and exposes the native data through NumPy. The test
checks wrapped VTK objects, pipeline expressions, point tuples, NumPy table
cells, paging through all 226 points, and a zero-copy mutation after stepping.

`opengl_scene.cpp` creates a Qt OpenGL 3.3 context, compiles shaders, uploads a
vertex buffer and texture, and draws a triangle into an offscreen framebuffer.
The GDB test checks the graphics error, live resource IDs, a 4×4 transform table,
nested vertex/position arrays, stepping, and successful exit. It launches
through the production GDB adapter, including inherited graphics environment
variables and a `QT_QPA_PLATFORM=xcb` override.

Build and run from the repository root:

```bash
cmake --build build --target test_dap fakedap_adapter -j6
QT_QPA_PLATFORM=offscreen \
LIGHTPAD_VTK_PYTHON=/path/to/vtk-venv/bin/python \
build/tests/bin/test_dap \
  testVtkPythonObjectsAndArrays \
  testOpenGLCppObjectsAndMatrices \
  testGdbLaunchEnvironmentPreservesGraphicsContext
```

The VTK interpreter needs `vtk`, `numpy`, and `debugpy`. The C++ test needs a
compiler, GDB with DAP support, Qt6 GUI/OpenGL development libraries, and an X
display. Use `xvfb-run -a` for a headless system with software OpenGL. Missing
dependencies cause explicit skips; compile errors, unexpected debugger stops,
incorrect data, and graphics errors fail the tests.

Set `LIGHTPAD_VTK_SCREENSHOT=/path/to/vtk.png` or
`LIGHTPAD_INSPECTOR_SCREENSHOT=/path/to/matrix.png` to capture the live inspector
in the VTK test or `testInspectorWithRealDebugpy`, respectively.

VTK's [NumPy bridge documentation](https://docs.vtk.org/en/latest/api/python/vtkmodules/vtkmodules.util.numpy_support.html)
describes the shared native storage exercised by the stepping test.

## C and C++ collections

`native_collections.c` and `native_collections.cpp` exercise fixed arrays,
multidimensional arrays, malloc buffers, structs and linked nodes, vectors,
nested vectors, vectors of strings, empty vectors, `std::string`, character
buffers with embedded NUL bytes, and arrays of C strings.

Double-click a variable or watch to open the inspector. Fixed arrays and vectors
have indexed tables; strings have text and character/code views. The Display
selector switches between automatic, decimal, hexadecimal, binary and character
representations. For a raw pointer, set Elements and choose View array. Element
type can reinterpret a buffer, and Columns arranges contiguous elements into
rows. Pointer lengths are explicit; unloaded memory is read only while paused.

Pointer-to-pointer values preserve their outer dimension. When an in-scope
length such as `count` is available, the Variables and Watches trees expose all
rows, and the inspector binds its Length expression to that symbol. Otherwise,
set Elements or enter a length expression. Pointer rows follow each row pointer;
String characters produces a ragged grid without reading beyond shorter
strings. Contiguous rows reshapes a single buffer.

```bash
QT_QPA_PLATFORM=offscreen build/tests/bin/test_dap \
  testNativeScalarFormats testNativeCollections testNativeCCollections \
  testNativeVariablesRemainStructured testNativeBufferPagingAndSnapshot \
  testNativeBufferReportsErrors
```

`string_sort.c` reproduces the interactive string-sorting program with input
`4`, `wkue`, `qoi`, `sbv`, `fekls`. Its regression checks all four allocated
strings in the Variables and Watches trees and in the inspector, including the
character grid, in both `main` and `string_sort`, before and after sorting. It
also verifies the full program output after continuing.

```bash
QT_QPA_PLATFORM=offscreen build/tests/bin/test_dap \
  testStringSortPointerArray testPointerRowsMatrix
```

Set `LIGHTPAD_STRING_SORT_SCREENSHOT=/path/to/sort` to capture the string-list
and character-grid views for each breakpoint case.

Live inspection regressions keep the same inspector open while stepping through
the insertion sort, following `strings` into comparator calls and retaining its
length, layout, display format, filter, and selected view. Returning from the
original function leaves an explicitly labeled out-of-scope snapshot. Native
collection tests also refresh allocated buffers, vectors and strings, retaining
previously loaded pages. Console regressions verify function return values and
errors from breakpoints inside calls without retrying interrupted expressions.

```bash
QT_QPA_PLATFORM=offscreen build/tests/bin/test_dap \
  testStringSortInspectorFollowsSteps testStringSortInspectorFollowsCallerFrame \
  testStringSortConsoleFunctionCall testNativeInspectorLiveCollections \
  testDapClientKeepsInteractiveEvaluationDuringFunctionCall
```

Set `LIGHTPAD_LIVE_INSPECTOR_SCREENSHOT=/path/to/live` to capture the updated
string and character-grid views after stepping.

These tests compile the fixtures and drive the production widgets through real
GDB DAP sessions. Set `LIGHTPAD_NATIVE_SCREENSHOT=/path/to/native` to capture the
collection views, one PNG per data case.

## Concurrency fixtures

The concurrency tests exercise real debugpy and GDB DAP sessions:

- `concurrency_threads.py`: named Python workers with distinct locals;
  switching the toolbar thread changes both inspection and execution context.
- `concurrency_threads.cpp`: native threads waiting on a condition variable;
  inspect each worker's user frame and nested state through GDB.
- `concurrency_processes.py`: two workers using both `spawn` and `fork` on Unix.
  Each receives an automatically attached session and breakpoint. The test
  inspects worker-specific locals and PIDs, pauses the supervising process,
  removes a breakpoint across focused/background workers, resumes the group,
  and checks successful completion and retained output.
- `concurrency_async.py`: named asyncio tasks waiting on an event, followed by
  cancellation and completion. The test opens the task table, navigates through
  Frames to a suspended task's nested locals, and checks snapshot invalidation.

```bash
QT_QPA_PLATFORM=offscreen build/tests/bin/test_dap \
  testPartialThreadExecution testSelectedThreadExecutionArguments \
  testPythonThreadSelection testCppThreadSelection \
  testPythonChildProcesses testAsyncioTaskInspection \
  testProcessScopedDataBreakpoints
```

Python tests need `debugpy` in `python3` on PATH; native tests need a C++17
compiler and GDB with DAP support (Linux for the named-thread fixture).
Set `LIGHTPAD_CONCURRENCY_SCREENSHOT=/path/to/tasks.png` to capture the live
async task table. `LIGHTPAD_DAP_TRACE=1` enables protocol lifecycle logging.
`LIGHTPAD_PROCESS_SCREENSHOT=/path/to/processes.png` captures the full debugger
panel with the process hierarchy open.
The Python workers periodically return from timed waits so debugpy can suspend
them; a Python thread blocked indefinitely inside native code may expose a
stack without inspectable locals until it returns to Python.
