#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "diagnostics/compilerdiagnosticparser.h"

class TestCompilerDiagnosticParser : public QObject {
  Q_OBJECT

private slots:
  void init();

  void testGccErrorWithColumn();
  void testRealMakeOutputFromNestedProject();
  void testMakeNoiseIsNotADiagnostic();
  void testAnsiColouredOutput();
  void testWarningAndNoteSeverities();
  void testRelativePathsResolveAgainstBuildDirectory();
  void testUnknownFilesAreIgnored();
  void testDuplicatesFromParallelJobsCollapse();
  void testMsvcFormat();
  void testLinkerErrorYieldsNothing();
  void testGroupingByFile();

private:
  QString makeSource(const QString &relative);

  QTemporaryDir m_tempDir;
};

void TestCompilerDiagnosticParser::init() { QVERIFY(m_tempDir.isValid()); }

QString TestCompilerDiagnosticParser::makeSource(const QString &relative) {
  const QString absolute = m_tempDir.path() + "/" + relative;
  QDir().mkpath(QFileInfo(absolute).absolutePath());
  QFile file(absolute);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    file.write("int main() { return 0; }\n");
    file.close();
  }
  return absolute;
}

void TestCompilerDiagnosticParser::testGccErrorWithColumn() {
  const QString source = makeSource("main.cpp");
  const QString output =
      source + ":142:2: error: expected ‘}’ at end of input\n";

  const QList<CompilerDiagnostic> results =
      CompilerDiagnosticParser::parse(output, m_tempDir.path());

  QCOMPARE(results.size(), 1);
  QCOMPARE(results.first().filePath, source);

  QCOMPARE(results.first().diagnostic.range.start.line, 141);
  QCOMPARE(results.first().diagnostic.range.start.character, 1);
  QCOMPARE(results.first().diagnostic.severity, LspDiagnosticSeverity::Error);
  QVERIFY(results.first().diagnostic.message.contains("end of input"));
}

void TestCompilerDiagnosticParser::testRealMakeOutputFromNestedProject() {
  const QString source = makeSource("src/example/main.cpp");
  const QString buildDir = m_tempDir.path() + "/src/example/build";
  QDir().mkpath(buildDir);

  const QString output =
      "gmake[4]: Entering directory '" + buildDir +
      "'\n"
      "[ 50%] Building CXX object CMakeFiles/vao_example.dir/main.cpp.o\n" +
      source +
      ":142:2: error: expected ‘}’ at end of input\n"
      "  142 | }\n"
      "      |  ^\n" +
      source +
      ":9:74: note: to match this ‘{’\n"
      "    9 | class Window : public QOpenGLWidget {\n" +
      source +
      ":142:2: error: expected unqualified-id at end of input\n"
      "gmake[4]: *** [CMakeFiles/vao_example.dir/build.make:76: "
      "CMakeFiles/vao_example.dir/main.cpp.o] Error 1\n"
      "gmake[1]: *** [Makefile:124: vao_example] Error 2\n";

  const QList<CompilerDiagnostic> results =
      CompilerDiagnosticParser::parse(output, buildDir);

  QCOMPARE(results.size(), 3);
  for (const CompilerDiagnostic &entry : results) {
    QCOMPARE(entry.filePath, source);
  }
  QCOMPARE(results.at(0).diagnostic.range.start.line, 141);
  QCOMPARE(results.at(0).diagnostic.severity, LspDiagnosticSeverity::Error);

  QCOMPARE(results.at(1).diagnostic.range.start.line, 8);
  QCOMPARE(results.at(1).diagnostic.severity,
           LspDiagnosticSeverity::Information);
  QCOMPARE(results.at(2).diagnostic.severity, LspDiagnosticSeverity::Error);
}

void TestCompilerDiagnosticParser::testMakeNoiseIsNotADiagnostic() {
  const QString output =
      "gmake[4]: *** [CMakeFiles/vao_example.dir/build.make:76: "
      "CMakeFiles/vao_example.dir/main.cpp.o] Error 1\n"
      "gmake[3]: *** [CMakeFiles/Makefile2:83: "
      "CMakeFiles/vao_example.dir/all] Error 2\n"
      "gmake[1]: Entering directory '/tmp/build'\n"
      "[ 50%] Building CXX object CMakeFiles/app.dir/main.cpp.o\n";

  QVERIFY(CompilerDiagnosticParser::parse(output, m_tempDir.path()).isEmpty());
}

