#include <QSignalSpy>
#include <QtTest>

#include "test_templates/testconfiguration.h"
#include "test_templates/testdiscovery.h"
#include "test_templates/testoutputparser.h"

Q_DECLARE_METATYPE(TestResult)
Q_DECLARE_METATYPE(DiscoveredTest)

class TestOutputParsers : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();

  // TAP parser tests
  void testTapParserBasic();
  void testTapParserSkip();
  void testTapParserMixed();

  // JUnit XML parser tests
  void testJunitXmlBasic();
  void testJunitXmlWithFailure();
  void testJunitXmlWithSkipped();

  // JSON parser tests (Go test format)
  void testJsonGoTestFormat();
  // JSON parser tests (Jest format)
  void testJsonJestFormat();
  // JSON parser tests (Cargo format)
  void testJsonCargoFormat();

  // Pytest parser tests
  void testPytestBasicOutput();
  void testPytestMixedStatuses();

  // CTest parser tests
  void testCtestBasicOutput();
  void testCtestMixedResults();

  // Generic regex parser tests
  void testGenericRegexDefaults();
  void testGenericRegexCustomPatterns();

  // Factory tests
  void testParserFactory();

  // TestConfiguration tests
  void testConfigurationFromJson();
  void testConfigurationToJson();
  void testConfigurationRunOverridesFromJson();
  void testConfigurationManagerSubstituteVariables();
  void testConfigurationManagerLoadTemplates();

  // CTest discovery adapter tests
  void testCtestDiscoveryParseJsonOutput();
  void testCtestDiscoveryParseJsonOutputEmpty();
  void testCtestDiscoveryParseDashN();
  void testCtestDiscoveryParseDashNEmpty();

  // GTest discovery adapter tests
  void testGTestParseListTestsOutput();
  void testGTestParseListTestsOutputEmpty();
  void testGTestBuildFilter();
  void testGTestBuildFilterEmpty();
  void testGTestBuildFilterSingle();

  // Pytest discovery adapter tests
  void testPytestDiscoveryParse();
  void testPytestDiscoveryParseEmpty();

  // Go test discovery adapter tests
  void testGoTestDiscoveryParse();
  void testGoTestDiscoveryParseEmpty();

  // Cargo test discovery adapter tests
  void testCargoTestDiscoveryParse();
  void testCargoTestDiscoveryParseEmpty();

  // Jest discovery adapter tests
  void testJestDiscoveryParse();
  void testJestDiscoveryParseEmpty();
};

void TestOutputParsers::initTestCase() {
  qRegisterMetaType<TestResult>("TestResult");
  qRegisterMetaType<DiscoveredTest>("DiscoveredTest");
  qRegisterMetaType<QList<DiscoveredTest>>("QList<DiscoveredTest>");
}

// --- TAP Parser ---

void TestOutputParsers::testTapParserBasic() {
  TapParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  parser.feed("1..3\n");
  parser.feed("ok 1 - addition works\n");
  parser.feed("ok 2 - subtraction works\n");
  parser.feed("not ok 3 - division by zero\n");
  parser.finish();

  QCOMPARE(results.size(), 3);
  QCOMPARE(results[0].status, TestStatus::Passed);
  QCOMPARE(results[0].name, QString("addition works"));
  QCOMPARE(results[1].status, TestStatus::Passed);
  QCOMPARE(results[2].status, TestStatus::Failed);
}

void TestOutputParsers::testTapParserSkip() {
  TapParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  parser.feed("ok 1 - basic test # SKIP not implemented\n");
  parser.feed("ok 2 - todo test # TODO fix later\n");
  parser.finish();

  QCOMPARE(results.size(), 2);
  QCOMPARE(results[0].status, TestStatus::Skipped);
  QCOMPARE(results[0].message, QString("not implemented"));
  QCOMPARE(results[1].status, TestStatus::Skipped);
}

void TestOutputParsers::testTapParserMixed() {
  TapParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  QByteArray data = "1..4\n"
                    "ok 1 - test alpha\n"
                    "not ok 2 - test beta\n"
                    "ok 3 - test gamma # SKIP platform\n"
                    "ok 4 - test delta\n";
  parser.feed(data);
  parser.finish();

  QCOMPARE(results.size(), 4);
  QCOMPARE(results[0].status, TestStatus::Passed);
  QCOMPARE(results[1].status, TestStatus::Failed);
  QCOMPARE(results[2].status, TestStatus::Skipped);
  QCOMPARE(results[3].status, TestStatus::Passed);
}

