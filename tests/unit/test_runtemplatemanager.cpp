#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMetaObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "run_templates/runtemplatemanager.h"
#include "ui/dialogs/runtemplateselector.h"

class TestRunTemplateManager : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testSubstituteVariables();
  void testSubstituteVariablesWithComplexPath();
  void testParseTemplateFromJson();
  void testCommonCppTestTemplatesPresent();
  void testMakefileTemplatesPresent();
  void testBuildCommandForMakefile();
  void testGetTemplatesForExtension();
  void testGetTemplateById();
  void testAssignmentPersistence();
  void testAssignmentHookPersistence();
  void testBuildCommand();
  void testPythonEnvironmentDisablesOutputBuffering();
  void testEmptyFilePath();
  void testWorkspaceFolderSubstitution();
  void testRunTemplateSelectorQuoteRoundTrip();
  void testShellSubstitutionQuotesValues_data();
  void testShellSubstitutionQuotesValues();
  void testShellSubstitutionLeavesUnknownVariables();
  void testShellScriptArgumentIndex();
  void testBuildCommandQuotesHostileFileName();
  void testAssignmentKeepsExistingEntries();
  void testAssignmentRefusesToOverwriteUnreadableConfig();

private:
  QTemporaryDir m_tempDir;
};

void TestRunTemplateManager::initTestCase() { QVERIFY(m_tempDir.isValid()); }

void TestRunTemplateManager::cleanupTestCase() {}

void TestRunTemplateManager::testSubstituteVariables() {
  QString filePath = "/home/user/project/main.py";

  QString result1 =
      RunTemplateManager::substituteVariables("${file}", filePath);
  QCOMPARE(result1, filePath);

  QString result2 =
      RunTemplateManager::substituteVariables("${fileDir}", filePath);
  QCOMPARE(result2, QString("/home/user/project"));

  QString result3 =
      RunTemplateManager::substituteVariables("${fileBasename}", filePath);
  QCOMPARE(result3, QString("main.py"));

  QString result4 =
      RunTemplateManager::substituteVariables("${fileBasenameNoExt}", filePath);
  QCOMPARE(result4, QString("main"));

  QString result5 =
      RunTemplateManager::substituteVariables("${fileExt}", filePath);
  QCOMPARE(result5, QString("py"));
}

void TestRunTemplateManager::testSubstituteVariablesWithComplexPath() {
  QString filePath = "/home/user/my-project/src/hello_world.cpp";

  QString result = RunTemplateManager::substituteVariables(
      "g++ -o ${fileBasenameNoExt} ${file}", filePath);
  QCOMPARE(
      result,
      QString("g++ -o hello_world /home/user/my-project/src/hello_world.cpp"));
}

void TestRunTemplateManager::testParseTemplateFromJson() {

  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();

  QList<RunTemplate> templates = manager.getAllTemplates();
  QVERIFY(!templates.isEmpty());

  bool foundPython = false;
  for (const RunTemplate &tmpl : templates) {
    if (tmpl.id == "python3") {
      foundPython = true;
      QCOMPARE(tmpl.name, QString("Python 3"));
      QVERIFY(tmpl.extensions.contains("py"));
      QCOMPARE(tmpl.command, QString("${python}"));
      break;
    }
  }
  QVERIFY(foundPython);
}

void TestRunTemplateManager::testCommonCppTestTemplatesPresent() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();

  RunTemplate ctestTemplate = manager.getTemplateById("cpp_cmake_ctest");
  QVERIFY(ctestTemplate.isValid());
  QCOMPARE(ctestTemplate.command, QString("bash"));
  QVERIFY(ctestTemplate.args.join(" ").contains("ctest"));

  RunTemplate makeTemplate = manager.getTemplateById("cpp_make_test");
  QVERIFY(makeTemplate.isValid());
  QCOMPARE(makeTemplate.command, QString("make"));
  QVERIFY(makeTemplate.args.contains("test"));
}

