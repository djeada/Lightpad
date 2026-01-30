#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFile>

#include "run_templates/runtemplatemanager.h"

class TestRunTemplateManager : public QObject
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

private:
    QTemporaryDir m_tempDir;
};

void TestRunTemplateManager::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestRunTemplateManager::cleanupTestCase()
{
}

void TestRunTemplateManager::testSubstituteVariables()
{
    QString filePath = "/home/user/project/main.py";
    
    QString result1 = RunTemplateManager::substituteVariables("${file}", filePath);
    QCOMPARE(result1, filePath);
    
    QString result2 = RunTemplateManager::substituteVariables("${fileDir}", filePath);
    QCOMPARE(result2, QString("/home/user/project"));
    
    QString result3 = RunTemplateManager::substituteVariables("${fileBasename}", filePath);
    QCOMPARE(result3, QString("main.py"));
    
    QString result4 = RunTemplateManager::substituteVariables("${fileBasenameNoExt}", filePath);
    QCOMPARE(result4, QString("main"));
    
    QString result5 = RunTemplateManager::substituteVariables("${fileExt}", filePath);
    QCOMPARE(result5, QString("py"));
}

void TestRunTemplateManager::testSubstituteVariablesWithComplexPath()
{
    QString filePath = "/home/user/my-project/src/hello_world.cpp";
    
    QString result = RunTemplateManager::substituteVariables(
        "g++ -o ${fileBasenameNoExt} ${file}", filePath);
    QCOMPARE(result, QString("g++ -o hello_world /home/user/my-project/src/hello_world.cpp"));
}

void TestRunTemplateManager::testParseTemplateFromJson()
{
    // Load templates
    RunTemplateManager& manager = RunTemplateManager::instance();
    manager.loadTemplates();
    
    // Verify some templates exist
    QList<RunTemplate> templates = manager.getAllTemplates();
    QVERIFY(!templates.isEmpty());
    
    // Check for Python template
    bool foundPython = false;
    for (const RunTemplate& tmpl : templates) {
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

void TestRunTemplateManager::testGetTemplatesForExtension()
{
    RunTemplateManager& manager = RunTemplateManager::instance();
    manager.loadTemplates();
    
    // Python files
    QList<RunTemplate> pyTemplates = manager.getTemplatesForExtension("py");
    QVERIFY(!pyTemplates.isEmpty());
    
    bool foundPython = false;
    for (const RunTemplate& tmpl : pyTemplates) {
        if (tmpl.language == "Python") {
            foundPython = true;
            break;
        }
    }
    QVERIFY(foundPython);
    
    // C++ files
    QList<RunTemplate> cppTemplates = manager.getTemplatesForExtension("cpp");
    QVERIFY(!cppTemplates.isEmpty());
    
    // Unknown extension
    QList<RunTemplate> unknownTemplates = manager.getTemplatesForExtension("xyz123");
    QVERIFY(unknownTemplates.isEmpty());
}

void TestRunTemplateManager::testGetTemplateById()
{
    RunTemplateManager& manager = RunTemplateManager::instance();
    manager.loadTemplates();
    
    RunTemplate tmpl = manager.getTemplateById("python3");
    QVERIFY(tmpl.isValid());
    QCOMPARE(tmpl.name, QString("Python 3"));
    
    RunTemplate invalidTmpl = manager.getTemplateById("nonexistent_id");
    QVERIFY(!invalidTmpl.isValid());
}

void TestRunTemplateManager::testAssignmentPersistence()
{
    RunTemplateManager& manager = RunTemplateManager::instance();
    manager.loadTemplates();
    
    // Create a test file in temp directory
    QString testFile = m_tempDir.path() + "/test.py";
    QFile file(testFile);
    file.open(QIODevice::WriteOnly);
    file.write("print('hello')");
    file.close();
    
    // Assign a template
    bool assigned = manager.assignTemplateToFile(testFile, "python3", QStringList() << "-v");
    QVERIFY(assigned);
    
    // Verify assignment exists
    FileTemplateAssignment assignment = manager.getAssignmentForFile(testFile);
    QCOMPARE(assignment.templateId, QString("python3"));
    QVERIFY(assignment.customArgs.contains("-v"));
    
    // Check that config file was created
    QString configFile = m_tempDir.path() + "/.lightpad/run_config.json";
    QVERIFY(QFile::exists(configFile));
    
    // Remove assignment
    bool removed = manager.removeAssignment(testFile);
    QVERIFY(removed);
    
    // Verify assignment is gone
    FileTemplateAssignment removedAssignment = manager.getAssignmentForFile(testFile);
    QVERIFY(removedAssignment.templateId.isEmpty());
}

void TestRunTemplateManager::testBuildCommand()
{
    RunTemplateManager& manager = RunTemplateManager::instance();
    manager.loadTemplates();
    
    QString testFile = m_tempDir.path() + "/hello.py";
    QFile file(testFile);
    file.open(QIODevice::WriteOnly);
    file.write("print('hello')");
    file.close();
    
    // Build command without explicit assignment (should use default for .py)
    QPair<QString, QStringList> cmd = manager.buildCommand(testFile);
    QVERIFY(!cmd.first.isEmpty());
    // Command should be python3 (or similar)
    QVERIFY(cmd.first.contains("python"));
}

void TestRunTemplateManager::testEmptyFilePath()
{
    RunTemplateManager& manager = RunTemplateManager::instance();
    
    // Empty file path should return empty command
    QPair<QString, QStringList> cmd = manager.buildCommand("");
    QVERIFY(cmd.first.isEmpty());
    
    // Empty file path assignment should return empty
    FileTemplateAssignment assignment = manager.getAssignmentForFile("");
    QVERIFY(assignment.templateId.isEmpty());
}

QTEST_MAIN(TestRunTemplateManager)
#include "test_runtemplatemanager.moc"