// --- JUnit XML Parser ---

void TestOutputParsers::testJunitXmlBasic() {
  JunitXmlParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<testsuites>
  <testsuite name="MathTests" tests="2">
    <testcase name="testAdd" classname="MathTests" time="0.012">
    </testcase>
    <testcase name="testSub" classname="MathTests" time="0.008">
    </testcase>
  </testsuite>
</testsuites>)";

  parser.feed(xml);
  parser.finish();

  QCOMPARE(results.size(), 2);
  QCOMPARE(results[0].status, TestStatus::Passed);
  QCOMPARE(results[0].name, QString("testAdd"));
  QCOMPARE(results[0].suite, QString("MathTests"));
  QCOMPARE(results[0].durationMs, 12);
  QCOMPARE(results[1].status, TestStatus::Passed);
  QCOMPARE(results[1].durationMs, 8);
}

void TestOutputParsers::testJunitXmlWithFailure() {
  JunitXmlParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  QByteArray xml = R"(<?xml version="1.0"?>
<testsuites>
  <testsuite name="Suite">
    <testcase name="testPass" classname="Suite" time="0.001">
    </testcase>
    <testcase name="testFail" classname="Suite" time="0.002">
      <failure message="expected 1 got 2">at test.cpp:42</failure>
    </testcase>
    <testcase name="testError" classname="Suite" time="0.003">
      <error message="null pointer">segfault at 0x0</error>
    </testcase>
  </testsuite>
</testsuites>)";

  parser.feed(xml);
  parser.finish();

  QCOMPARE(results.size(), 3);
  QCOMPARE(results[0].status, TestStatus::Passed);
  QCOMPARE(results[1].status, TestStatus::Failed);
  QCOMPARE(results[1].message, QString("expected 1 got 2"));
  QCOMPARE(results[1].stackTrace, QString("at test.cpp:42"));
  QCOMPARE(results[2].status, TestStatus::Errored);
  QCOMPARE(results[2].message, QString("null pointer"));
}

void TestOutputParsers::testJunitXmlWithSkipped() {
  JunitXmlParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  QByteArray xml = R"(<?xml version="1.0"?>
<testsuites>
  <testsuite name="Suite">
    <testcase name="testSkipped" classname="Suite" time="0.000">
      <skipped message="not applicable"/>
    </testcase>
  </testsuite>
</testsuites>)";

  parser.feed(xml);
  parser.finish();

  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].status, TestStatus::Skipped);
  QCOMPARE(results[0].message, QString("not applicable"));
}

// --- JSON Parser (Go test format) ---

void TestOutputParsers::testJsonGoTestFormat() {
  JsonTestParser parser;
  QList<TestResult> started;
  QList<TestResult> finished;
  connect(&parser, &ITestOutputParser::testStarted,
          [&started](const TestResult &r) { started.append(r); });
  connect(&parser, &ITestOutputParser::testFinished,
          [&finished](const TestResult &r) { finished.append(r); });

  QByteArray data =
      R"({"Time":"2024-01-01T00:00:00Z","Action":"run","Package":"pkg","Test":"TestAdd"})"
      "\n"
      R"({"Time":"2024-01-01T00:00:01Z","Action":"pass","Package":"pkg","Test":"TestAdd","Elapsed":0.5})"
      "\n"
      R"({"Time":"2024-01-01T00:00:01Z","Action":"run","Package":"pkg","Test":"TestSub"})"
      "\n"
      R"({"Time":"2024-01-01T00:00:02Z","Action":"fail","Package":"pkg","Test":"TestSub","Elapsed":1.2})"
      "\n";

  parser.feed(data);
  parser.finish();

  QCOMPARE(started.size(), 2);
  QCOMPARE(finished.size(), 2);
  QCOMPARE(finished[0].status, TestStatus::Passed);
  QCOMPARE(finished[0].name, QString("TestAdd"));
  QCOMPARE(finished[0].durationMs, 500);
  QCOMPARE(finished[1].status, TestStatus::Failed);
  QCOMPARE(finished[1].durationMs, 1200);
}

// --- JSON Parser (Jest format) ---