void TestRunTemplateManager::testMakefileTemplatesPresent() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();

  RunTemplate defaultTarget = manager.getTemplateById("make");
  QVERIFY(defaultTarget.isValid());
  QCOMPARE(defaultTarget.command, QString("make"));
  QVERIFY(defaultTarget.extensions.contains("Makefile"));
  QCOMPARE(defaultTarget.workingDirectory, QString("${fileDir}"));

  RunTemplate makeAll = manager.getTemplateById("make_all");
  QVERIFY(makeAll.isValid());
  QCOMPARE(makeAll.command, QString("make"));
  QVERIFY(makeAll.args.contains("all"));
  QVERIFY(makeAll.extensions.contains("Makefile"));

  RunTemplate makeClean = manager.getTemplateById("make_clean");
  QVERIFY(makeClean.isValid());
  QCOMPARE(makeClean.command, QString("make"));
  QVERIFY(makeClean.args.contains("clean"));

  RunTemplate makeTest = manager.getTemplateById("make_test");
  QVERIFY(makeTest.isValid());
  QCOMPARE(makeTest.command, QString("make"));
  QVERIFY(makeTest.args.contains("test"));

  RunTemplate makeInstall = manager.getTemplateById("make_install");
  QVERIFY(makeInstall.isValid());
  QCOMPARE(makeInstall.command, QString("make"));
  QVERIFY(makeInstall.args.contains("install"));

  RunTemplate makeRun = manager.getTemplateById("make_run");
  QVERIFY(makeRun.isValid());
  QCOMPARE(makeRun.command, QString("make"));
  QVERIFY(makeRun.args.contains("run"));

  RunTemplate makeParallel = manager.getTemplateById("make_parallel");
  QVERIFY(makeParallel.isValid());
  QCOMPARE(makeParallel.command, QString("bash"));
  QVERIFY(makeParallel.args.join(" ").contains("nproc"));

  QList<RunTemplate> makefileTemplates =
      manager.getTemplatesForExtension("Makefile");
  QVERIFY(makefileTemplates.size() >= 7);
}

void TestRunTemplateManager::testBuildCommandForMakefile() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();
  manager.setWorkspaceFolder(m_tempDir.path());

  QString makefilePath = m_tempDir.path() + "/Makefile";
  QFile file(makefilePath);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("help:\n\t@echo help\n");
  file.close();

  QList<RunTemplate> templates = manager.getTemplatesForFilePath(makefilePath);
  QVERIFY(!templates.isEmpty());
  QCOMPARE(templates.first().id, QString("make"));

  QPair<QString, QStringList> cmd = manager.buildCommand(makefilePath);
  QCOMPARE(cmd.first, QString("make"));
  QVERIFY(cmd.second.isEmpty());

  FileTemplateAssignment assignment;
  assignment.templateId = "make";
  assignment.customArgs = QStringList() << "arena";
  QVERIFY(manager.assignTemplateToFile(makefilePath, assignment));

  cmd = manager.buildCommand(makefilePath);
  QCOMPARE(cmd.first, QString("make"));
  QCOMPARE(cmd.second, QStringList() << "arena");

  QVERIFY(manager.removeAssignment(makefilePath));
}

void TestRunTemplateManager::testGetTemplatesForExtension() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();

  QList<RunTemplate> pyTemplates = manager.getTemplatesForExtension("py");
  QVERIFY(!pyTemplates.isEmpty());

  bool foundPython = false;
  for (const RunTemplate &tmpl : pyTemplates) {
    if (tmpl.language == "Python") {
      foundPython = true;
      break;
    }
  }
  QVERIFY(foundPython);

  QList<RunTemplate> cppTemplates = manager.getTemplatesForExtension("cpp");
  QVERIFY(!cppTemplates.isEmpty());

  QList<RunTemplate> unknownTemplates =
      manager.getTemplatesForExtension("xyz123");
  QVERIFY(unknownTemplates.isEmpty());
}

void TestRunTemplateManager::testGetTemplateById() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();

  RunTemplate tmpl = manager.getTemplateById("python3");
  QVERIFY(tmpl.isValid());
  QCOMPARE(tmpl.name, QString("Python 3"));

  RunTemplate invalidTmpl = manager.getTemplateById("nonexistent_id");
  QVERIFY(!invalidTmpl.isValid());
}

void TestRunTemplateManager::testAssignmentPersistence() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();
  manager.setWorkspaceFolder(m_tempDir.path());

  QString testFile = m_tempDir.path() + "/test.py";
  QFile file(testFile);
  file.open(QIODevice::WriteOnly);
  file.write("print('hello')");
  file.close();

  FileTemplateAssignment newAssignment;
  newAssignment.templateId = "python3";
  newAssignment.customArgs = QStringList() << "-v";
  newAssignment.pythonMode = PythonProjectEnvironment::workspaceVenvMode();
  newAssignment.pythonVenvPath = "${workspaceFolder}/.venv";
  bool assigned = manager.assignTemplateToFile(testFile, newAssignment);
  QVERIFY(assigned);

  FileTemplateAssignment assignment = manager.getAssignmentForFile(testFile);
  QCOMPARE(assignment.templateId, QString("python3"));
  QVERIFY(assignment.customArgs.contains("-v"));
  QCOMPARE(assignment.pythonMode,
           QString(PythonProjectEnvironment::workspaceVenvMode()));
  QCOMPARE(assignment.pythonVenvPath, QString("${workspaceFolder}/.venv"));

  QString configFile = m_tempDir.path() + "/.lightpad/run_config.json";
  QVERIFY(QFile::exists(configFile));

  bool removed = manager.removeAssignment(testFile);
  QVERIFY(removed);

  FileTemplateAssignment removedAssignment =
      manager.getAssignmentForFile(testFile);
  QVERIFY(removedAssignment.templateId.isEmpty());
}

