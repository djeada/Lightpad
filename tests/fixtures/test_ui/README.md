# Tests-panel integration fixtures

These projects deliberately contain passing, failing, and skipped tests. The
Qt UI test copies each project to a temporary directory before building/running
it; it never edits these source fixtures.

The checks drive the real Tests panel through discovery, execution, clicking
definitions, failure details, multi-selection, and failed reruns. They verify
stable row identities and font scaling at 17 and 24 points. Screenshots are
written to `/tmp/lightpad-tests-ui-{cpp,c,rust,node}-24pt.png`.

Build `test_testpanel`, then run:

```sh
QT_QPA_PLATFORM=offscreen \
LIGHTPAD_TEST_UI_LANGUAGES=1 \
LIGHTPAD_TEST_GTEST_SOURCE=/path/to/googletest \
LIGHTPAD_TEST_JEST_NODE_MODULES=/path/to/node_modules \
./build/tests/test_testpanel testNativeLanguageWorkflows
```

Requirements: C/C++ compilers, CMake/CTest, Rust/Cargo, Node.js/npm, and Jest.
GoogleTest can be installed as a CMake package instead of specifying its source
directory. Install the Node fixture's dependencies in a disposable directory,
then point `LIGHTPAD_TEST_JEST_NODE_MODULES` to that directory's `node_modules`.
The checks never download dependencies themselves. Without the opt-in variable,
normal unit runs skip these four integration rows.

The Rust fixture includes duplicate function names in different modules. The
Jest fixture includes a nested suite. Selected runs must use their complete
identities rather than ambiguous short names.