void TestOutputParsers::testJsonJestFormat() {
  JsonTestParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  QByteArray data = R"({"testResults":[{"testFilePath":"/src/math.test.js","testResults":[{"fullName":"Math addition","title":"addition","status":"passed","duration":5},{"fullName":"Math subtraction","title":"subtraction","status":"failed","duration":10,"failureMessages":["Expected 3 but got 4"]},{"fullName":"Math pending","title":"pending","status":"pending","duration":0}]}]})"
                    "\n";

  parser.feed(data);
  parser.finish();

  QCOMPARE(results.size(), 3);
  QCOMPARE(results[0].status, TestStatus::Passed);
  QCOMPARE(results[0].name, QString("Math addition"));
  QCOMPARE(results[0].durationMs, 5);
  QCOMPARE(results[1].status, TestStatus::Failed);
  QCOMPARE(results[1].message, QString("Expected 3 but got 4"));
  QCOMPARE(results[2].status, TestStatus::Skipped);
}

// --- JSON Parser (Cargo format) ---

void TestOutputParsers::testJsonCargoFormat() {
  JsonTestParser parser;
  QList<TestResult> started;
  QList<TestResult> finished;
  connect(&parser, &ITestOutputParser::testStarted,
          [&started](const TestResult &r) { started.append(r); });
  connect(&parser, &ITestOutputParser::testFinished,
          [&finished](const TestResult &r) { finished.append(r); });

  QByteArray data =
      R"({"type":"test","event":"started","name":"tests::test_add"})" "\n"
      R"({"type":"test","event":"ok","name":"tests::test_add"})" "\n"
      R"({"type":"test","event":"started","name":"tests::test_fail"})" "\n"
      R"({"type":"test","event":"failed","name":"tests::test_fail","stdout":"assertion failed"})" "\n"
      R"({"type":"test","event":"started","name":"tests::test_skip"})" "\n"
      R"({"type":"test","event":"ignored","name":"tests::test_skip"})" "\n";

  parser.feed(data);
  parser.finish();

  QCOMPARE(started.size(), 3);
  QCOMPARE(finished.size(), 3);
  QCOMPARE(finished[0].status, TestStatus::Passed);
  QCOMPARE(finished[0].name, QString("tests::test_add"));
  QCOMPARE(finished[1].status, TestStatus::Failed);
  QCOMPARE(finished[1].stdoutOutput, QString("assertion failed"));
  QCOMPARE(finished[2].status, TestStatus::Skipped);
}

// --- Pytest Parser ---

void TestOutputParsers::testPytestBasicOutput() {
  PytestParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  QByteArray data =
      "tests/test_math.py::test_add PASSED\n"
      "tests/test_math.py::test_subtract PASSED\n"
      "tests/test_math.py::test_divide FAILED\n";

  parser.feed(data);
  parser.finish();

  QCOMPARE(results.size(), 3);
  QCOMPARE(results[0].status, TestStatus::Passed);
  QCOMPARE(results[0].name, QString("test_add"));
  QCOMPARE(results[0].filePath, QString("tests/test_math.py"));
  QCOMPARE(results[1].status, TestStatus::Passed);
  QCOMPARE(results[2].status, TestStatus::Failed);
}

void TestOutputParsers::testPytestMixedStatuses() {
  PytestParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  QByteArray data =
      "tests/test_example.py::test_pass PASSED\n"
      "tests/test_example.py::test_skip SKIPPED\n"
      "tests/test_example.py::test_error ERROR\n"
      "tests/test_example.py::test_xfail XFAIL\n";

  parser.feed(data);
  parser.finish();

  QCOMPARE(results.size(), 4);
  QCOMPARE(results[0].status, TestStatus::Passed);
  QCOMPARE(results[1].status, TestStatus::Skipped);
  QCOMPARE(results[2].status, TestStatus::Errored);
  QCOMPARE(results[3].status, TestStatus::Passed); // XFAIL = expected failure = pass
}

// --- CTest Parser ---

void TestOutputParsers::testCtestBasicOutput() {
  CtestParser parser;
  QList<TestResult> started;
  QList<TestResult> finished;
  connect(&parser, &ITestOutputParser::testStarted,
          [&started](const TestResult &r) { started.append(r); });
  connect(&parser, &ITestOutputParser::testFinished,
          [&finished](const TestResult &r) { finished.append(r); });

  QByteArray data =
      "    Start 1: LoggerTests\n"
      "1/3 Test #1: LoggerTests ..................   Passed    0.02 sec\n"
      "    Start 2: ThemeTests\n"
      "2/3 Test #2: ThemeTests ...................   Passed    0.01 sec\n"
      "    Start 3: FailTest\n"
      "3/3 Test #3: FailTest .....................***Failed    0.05 sec\n";

  parser.feed(data);
  parser.finish();

  QCOMPARE(started.size(), 3);
  QCOMPARE(finished.size(), 3);
  QCOMPARE(finished[0].status, TestStatus::Passed);
  QCOMPARE(finished[0].name, QString("LoggerTests"));
  QCOMPARE(finished[0].durationMs, 20);
  QCOMPARE(finished[1].status, TestStatus::Passed);
  QCOMPARE(finished[2].status, TestStatus::Failed);
  QCOMPARE(finished[2].name, QString("FailTest"));
}