void TestRunTemplateManager::testAssignmentHookPersistence() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();
  manager.setWorkspaceFolder(m_tempDir.path());

  QString testFile = m_tempDir.path() + "/hooks_test.py";
  QFile file(testFile);
  file.open(QIODevice::WriteOnly);
  file.write("print('hello')");
  file.close();

  FileTemplateAssignment newAssignment;
  newAssignment.templateId = "python3";
  newAssignment.preRunCommand = "echo PRE";
  newAssignment.postRunCommand = "echo POST";
  QVERIFY(manager.assignTemplateToFile(testFile, newAssignment));

  FileTemplateAssignment savedAssignment =
      manager.getAssignmentForFile(testFile);
  QCOMPARE(savedAssignment.preRunCommand, QString("echo PRE"));
  QCOMPARE(savedAssignment.postRunCommand, QString("echo POST"));

  QVERIFY(manager.removeAssignment(testFile));
}

void TestRunTemplateManager::testBuildCommand() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();

  QString testFile = m_tempDir.path() + "/hello.py";
  QFile file(testFile);
  file.open(QIODevice::WriteOnly);
  file.write("print('hello')");
  file.close();

  QPair<QString, QStringList> cmd = manager.buildCommand(testFile);
  QVERIFY(!cmd.first.isEmpty());

  QVERIFY(cmd.first.contains("python"));
}

void TestRunTemplateManager::testPythonEnvironmentDisablesOutputBuffering() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();
  manager.setWorkspaceFolder(m_tempDir.path());

  QString testFile = m_tempDir.path() + "/unbuffered.py";
  QFile file(testFile);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("print('ready')\n");
  file.close();

  QMap<QString, QString> env = manager.getEnvironment(testFile);
  QCOMPARE(env.value("PYTHONUNBUFFERED"), QString("1"));
}

void TestRunTemplateManager::testEmptyFilePath() {
  RunTemplateManager &manager = RunTemplateManager::instance();

  QPair<QString, QStringList> cmd = manager.buildCommand("");
  QVERIFY(cmd.first.isEmpty());

  FileTemplateAssignment assignment = manager.getAssignmentForFile("");
  QVERIFY(assignment.templateId.isEmpty());
}

void TestRunTemplateManager::testWorkspaceFolderSubstitution() {
  RunTemplateManager &manager = RunTemplateManager::instance();

  manager.setWorkspaceFolder("/tmp/lightpad-workspace");
  QString result = RunTemplateManager::substituteVariables(
      "${workspaceFolder}/tests", "/tmp/lightpad-workspace/src/main.cpp");
  QCOMPARE(result, QString("/tmp/lightpad-workspace/tests"));

  manager.setWorkspaceFolder(QString());
  result = RunTemplateManager::substituteVariables(
      "${workspaceFolder}", "/tmp/lightpad-workspace/src/main.cpp");
  QCOMPARE(result, QString("/tmp/lightpad-workspace/src"));
}

void TestRunTemplateManager::testRunTemplateSelectorQuoteRoundTrip() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();
  manager.setWorkspaceFolder(m_tempDir.path());

  QString testFile = m_tempDir.path() + "/quote_roundtrip.cpp";
  QFile file(testFile);
  file.open(QIODevice::WriteOnly);
  file.write("int main() { return 0; }\n");
  file.close();

  FileTemplateAssignment originalAssignment;
  originalAssignment.templateId = "cpp_gcc";
  originalAssignment.customArgs = QStringList()
                                  << "--gtest_filter" << "Suite Name.*";
  originalAssignment.compilerFlags = QStringList()
                                     << "-DTEST_LABEL=With Space" << "-O2";
  QVERIFY(manager.assignTemplateToFile(testFile, originalAssignment));

  RunTemplateSelector selector(testFile);

  auto findByPlaceholder = [&selector](const QString &needle) -> QLineEdit * {
    for (QLineEdit *edit : selector.findChildren<QLineEdit *>()) {
      if (edit && edit->placeholderText().contains(needle)) {
        return edit;
      }
    }
    return nullptr;
  };

  QLineEdit *customArgsEdit = findByPlaceholder("Additional arguments");
  QLineEdit *compilerFlagsEdit = findByPlaceholder("-std=c++17");

  QVERIFY(customArgsEdit != nullptr);
  QVERIFY(compilerFlagsEdit != nullptr);

  QVERIFY(customArgsEdit->text().contains("\"Suite Name.*\""));
  QVERIFY(compilerFlagsEdit->text().contains("\"-DTEST_LABEL=With Space\""));

  QVERIFY(
      QMetaObject::invokeMethod(&selector, "onAccept", Qt::DirectConnection));

  FileTemplateAssignment savedAssignment =
      manager.getAssignmentForFile(testFile);
  QCOMPARE(savedAssignment.customArgs, originalAssignment.customArgs);
  QCOMPARE(savedAssignment.compilerFlags, originalAssignment.compilerFlags);

  QVERIFY(manager.removeAssignment(testFile));
}