void TestCompilerDiagnosticParser::testAnsiColouredOutput() {
  const QString source = makeSource("coloured.cpp");
  const QString output = "\x1B[32m[ 50%] Building\x1B[0m\n" + source +
                         ":7:3: \x1B[31merror:\x1B[0m something went wrong\n";

  const QList<CompilerDiagnostic> results =
      CompilerDiagnosticParser::parse(output, m_tempDir.path());

  QCOMPARE(results.size(), 1);
  QCOMPARE(results.first().filePath, source);
  QCOMPARE(results.first().diagnostic.message, QString("something went wrong"));
}

void TestCompilerDiagnosticParser::testWarningAndNoteSeverities() {
  const QString source = makeSource("severity.cpp");
  const QString output = source + ":3:1: warning: unused variable 'x'\n" +
                         source + ":4:1: fatal error: no such file\n";

  const QList<CompilerDiagnostic> results =
      CompilerDiagnosticParser::parse(output, m_tempDir.path());

  QCOMPARE(results.size(), 2);
  QCOMPARE(results.at(0).diagnostic.severity, LspDiagnosticSeverity::Warning);
  QCOMPARE(results.at(1).diagnostic.severity, LspDiagnosticSeverity::Error);
}

void TestCompilerDiagnosticParser::
    testRelativePathsResolveAgainstBuildDirectory() {
  makeSource("nested/rel.cpp");
  const QString output = "rel.cpp:12: error: something\n";

  const QList<CompilerDiagnostic> results =
      CompilerDiagnosticParser::parse(output, m_tempDir.path() + "/nested");

  QCOMPARE(results.size(), 1);
  QCOMPARE(results.first().filePath, m_tempDir.path() + "/nested/rel.cpp");

  QCOMPARE(results.first().diagnostic.range.start.character, 0);
}

void TestCompilerDiagnosticParser::testUnknownFilesAreIgnored() {
  const QString output =
      m_tempDir.path() + "/does-not-exist.cpp:3:1: error: nope\n";

  QVERIFY(CompilerDiagnosticParser::parse(output, m_tempDir.path()).isEmpty());
}

void TestCompilerDiagnosticParser::testDuplicatesFromParallelJobsCollapse() {
  const QString source = makeSource("dup.cpp");
  const QString line = source + ":5:9: error: same problem\n";

  const QList<CompilerDiagnostic> results =
      CompilerDiagnosticParser::parse(line + line + line, m_tempDir.path());

  QCOMPARE(results.size(), 1);
}

void TestCompilerDiagnosticParser::testMsvcFormat() {
  const QString source = makeSource("msvc.cpp");
  const QString output =
      source + "(142,2): error C2143: syntax error: missing '}'\n";

  const QList<CompilerDiagnostic> results =
      CompilerDiagnosticParser::parse(output, m_tempDir.path());

  QCOMPARE(results.size(), 1);
  QCOMPARE(results.first().diagnostic.range.start.line, 141);
  QCOMPARE(results.first().diagnostic.code, QString("C2143"));
  QCOMPARE(results.first().diagnostic.severity, LspDiagnosticSeverity::Error);
}

void TestCompilerDiagnosticParser::testLinkerErrorYieldsNothing() {

  const QString output = "/usr/bin/ld: cannot find -lfoo\n"
                         "collect2: error: ld returned 1 exit status\n";

  QVERIFY(CompilerDiagnosticParser::parse(output, m_tempDir.path()).isEmpty());
}

void TestCompilerDiagnosticParser::testGroupingByFile() {
  const QString first = makeSource("group-a.cpp");
  const QString second = makeSource("group-b.cpp");
  const QString output = first + ":1:1: error: a\n" + second +
                         ":2:1: error: b\n" + first + ":3:1: warning: c\n";

  const QMap<QString, QList<LspDiagnostic>> grouped =
      CompilerDiagnosticParser::parseByFile(output, m_tempDir.path());

  QCOMPARE(grouped.size(), 2);
  QCOMPARE(grouped.value(first).size(), 2);
  QCOMPARE(grouped.value(second).size(), 1);
}

QTEST_MAIN(TestCompilerDiagnosticParser)
#include "test_compilerdiagnosticparser.moc"