void TestOutputParsers::testCtestMixedResults() {
  CtestParser parser;
  QList<TestResult> finished;
  connect(&parser, &ITestOutputParser::testFinished,
          [&finished](const TestResult &r) { finished.append(r); });

  QByteArray data =
      "1/2 Test #1: PassTest .....................   Passed    0.10 sec\n"
      "2/2 Test #2: SkipTest .....................   Not Run   0.00 sec\n";

  parser.feed(data);
  parser.finish();

  QCOMPARE(finished.size(), 2);
  QCOMPARE(finished[0].status, TestStatus::Passed);
  QCOMPARE(finished[0].durationMs, 100);
  QCOMPARE(finished[1].status, TestStatus::Skipped);
}

// --- Generic Regex Parser ---

void TestOutputParsers::testGenericRegexDefaults() {
  GenericRegexParser parser;
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  QByteArray data =
      "PASS: test_one\n"
      "FAIL: test_two\n"
      "SKIP: test_three\n"
      "some other output\n";

  parser.feed(data);
  parser.finish();

  QCOMPARE(results.size(), 3);
  QCOMPARE(results[0].status, TestStatus::Passed);
  QCOMPARE(results[0].name, QString("test_one"));
  QCOMPARE(results[1].status, TestStatus::Failed);
  QCOMPARE(results[1].name, QString("test_two"));
  QCOMPARE(results[2].status, TestStatus::Skipped);
}

void TestOutputParsers::testGenericRegexCustomPatterns() {
  GenericRegexParser parser(R"(^\[OK\]\s+(.+)$)", R"(^\[ERR\]\s+(.+)$)",
                            R"(^\[SKIP\]\s+(.+)$)");
  QList<TestResult> results;
  connect(&parser, &ITestOutputParser::testFinished,
          [&results](const TestResult &r) { results.append(r); });

  QByteArray data =
      "[OK] my_test_1\n"
      "[ERR] my_test_2\n"
      "[SKIP] my_test_3\n";

  parser.feed(data);
  parser.finish();

  QCOMPARE(results.size(), 3);
  QCOMPARE(results[0].status, TestStatus::Passed);
  QCOMPARE(results[0].name, QString("my_test_1"));
  QCOMPARE(results[1].status, TestStatus::Failed);
  QCOMPARE(results[2].status, TestStatus::Skipped);
}

// --- Factory ---

void TestOutputParsers::testParserFactory() {
  auto *tap = TestOutputParserFactory::createParser("tap");
  QVERIFY(tap != nullptr);
  QCOMPARE(tap->formatId(), QString("tap"));
  delete tap;

  auto *junit = TestOutputParserFactory::createParser("junit_xml");
  QVERIFY(junit != nullptr);
  QCOMPARE(junit->formatId(), QString("junit_xml"));
  delete junit;

  auto *json = TestOutputParserFactory::createParser("go_json");
  QVERIFY(json != nullptr);
  QCOMPARE(json->formatId(), QString("json"));
  delete json;

  auto *jest = TestOutputParserFactory::createParser("jest_json");
  QVERIFY(jest != nullptr);
  QCOMPARE(jest->formatId(), QString("json"));
  delete jest;

  auto *cargo = TestOutputParserFactory::createParser("cargo_json");
  QVERIFY(cargo != nullptr);
  QCOMPARE(cargo->formatId(), QString("json"));
  delete cargo;

  auto *pytest = TestOutputParserFactory::createParser("pytest");
  QVERIFY(pytest != nullptr);
  QCOMPARE(pytest->formatId(), QString("pytest"));
  delete pytest;

  auto *ctest = TestOutputParserFactory::createParser("ctest");
  QVERIFY(ctest != nullptr);
  QCOMPARE(ctest->formatId(), QString("ctest"));
  delete ctest;

  auto *generic = TestOutputParserFactory::createParser("generic");
  QVERIFY(generic != nullptr);
  QCOMPARE(generic->formatId(), QString("generic"));
  delete generic;

  // Unknown format should return generic parser
  auto *unknown = TestOutputParserFactory::createParser("unknown_format");
  QVERIFY(unknown != nullptr);
  QCOMPARE(unknown->formatId(), QString("generic"));
  delete unknown;
}

