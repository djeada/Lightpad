#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "format_templates/formattemplatemanager.h"

class TestFormatTemplateManager : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  void testSubstituteVariables();
  void testSubstituteVariablesWithComplexPath();
  void testParseTemplateFromJson();
  void testGetTemplatesForExtension();
  void testGetTemplateById();
  void testAssignmentPersistence();
  void testBuildCommand();
  void testEmptyFilePath();
  void testHasFormatTemplate();

private:
  QTemporaryDir m_tempDir;
};

void TestFormatTemplateManager::initTestCase() { QVERIFY(m_tempDir.isValid()); }

void TestFormatTemplateManager::cleanupTestCase() {}

void TestFormatTemplateManager::testSubstituteVariables() {
  QString filePath = "/home/user/project/main.cpp";

  QString result1 =
      FormatTemplateManager::substituteVariables("${file}", filePath);
  QCOMPARE(result1, filePath);

  QString result2 =
      FormatTemplateManager::substituteVariables("${fileDir}", filePath);
  QCOMPARE(result2, QString("/home/user/project"));

  QString result3 =
      FormatTemplateManager::substituteVariables("${fileBasename}", filePath);
  QCOMPARE(result3, QString("main.cpp"));

  QString result4 = FormatTemplateManager::substituteVariables(
      "${fileBasenameNoExt}", filePath);
  QCOMPARE(result4, QString("main"));

  QString result5 =
      FormatTemplateManager::substituteVariables("${fileExt}", filePath);
  QCOMPARE(result5, QString("cpp"));

  QString result6 = FormatTemplateManager::substituteVariables(
      "${workspaceFolder}", filePath);
  QCOMPARE(result6, QString("/home/user/project"));
}

void TestFormatTemplateManager::testSubstituteVariablesWithComplexPath() {
  QString filePath = "/home/user/my-project/src/hello_world.py";

  QString result = FormatTemplateManager::substituteVariables(
      "black --line-length 88 ${file}", filePath);
  QCOMPARE(
      result,
      QString(
          "black --line-length 88 /home/user/my-project/src/hello_world.py"));
}

void TestFormatTemplateManager::testParseTemplateFromJson() {

  FormatTemplateManager &manager = FormatTemplateManager::instance();
  manager.loadTemplates();

  QList<FormatTemplate> templates = manager.getAllTemplates();
  QVERIFY(!templates.isEmpty());

  bool foundBlack = false;
  for (const FormatTemplate &tmpl : templates) {
    if (tmpl.id == "black") {
      foundBlack = true;
      QCOMPARE(tmpl.name, QString("Black"));
      QVERIFY(tmpl.extensions.contains("py"));
      QCOMPARE(tmpl.command, QString("black"));
      break;
    }
  }
  QVERIFY(foundBlack);
}

void TestFormatTemplateManager::testGetTemplatesForExtension() {
  FormatTemplateManager &manager = FormatTemplateManager::instance();
  manager.loadTemplates();

  QList<FormatTemplate> pyTemplates = manager.getTemplatesForExtension("py");
  QVERIFY(!pyTemplates.isEmpty());

  bool foundPythonFormatter = false;
  for (const FormatTemplate &tmpl : pyTemplates) {
    if (tmpl.language == "Python") {
      foundPythonFormatter = true;
      break;
    }
  }
  QVERIFY(foundPythonFormatter);

  QList<FormatTemplate> cppTemplates = manager.getTemplatesForExtension("cpp");
  QVERIFY(!cppTemplates.isEmpty());

  QList<FormatTemplate> unknownTemplates =
      manager.getTemplatesForExtension("xyz123");
  QVERIFY(unknownTemplates.isEmpty());
}

void TestFormatTemplateManager::testGetTemplateById() {
  FormatTemplateManager &manager = FormatTemplateManager::instance();
  manager.loadTemplates();

  FormatTemplate tmpl = manager.getTemplateById("black");
  QVERIFY(tmpl.isValid());
  QCOMPARE(tmpl.name, QString("Black"));

  FormatTemplate invalidTmpl = manager.getTemplateById("nonexistent_id");
  QVERIFY(!invalidTmpl.isValid());
}

