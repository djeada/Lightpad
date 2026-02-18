#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMetaObject>
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
  void testGetTemplatesForExtension();
  void testGetTemplateById();
  void testAssignmentPersistence();
  void testAssignmentHookPersistence();
  void testBuildCommand();
  void testEmptyFilePath();
  void testWorkspaceFolderSubstitution();
  void testRunTemplateSelectorQuoteRoundTrip();

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
      QCOMPARE(tmpl.command, QString("python3"));
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
  bool assigned = manager.assignTemplateToFile(testFile, newAssignment);
  QVERIFY(assigned);

  FileTemplateAssignment assignment = manager.getAssignmentForFile(testFile);
  QCOMPARE(assignment.templateId, QString("python3"));
  QVERIFY(assignment.customArgs.contains("-v"));

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

  FileTemplateAssignment savedAssignment = manager.getAssignmentForFile(testFile);
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
  originalAssignment.customArgs =
      QStringList() << "--gtest_filter" << "Suite Name.*";
  originalAssignment.compilerFlags =
      QStringList() << "-DTEST_LABEL=With Space" << "-O2";
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

  QVERIFY(QMetaObject::invokeMethod(&selector, "onAccept",
                                    Qt::DirectConnection));

  FileTemplateAssignment savedAssignment = manager.getAssignmentForFile(testFile);
  QCOMPARE(savedAssignment.customArgs, originalAssignment.customArgs);
  QCOMPARE(savedAssignment.compilerFlags, originalAssignment.compilerFlags);

  QVERIFY(manager.removeAssignment(testFile));
}

QTEST_MAIN(TestRunTemplateManager)
#include "test_runtemplatemanager.moc"