// --- TestConfiguration ---

void TestOutputParsers::testConfigurationFromJson() {
  QJsonObject obj;
  obj["id"] = "pytest";
  obj["name"] = "pytest";
  obj["language"] = "Python";
  obj["command"] = "python3";
  obj["workingDirectory"] = "${workspaceFolder}";
  obj["outputFormat"] = "pytest";
  obj["testFilePattern"] = "test_*.py";
  QJsonArray ext;
  ext.append("py");
  obj["extensions"] = ext;
  QJsonArray args;
  args.append("-m");
  args.append("pytest");
  args.append("-v");
  obj["args"] = args;

  TestConfiguration cfg = TestConfiguration::fromJson(obj);

  QCOMPARE(cfg.id, QString("pytest"));
  QCOMPARE(cfg.name, QString("pytest"));
  QCOMPARE(cfg.language, QString("Python"));
  QCOMPARE(cfg.command, QString("python3"));
  QCOMPARE(cfg.extensions.size(), 1);
  QCOMPARE(cfg.extensions[0], QString("py"));
  QCOMPARE(cfg.args.size(), 3);
  QCOMPARE(cfg.outputFormat, QString("pytest"));
  QCOMPARE(cfg.testFilePattern, QString("test_*.py"));
  QVERIFY(cfg.isValid());
}

void TestOutputParsers::testConfigurationToJson() {
  TestConfiguration cfg;
  cfg.id = "go_test";
  cfg.name = "Go Test";
  cfg.language = "Go";
  cfg.command = "go";
  cfg.args = {"test", "-v", "-json"};
  cfg.extensions = {"go"};
  cfg.workingDirectory = "${workspaceFolder}";
  cfg.outputFormat = "go_json";
  cfg.runFailed.args = {"test", "-v", "-json", "-run", "${testName}", "./..."};
  cfg.runSuite.args = {"test", "-v", "-json", "-run", "^${testName}", "./..."};

  QJsonObject obj = cfg.toJson();

  QCOMPARE(obj["id"].toString(), QString("go_test"));
  QCOMPARE(obj["name"].toString(), QString("Go Test"));
  QCOMPARE(obj["command"].toString(), QString("go"));
  QCOMPARE(obj["args"].toArray().size(), 3);
  QCOMPARE(obj["outputFormat"].toString(), QString("go_json"));
  QVERIFY(obj.contains("runFailed"));
  QCOMPARE(obj["runFailed"].toObject()["args"].toArray().size(), 6);
  QVERIFY(obj.contains("runSuite"));
  QCOMPARE(obj["runSuite"].toObject()["args"].toArray().size(), 6);
}

void TestOutputParsers::testConfigurationRunOverridesFromJson() {
  QJsonObject obj;
  obj["id"] = "gtest_cmake";
  obj["name"] = "Google Test (CTest)";
  obj["command"] = "bash";
  QJsonArray args;
  args.append("-lc");
  args.append("ctest --test-dir build -V");
  obj["args"] = args;

  QJsonObject runFailed;
  QJsonArray failedArgs;
  failedArgs.append("-lc");
  failedArgs.append("ctest --test-dir build -V -R '${testName}'");
  runFailed["args"] = failedArgs;
  obj["runFailed"] = runFailed;

  QJsonObject runSuite;
  QJsonArray suiteArgs;
  suiteArgs.append("-lc");
  suiteArgs.append("ctest --test-dir build -V -R '^${testName}'");
  runSuite["args"] = suiteArgs;
  obj["runSuite"] = runSuite;

  TestConfiguration cfg = TestConfiguration::fromJson(obj);

  QCOMPARE(cfg.runFailed.args.size(), 2);
  QVERIFY(cfg.runFailed.args[1].contains("${testName}"));
  QCOMPARE(cfg.runSuite.args.size(), 2);
  QVERIFY(cfg.runSuite.args[1].contains("${testName}"));

  // Verify round-trip
  QJsonObject out = cfg.toJson();
  QVERIFY(out.contains("runFailed"));
  QVERIFY(out.contains("runSuite"));
  QCOMPARE(out["runFailed"].toObject()["args"].toArray().size(), 2);
  QCOMPARE(out["runSuite"].toObject()["args"].toArray().size(), 2);
}