void TestRunTemplateManager::testShellSubstitutionQuotesValues_data() {
  QTest::addColumn<QString>("script");
  QTest::addColumn<QString>("expectedTemplate");

  QTest::newRow("unquoted") << "printf '%s' ${v}" << "V";
  QTest::newRow("double") << "printf '%s' \"pre ${v} post\"" << "pre V post";
  QTest::newRow("single") << "printf '%s' 'pre ${v} post'" << "pre V post";
  QTest::newRow("adjacent") << "printf '%s' x${v}\"${v}\"'${v}'" << "xVVV";
  QTest::newRow("command-substitution")
      << "printf '%s' \"$(printf '%s' \"${v}\")\"" << "V";
  QTest::newRow("backtick") << "printf '%s' \"`printf '%s' ${v}`\"" << "V";
  QTest::newRow("subshell") << "(printf '%s' \"${v}\") && printf '!'" << "V!";
  QTest::newRow("escaped-quote") << "printf '%s' \\\"${v}\\\"" << "\"V\"";
  QTest::newRow("comment") << "printf '%s' ${v} # it's ${v}" << "V";
}

void TestRunTemplateManager::testShellSubstitutionQuotesValues() {
  QFETCH(QString, script);
  QFETCH(QString, expectedTemplate);

  if (QStandardPaths::findExecutable("bash").isEmpty()) {
    QSKIP("bash is not available");
  }

  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  const QStringList values = {"plain",
                              "with space",
                              "x$(touch pwned1)",
                              "x`touch pwned2`",
                              "a\"b",
                              "a'b",
                              "a\\b",
                              "a\\\\'\"$HOME",
                              ";touch pwned3;",
                              "$'x'",
                              "'\"'\"`$(touch pwned4)`"};

  for (const QString &value : values) {
    QMap<QString, QString> vars;
    vars.insert("v", value);
    const QString substituted =
        PythonProjectEnvironment::substituteShellVariables(script, vars);

    QProcess process;
    process.setWorkingDirectory(dir.path());
    process.start("bash", {"-c", substituted});
    QVERIFY(process.waitForFinished(10000));
    QString expected = expectedTemplate;
    expected.replace("V", value);
    QCOMPARE(QString::fromUtf8(process.readAllStandardOutput()), expected);
    QVERIFY2(QDir(dir.path()).entryList(QDir::Files).isEmpty(),
             qPrintable(substituted));
  }
}

void TestRunTemplateManager::testShellSubstitutionLeavesUnknownVariables() {
  QMap<QString, QString> vars;
  vars.insert("file", "a b");
  QCOMPARE(PythonProjectEnvironment::substituteShellVariables(
               "echo ${HOME} ${file} $PATH", vars),
           QString("echo ${HOME} 'a b' $PATH"));
  QCOMPARE(PythonProjectEnvironment::substituteShellVariables(
               "echo \"${file}\"", vars),
           QString("echo \"\"'a b'\"\""));
  QCOMPARE(PythonProjectEnvironment::substituteShellVariables("echo '${file}'",
                                                              vars),
           QString("echo '''a b'''"));
  QCOMPARE(PythonProjectEnvironment::shellQuote("it's"), QString("'it'\\''s'"));
}