void TestFormatTemplateManager::testAssignmentPersistence() {
  FormatTemplateManager &manager = FormatTemplateManager::instance();
  manager.loadTemplates();

  QString testFile = m_tempDir.path() + "/test.py";
  QFile file(testFile);
  file.open(QIODevice::WriteOnly);
  file.write("print('hello')");
  file.close();

  FileFormatAssignment newAssignment;
  newAssignment.filePath = testFile;
  newAssignment.templateId = "black";
  newAssignment.customArgs = QStringList() << "--line-length" << "120";
  newAssignment.workingDirectory = "${fileDir}";
  newAssignment.customEnv["PYTHONPATH"] = "${fileDir}";
  newAssignment.preFormatCommand = "echo pre-format";
  newAssignment.postFormatCommand = "echo post-format";

  bool assigned = manager.assignTemplateToFile(testFile, newAssignment);
  QVERIFY(assigned);

  FileFormatAssignment assignment = manager.getAssignmentForFile(testFile);
  QCOMPARE(assignment.templateId, QString("black"));
  QVERIFY(assignment.customArgs.contains("--line-length"));
  QCOMPARE(assignment.workingDirectory, QString("${fileDir}"));
  QCOMPARE(assignment.customEnv.value("PYTHONPATH"), QString("${fileDir}"));
  QCOMPARE(assignment.preFormatCommand, QString("echo pre-format"));
  QCOMPARE(assignment.postFormatCommand, QString("echo post-format"));

  QString configFile = m_tempDir.path() + "/.lightpad/format_config.json";
  QVERIFY(QFile::exists(configFile));

  bool removed = manager.removeAssignment(testFile);
  QVERIFY(removed);

  FileFormatAssignment removedAssignment =
      manager.getAssignmentForFile(testFile);
  QVERIFY(removedAssignment.templateId.isEmpty());
}

void TestFormatTemplateManager::testBuildCommand() {
  FormatTemplateManager &manager = FormatTemplateManager::instance();
  manager.loadTemplates();

  QString testFile = m_tempDir.path() + "/hello.py";
  QFile file(testFile);
  file.open(QIODevice::WriteOnly);
  file.write("print('hello')");
  file.close();

  QPair<QString, QStringList> cmd = manager.buildCommand(testFile);
  QVERIFY(!cmd.first.isEmpty());

  QVERIFY(cmd.first.contains("black") || cmd.first.contains("autopep8") ||
          cmd.first.contains("yapf"));
}

void TestFormatTemplateManager::testEmptyFilePath() {
  FormatTemplateManager &manager = FormatTemplateManager::instance();

  QPair<QString, QStringList> cmd = manager.buildCommand("");
  QVERIFY(cmd.first.isEmpty());

  FileFormatAssignment assignment = manager.getAssignmentForFile("");
  QVERIFY(assignment.templateId.isEmpty());
}

void TestFormatTemplateManager::testHasFormatTemplate() {
  FormatTemplateManager &manager = FormatTemplateManager::instance();
  manager.loadTemplates();

  QString testFile = m_tempDir.path() + "/test.cpp";
  QFile file(testFile);
  file.open(QIODevice::WriteOnly);
  file.write("int main() {}");
  file.close();

  QVERIFY(manager.hasFormatTemplate(testFile));

  QString unknownFile = m_tempDir.path() + "/test.xyz123";
  QFile unknownFileHandle(unknownFile);
  unknownFileHandle.open(QIODevice::WriteOnly);
  unknownFileHandle.write("test");
  unknownFileHandle.close();

  QVERIFY(!manager.hasFormatTemplate(unknownFile));

  QVERIFY(!manager.hasFormatTemplate(""));
}

QTEST_MAIN(TestFormatTemplateManager)
#include "test_formattemplatemanager.moc"