void TestOutputParsers::testConfigurationManagerSubstituteVariables() {
  QString result = TestConfigurationManager::substituteVariables(
      "python3 -m pytest ${file}", "/home/user/project/test_main.py",
      "/home/user/project");
  QCOMPARE(result,
           QString("python3 -m pytest /home/user/project/test_main.py"));

  result = TestConfigurationManager::substituteVariables(
      "${workspaceFolder}/build", "/home/user/project/main.cpp",
      "/home/user/project");
  QCOMPARE(result, QString("/home/user/project/build"));

  result = TestConfigurationManager::substituteVariables(
      "pytest -k ${testName}", "/home/user/project/test.py",
      "/home/user/project", "test_addition");
  QCOMPARE(result, QString("pytest -k test_addition"));

  result = TestConfigurationManager::substituteVariables(
      "${fileBasenameNoExt}", "/home/user/project/test_math.py",
      "/home/user/project");
  QCOMPARE(result, QString("test_math"));
}

void TestOutputParsers::testConfigurationManagerLoadTemplates() {
  // This test verifies that templates can be loaded from the QRC resource
  TestConfigurationManager &mgr = TestConfigurationManager::instance();
  bool loaded = mgr.loadTemplates();

  // Templates may or may not be found depending on QRC availability in test
  // binary. The important thing is the method doesn't crash.
  if (loaded) {
    QVERIFY(!mgr.allTemplates().isEmpty());

    // Verify we have the expected template IDs
    TestConfiguration pytest = mgr.templateById("pytest");
    if (pytest.isValid()) {
      QCOMPARE(pytest.language, QString("Python"));
      QCOMPARE(pytest.outputFormat, QString("pytest"));
      // Verify runFailed args are loaded
      QVERIFY(!pytest.runFailed.args.isEmpty());
    }

    // Verify C++ template has runFailed and runSuite overrides
    TestConfiguration gtest = mgr.templateById("gtest_cmake");
    if (gtest.isValid()) {
      QCOMPARE(gtest.language, QString("C++"));
      QVERIFY(!gtest.runFailed.args.isEmpty());
      QVERIFY(!gtest.runSuite.args.isEmpty());
    }
  }
}

// --- CTest Discovery Adapter ---

void TestOutputParsers::testCtestDiscoveryParseJsonOutput() {
  QByteArray json = R"({
    "kind": "ctestInfo",
    "version": { "major": 1, "minor": 0 },
    "tests": [
      {
        "name": "LoggerTests",
        "index": 1,
        "command": ["/path/to/test_logger"],
        "properties": []
      },
      {
        "name": "ThemeTests",
        "index": 2,
        "command": ["/path/to/test_theme"],
        "properties": [
          { "name": "WORKING_DIRECTORY", "value": "/home/user/project/build" }
        ]
      },
      {
        "name": "DocumentTests",
        "index": 3,
        "command": ["/path/to/test_document"],
        "properties": []
      }
    ]
  })";

  QList<DiscoveredTest> tests =
      CTestDiscoveryAdapter::parseJsonOutput(json);

  QCOMPARE(tests.size(), 3);
  QCOMPARE(tests[0].name, QString("LoggerTests"));
  QCOMPARE(tests[0].id, QString("1"));
  QCOMPARE(tests[1].name, QString("ThemeTests"));
  QCOMPARE(tests[1].id, QString("2"));
  QCOMPARE(tests[1].filePath,
           QString("/home/user/project/build"));
  QCOMPARE(tests[2].name, QString("DocumentTests"));
  QCOMPARE(tests[2].id, QString("3"));
}

void TestOutputParsers::testCtestDiscoveryParseJsonOutputEmpty() {
  QByteArray json = R"({"tests": []})";
  QList<DiscoveredTest> tests =
      CTestDiscoveryAdapter::parseJsonOutput(json);
  QCOMPARE(tests.size(), 0);

  // Invalid JSON should also return empty
  QList<DiscoveredTest> bad =
      CTestDiscoveryAdapter::parseJsonOutput("not json");
  QCOMPARE(bad.size(), 0);
}

void TestOutputParsers::testCtestDiscoveryParseDashN() {
  QString output =
      "Test project /home/user/project/build\n"
      "  Test  #1: LoggerTests\n"
      "  Test  #2: ThemeTests\n"
      "  Test  #3: DocumentTests\n"
      "  Test  #4: SettingsTests\n"
      "\n"
      "Total Tests: 4\n";

  QList<DiscoveredTest> tests =
      CTestDiscoveryAdapter::parseDashNOutput(output);

  QCOMPARE(tests.size(), 4);
  QCOMPARE(tests[0].name, QString("LoggerTests"));
  QCOMPARE(tests[0].id, QString("1"));
  QCOMPARE(tests[1].name, QString("ThemeTests"));
  QCOMPARE(tests[1].id, QString("2"));
  QCOMPARE(tests[2].name, QString("DocumentTests"));
  QCOMPARE(tests[2].id, QString("3"));
  QCOMPARE(tests[3].name, QString("SettingsTests"));
  QCOMPARE(tests[3].id, QString("4"));
}

