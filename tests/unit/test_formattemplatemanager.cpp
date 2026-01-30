#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFile>

#include "format_templates/formattemplatemanager.h"

class TestFormatTemplateManager : public QObject
{
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

void TestFormatTemplateManager::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestFormatTemplateManager::cleanupTestCase()
{
}

void TestFormatTemplateManager::testSubstituteVariables()
{
    QString filePath = "/home/user/project/main.cpp";
    
    QString result1 = FormatTemplateManager::substituteVariables("${file}", filePath);
    QCOMPARE(result1, filePath);
    
    QString result2 = FormatTemplateManager::substituteVariables("${fileDir}", filePath);
    QCOMPARE(result2, QString("/home/user/project"));
    
    QString result3 = FormatTemplateManager::substituteVariables("${fileBasename}", filePath);
    QCOMPARE(result3, QString("main.cpp"));
    
    QString result4 = FormatTemplateManager::substituteVariables("${fileBasenameNoExt}", filePath);
    QCOMPARE(result4, QString("main"));
    
    QString result5 = FormatTemplateManager::substituteVariables("${fileExt}", filePath);
    QCOMPARE(result5, QString("cpp"));
}

void TestFormatTemplateManager::testSubstituteVariablesWithComplexPath()
{
    QString filePath = "/home/user/my-project/src/hello_world.py";
    
    QString result = FormatTemplateManager::substituteVariables(
        "black --line-length 88 ${file}", filePath);
    QCOMPARE(result, QString("black --line-length 88 /home/user/my-project/src/hello_world.py"));
}

void TestFormatTemplateManager::testParseTemplateFromJson()
{
    // Load templates
    FormatTemplateManager& manager = FormatTemplateManager::instance();
    manager.loadTemplates();
    
    // Verify some templates exist
    QList<FormatTemplate> templates = manager.getAllTemplates();
    QVERIFY(!templates.isEmpty());
    
    // Check for Black template (Python formatter)
    bool foundBlack = false;
    for (const FormatTemplate& tmpl : templates) {
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

void TestFormatTemplateManager::testGetTemplatesForExtension()
{
    FormatTemplateManager& manager = FormatTemplateManager::instance();
    manager.loadTemplates();
    
    // Python files
    QList<FormatTemplate> pyTemplates = manager.getTemplatesForExtension("py");
    QVERIFY(!pyTemplates.isEmpty());
    
    bool foundPythonFormatter = false;
    for (const FormatTemplate& tmpl : pyTemplates) {
        if (tmpl.language == "Python") {
            foundPythonFormatter = true;
            break;
        }
    }
    QVERIFY(foundPythonFormatter);
    
    // C++ files
    QList<FormatTemplate> cppTemplates = manager.getTemplatesForExtension("cpp");
    QVERIFY(!cppTemplates.isEmpty());
    
    // Unknown extension
    QList<FormatTemplate> unknownTemplates = manager.getTemplatesForExtension("xyz123");
    QVERIFY(unknownTemplates.isEmpty());
}

void TestFormatTemplateManager::testGetTemplateById()
{
    FormatTemplateManager& manager = FormatTemplateManager::instance();
    manager.loadTemplates();
    
    FormatTemplate tmpl = manager.getTemplateById("black");
    QVERIFY(tmpl.isValid());
    QCOMPARE(tmpl.name, QString("Black"));
    
    FormatTemplate invalidTmpl = manager.getTemplateById("nonexistent_id");
    QVERIFY(!invalidTmpl.isValid());
}

void TestFormatTemplateManager::testAssignmentPersistence()
{
    FormatTemplateManager& manager = FormatTemplateManager::instance();
    manager.loadTemplates();
    
    // Create a test file in temp directory
    QString testFile = m_tempDir.path() + "/test.py";
    QFile file(testFile);
    file.open(QIODevice::WriteOnly);
    file.write("print('hello')");
    file.close();
    
    // Assign a template
    bool assigned = manager.assignTemplateToFile(testFile, "black", QStringList() << "--line-length" << "120");
    QVERIFY(assigned);
    
    // Verify assignment exists
    FileFormatAssignment assignment = manager.getAssignmentForFile(testFile);
    QCOMPARE(assignment.templateId, QString("black"));
    QVERIFY(assignment.customArgs.contains("--line-length"));
    
    // Check that config file was created
    QString configFile = m_tempDir.path() + "/.lightpad/format_config.json";
    QVERIFY(QFile::exists(configFile));
    
    // Remove assignment
    bool removed = manager.removeAssignment(testFile);
    QVERIFY(removed);
    
    // Verify assignment is gone
    FileFormatAssignment removedAssignment = manager.getAssignmentForFile(testFile);
    QVERIFY(removedAssignment.templateId.isEmpty());
}

void TestFormatTemplateManager::testBuildCommand()
{
    FormatTemplateManager& manager = FormatTemplateManager::instance();
    manager.loadTemplates();
    
    QString testFile = m_tempDir.path() + "/hello.py";
    QFile file(testFile);
    file.open(QIODevice::WriteOnly);
    file.write("print('hello')");
    file.close();
    
    // Build command without explicit assignment (should use default for .py)
    QPair<QString, QStringList> cmd = manager.buildCommand(testFile);
    QVERIFY(!cmd.first.isEmpty());
    // Command should be black or another Python formatter
    QVERIFY(cmd.first.contains("black") || cmd.first.contains("autopep8") || cmd.first.contains("yapf"));
}

void TestFormatTemplateManager::testEmptyFilePath()
{
    FormatTemplateManager& manager = FormatTemplateManager::instance();
    
    // Empty file path should return empty command
    QPair<QString, QStringList> cmd = manager.buildCommand("");
    QVERIFY(cmd.first.isEmpty());
    
    // Empty file path assignment should return empty
    FileFormatAssignment assignment = manager.getAssignmentForFile("");
    QVERIFY(assignment.templateId.isEmpty());
}

void TestFormatTemplateManager::testHasFormatTemplate()
{
    FormatTemplateManager& manager = FormatTemplateManager::instance();
    manager.loadTemplates();
    
    QString testFile = m_tempDir.path() + "/test.cpp";
    QFile file(testFile);
    file.open(QIODevice::WriteOnly);
    file.write("int main() {}");
    file.close();
    
    // Should have a template for .cpp files
    QVERIFY(manager.hasFormatTemplate(testFile));
    
    // Should not have template for unknown extension
    QString unknownFile = m_tempDir.path() + "/test.xyz123";
    QFile unknownFileHandle(unknownFile);
    unknownFileHandle.open(QIODevice::WriteOnly);
    unknownFileHandle.write("test");
    unknownFileHandle.close();
    
    QVERIFY(!manager.hasFormatTemplate(unknownFile));
    
    // Empty path should return false
    QVERIFY(!manager.hasFormatTemplate(""));
}

QTEST_MAIN(TestFormatTemplateManager)
#include "test_formattemplatemanager.moc"