void TestRunTemplateManager::testShellScriptArgumentIndex() {
  QCOMPARE(PythonProjectEnvironment::shellScriptArgumentIndex("bash",
                                                              {"-c", "echo"}),
           1);
  QCOMPARE(PythonProjectEnvironment::shellScriptArgumentIndex("/bin/sh",
                                                              {"-lc", "echo"}),
           1);
  QCOMPARE(PythonProjectEnvironment::shellScriptArgumentIndex(
               "zsh", {"-o", "pipefail", "-ic", "echo"}),
           3);
  QCOMPARE(PythonProjectEnvironment::shellScriptArgumentIndex(
               "bash", {"script.sh", "-c", "echo"}),
           -1);
  QCOMPARE(PythonProjectEnvironment::shellScriptArgumentIndex(
               "python3", {"-c", "print(1)"}),
           -1);
}

void TestRunTemplateManager::testBuildCommandQuotesHostileFileName() {
  if (QStandardPaths::findExecutable("bash").isEmpty()) {
    QSKIP("bash is not available");
  }

  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();

  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  manager.setWorkspaceFolder(dir.path());

  const QString binDir = dir.path() + "/bin";
  QVERIFY(QDir().mkpath(binDir));
  QFile fakeCompiler(binDir + "/g++");
  QVERIFY(fakeCompiler.open(QIODevice::WriteOnly));
  fakeCompiler.write("#!/bin/sh\n"
                     "printf '#!/bin/sh\\necho ran\\n' > \"$3\"\n"
                     "chmod +x \"$3\"\n");
  fakeCompiler.close();
  fakeCompiler.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                              QFileDevice::ExeOwner);

  const QString srcDir = dir.path() + "/src";
  QVERIFY(QDir().mkpath(srcDir));
  const QString hostile = srcDir + "/x$(touch pwned)a\"b'c.cpp";
  QFile source(hostile);
  QVERIFY(source.open(QIODevice::WriteOnly));
  source.write("int main() { return 0; }\n");
  source.close();

  FileTemplateAssignment assignment;
  assignment.templateId = "cpp_gcc";
  assignment.sourceFiles = QStringList()
                           << "${fileDir}/extra $(touch pwned2).cpp";
  QVERIFY(manager.assignTemplateToFile(hostile, assignment));

  const QPair<QString, QStringList> cmd = manager.buildCommand(hostile, "cpp");
  QCOMPARE(cmd.first, QString("bash"));

  QProcess process;
  process.setWorkingDirectory(srcDir);
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("PATH", binDir + ":" + env.value("PATH"));
  process.setProcessEnvironment(env);
  process.start(cmd.first, cmd.second);
  QVERIFY(process.waitForFinished(10000));
  QCOMPARE(QString::fromUtf8(process.readAllStandardOutput()).trimmed(),
           QString("ran"));
  QVERIFY(!QFile::exists(srcDir + "/pwned"));
  QVERIFY(!QFile::exists(srcDir + "/pwned2"));
  QVERIFY(QFile::exists(srcDir + "/x$(touch pwned)a\"b'c"));

  QVERIFY(manager.removeAssignment(hostile));
  manager.setWorkspaceFolder(QString());
}

void TestRunTemplateManager::testAssignmentKeepsExistingEntries() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();

  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(QDir().mkpath(dir.path() + "/.lightpad"));
  const QString configPath = dir.path() + "/.lightpad/run_config.json";
  {
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"version\":\"1.0\",\"assignments\":[{\"file\":\"a.py\","
               "\"template\":\"python3\"}]}");
  }

  manager.setWorkspaceFolder(QString());
  manager.setWorkspaceFolder(dir.path());
  FileTemplateAssignment assignment;
  assignment.templateId = "python3";
  QVERIFY(manager.assignTemplateToFile(dir.path() + "/b.py", assignment));

  QFile file(configPath);
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QJsonArray entries =
      QJsonDocument::fromJson(file.readAll()).object()["assignments"].toArray();
  QCOMPARE(entries.size(), 2);
  manager.setWorkspaceFolder(QString());
}

void TestRunTemplateManager::
    testAssignmentRefusesToOverwriteUnreadableConfig() {
  RunTemplateManager &manager = RunTemplateManager::instance();
  manager.loadTemplates();

  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QVERIFY(QDir().mkpath(dir.path() + "/.lightpad"));
  const QString configPath = dir.path() + "/.lightpad/run_config.json";
  const QByteArray corrupt = "{\"assignments\": [";
  {
    QFile file(configPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(corrupt);
  }

  manager.setWorkspaceFolder(QString());
  manager.setWorkspaceFolder(dir.path());
  FileTemplateAssignment assignment;
  assignment.templateId = "python3";
  QVERIFY(!manager.assignTemplateToFile(dir.path() + "/b.py", assignment));

  QFile file(configPath);
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), corrupt);
  manager.setWorkspaceFolder(QString());
}

QTEST_MAIN(TestRunTemplateManager)
#include "test_runtemplatemanager.moc"