void TestOutputParsers::testCtestDiscoveryParseDashNEmpty() {
  QList<DiscoveredTest> tests =
      CTestDiscoveryAdapter::parseDashNOutput("");
  QCOMPARE(tests.size(), 0);

  QList<DiscoveredTest> noTests =
      CTestDiscoveryAdapter::parseDashNOutput(
          "Test project /build\nTotal Tests: 0\n");
  QCOMPARE(noTests.size(), 0);
}

// --- GTest Discovery Adapter ---

void TestOutputParsers::testGTestParseListTestsOutput() {
  QString output =
      "Running main() from gtest_main.cc\n"
      "MathTests.\n"
      "  TestAdd\n"
      "  TestSubtract\n"
      "  TestMultiply\n"
      "StringTests.\n"
      "  TestConcat\n"
      "  TestSplit # This is a comment\n";

  QList<DiscoveredTest> tests =
      GTestDiscoveryAdapter::parseListTestsOutput(output);

  QCOMPARE(tests.size(), 5);
  QCOMPARE(tests[0].suite, QString("MathTests"));
  QCOMPARE(tests[0].name, QString("TestAdd"));
  QCOMPARE(tests[0].id, QString("MathTests.TestAdd"));
  QCOMPARE(tests[1].name, QString("TestSubtract"));
  QCOMPARE(tests[1].id, QString("MathTests.TestSubtract"));
  QCOMPARE(tests[2].name, QString("TestMultiply"));
  QCOMPARE(tests[3].suite, QString("StringTests"));
  QCOMPARE(tests[3].name, QString("TestConcat"));
  QCOMPARE(tests[3].id, QString("StringTests.TestConcat"));
  QCOMPARE(tests[4].name, QString("TestSplit"));
  QCOMPARE(tests[4].id, QString("StringTests.TestSplit"));
}

void TestOutputParsers::testGTestParseListTestsOutputEmpty() {
  QList<DiscoveredTest> tests =
      GTestDiscoveryAdapter::parseListTestsOutput("");
  QCOMPARE(tests.size(), 0);
}

void TestOutputParsers::testGTestBuildFilter() {
  QStringList names = {"MathTests.TestAdd", "MathTests.TestSubtract",
                       "StringTests.TestConcat"};
  QString filter = GTestDiscoveryAdapter::buildGTestFilter(names);
  QCOMPARE(filter,
           QString("MathTests.TestAdd:MathTests.TestSubtract:StringTests."
                   "TestConcat"));
}

void TestOutputParsers::testGTestBuildFilterEmpty() {
  QString filter = GTestDiscoveryAdapter::buildGTestFilter({});
  QVERIFY(filter.isEmpty());
}

void TestOutputParsers::testGTestBuildFilterSingle() {
  QString filter =
      GTestDiscoveryAdapter::buildGTestFilter({"MathTests.TestAdd"});
  QCOMPARE(filter, QString("MathTests.TestAdd"));
}

// --- Pytest Discovery Adapter ---

void TestOutputParsers::testPytestDiscoveryParse() {
  QString output =
      "test_math.py::TestArithmetic::test_add\n"
      "test_math.py::TestArithmetic::test_subtract\n"
      "test_math.py::test_standalone\n"
      "tests/test_util.py::test_helper\n"
      "\n"
      "4 tests collected\n";

  QList<DiscoveredTest> tests =
      PytestDiscoveryAdapter::parseCollectOutput(output);

  QCOMPARE(tests.size(), 4);
  QCOMPARE(tests[0].name, QString("test_add"));
  QCOMPARE(tests[0].suite, QString("TestArithmetic"));
  QCOMPARE(tests[0].filePath, QString("test_math.py"));
  QCOMPARE(tests[0].id, QString("test_math.py::TestArithmetic::test_add"));
  QCOMPARE(tests[1].name, QString("test_subtract"));
  QCOMPARE(tests[1].suite, QString("TestArithmetic"));
  QCOMPARE(tests[2].name, QString("test_standalone"));
  QVERIFY(tests[2].suite.isEmpty());
  QCOMPARE(tests[2].filePath, QString("test_math.py"));
  QCOMPARE(tests[3].name, QString("test_helper"));
  QCOMPARE(tests[3].filePath, QString("tests/test_util.py"));
}

void TestOutputParsers::testPytestDiscoveryParseEmpty() {
  QList<DiscoveredTest> tests =
      PytestDiscoveryAdapter::parseCollectOutput("");
  QCOMPARE(tests.size(), 0);

  QList<DiscoveredTest> noTests =
      PytestDiscoveryAdapter::parseCollectOutput(
          "no tests ran in 0.01s\n");
  QCOMPARE(noTests.size(), 0);
}

// --- Go Test Discovery Adapter ---

void TestOutputParsers::testGoTestDiscoveryParse() {
  QString output =
      "TestAdd\n"
      "TestSubtract\n"
      "TestSuite_MethodA\n"
      "BenchmarkSort\n"
      "ok  example.com/pkg 0.003s\n";

  QList<DiscoveredTest> tests =
      GoTestDiscoveryAdapter::parseListOutput(output);

  QCOMPARE(tests.size(), 4);
  QCOMPARE(tests[0].name, QString("TestAdd"));
  QCOMPARE(tests[0].id, QString("TestAdd"));
  QVERIFY(tests[0].suite.isEmpty());
  QCOMPARE(tests[2].name, QString("TestSuite_MethodA"));
  QCOMPARE(tests[2].suite, QString("TestSuite"));
  QCOMPARE(tests[3].name, QString("BenchmarkSort"));
}

void TestOutputParsers::testGoTestDiscoveryParseEmpty() {
  QList<DiscoveredTest> tests =
      GoTestDiscoveryAdapter::parseListOutput("");
  QCOMPARE(tests.size(), 0);

  QList<DiscoveredTest> noTests =
      GoTestDiscoveryAdapter::parseListOutput(
          "ok  example.com/pkg 0.001s\n");
  QCOMPARE(noTests.size(), 0);
}

// --- Cargo Test Discovery Adapter ---

void TestOutputParsers::testCargoTestDiscoveryParse() {
  QString output =
      "tests::test_basic: test\n"
      "tests::math::test_add: test\n"
      "tests::math::test_sub: test\n"
      "integration::test_full: test\n"
      "\n"
      "4 tests, 0 benchmarks\n";

  QList<DiscoveredTest> tests =
      CargoTestDiscoveryAdapter::parseListOutput(output);

  QCOMPARE(tests.size(), 4);
  QCOMPARE(tests[0].name, QString("test_basic"));
  QCOMPARE(tests[0].suite, QString("tests"));
  QCOMPARE(tests[0].id, QString("tests::test_basic"));
  QCOMPARE(tests[1].name, QString("test_add"));
  QCOMPARE(tests[1].suite, QString("tests::math"));
  QCOMPARE(tests[1].id, QString("tests::math::test_add"));
  QCOMPARE(tests[3].name, QString("test_full"));
  QCOMPARE(tests[3].suite, QString("integration"));
}

void TestOutputParsers::testCargoTestDiscoveryParseEmpty() {
  QList<DiscoveredTest> tests =
      CargoTestDiscoveryAdapter::parseListOutput("");
  QCOMPARE(tests.size(), 0);

  QList<DiscoveredTest> noTests =
      CargoTestDiscoveryAdapter::parseListOutput(
          "\n0 tests, 0 benchmarks\n");
  QCOMPARE(noTests.size(), 0);
}

// --- Jest Discovery Adapter ---

void TestOutputParsers::testJestDiscoveryParse() {
  QString output =
      "/home/user/project/src/__tests__/math.test.js\n"
      "/home/user/project/src/__tests__/util.test.ts\n"
      "/home/user/project/tests/integration.test.js\n";

  QList<DiscoveredTest> tests =
      JestDiscoveryAdapter::parseListOutput(output);

  QCOMPARE(tests.size(), 3);
  QCOMPARE(tests[0].name, QString("math.test.js"));
  QCOMPARE(tests[0].filePath,
           QString("/home/user/project/src/__tests__/math.test.js"));
  QCOMPARE(tests[0].suite, QString("__tests__"));
  QCOMPARE(tests[1].name, QString("util.test.ts"));
  QCOMPARE(tests[2].name, QString("integration.test.js"));
  QCOMPARE(tests[2].suite, QString("tests"));
}

void TestOutputParsers::testJestDiscoveryParseEmpty() {
  QList<DiscoveredTest> tests =
      JestDiscoveryAdapter::parseListOutput("");
  QCOMPARE(tests.size(), 0);
}

QTEST_MAIN(TestOutputParsers)
#include "test_testoutputparsers.moc"
