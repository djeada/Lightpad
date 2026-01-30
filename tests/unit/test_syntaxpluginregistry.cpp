#include <QtTest/QtTest>
#include "syntax/syntaxpluginregistry.h"
#include "syntax/cppsyntaxplugin.h"
#include "syntax/javascriptsyntaxplugin.h"
#include "syntax/pythonsyntaxplugin.h"
#include <memory>

class TestSyntaxPluginRegistry : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void testSingletonInstance();
    void testRegisterPlugin();
    void testGetPluginByLanguageId();
    void testGetPluginByExtension();
    void testGetAllLanguageIds();
    void testGetAllExtensions();
    void testIsLanguageSupported();
    void testIsExtensionSupported();
    void testPluginReplacement();
};

void TestSyntaxPluginRegistry::init()
{
    // Clear registry before each test
    SyntaxPluginRegistry::instance().clear();
}

void TestSyntaxPluginRegistry::cleanup()
{
    // Clear registry after each test
    SyntaxPluginRegistry::instance().clear();
}

void TestSyntaxPluginRegistry::testSingletonInstance()
{
    SyntaxPluginRegistry& reg1 = SyntaxPluginRegistry::instance();
    SyntaxPluginRegistry& reg2 = SyntaxPluginRegistry::instance();
    QCOMPARE(&reg1, &reg2);
}

void TestSyntaxPluginRegistry::testRegisterPlugin()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Register a C++ plugin
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    
    // Check that it was registered
    QVERIFY(registry.isLanguageSupported("cpp"));
    QVERIFY(registry.getPluginByLanguageId("cpp") != nullptr);
}

void TestSyntaxPluginRegistry::testGetPluginByLanguageId()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Register plugins
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());
    
    // Get by language ID
    ISyntaxPlugin* cppPlugin = registry.getPluginByLanguageId("cpp");
    QVERIFY(cppPlugin != nullptr);
    QCOMPARE(cppPlugin->languageId(), QString("cpp"));
    QCOMPARE(cppPlugin->languageName(), QString("C++"));
    
    ISyntaxPlugin* jsPlugin = registry.getPluginByLanguageId("js");
    QVERIFY(jsPlugin != nullptr);
    QCOMPARE(jsPlugin->languageId(), QString("js"));
    
    // Non-existent language
    ISyntaxPlugin* rustPlugin = registry.getPluginByLanguageId("rust");
    QVERIFY(rustPlugin == nullptr);
}

void TestSyntaxPluginRegistry::testGetPluginByExtension()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Register plugins
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<PythonSyntaxPlugin>());
    
    // Get by extension
    ISyntaxPlugin* cppPlugin = registry.getPluginByExtension("cpp");
    QVERIFY(cppPlugin != nullptr);
    QCOMPARE(cppPlugin->languageId(), QString("cpp"));
    
    ISyntaxPlugin* hPlugin = registry.getPluginByExtension("h");
    QVERIFY(hPlugin != nullptr);
    QCOMPARE(hPlugin->languageId(), QString("cpp"));
    
    ISyntaxPlugin* pyPlugin = registry.getPluginByExtension("py");
    QVERIFY(pyPlugin != nullptr);
    QCOMPARE(pyPlugin->languageId(), QString("py"));
    
    // Test with leading dot
    ISyntaxPlugin* pyPlugin2 = registry.getPluginByExtension(".py");
    QVERIFY(pyPlugin2 != nullptr);
    QCOMPARE(pyPlugin2->languageId(), QString("py"));
    
    // Non-existent extension
    ISyntaxPlugin* rsPlugin = registry.getPluginByExtension("rs");
    QVERIFY(rsPlugin == nullptr);
}

void TestSyntaxPluginRegistry::testGetAllLanguageIds()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Initially empty
    QVERIFY(registry.getAllLanguageIds().isEmpty());
    
    // Register plugins
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<PythonSyntaxPlugin>());
    
    QStringList ids = registry.getAllLanguageIds();
    QCOMPARE(ids.size(), 3);
    QVERIFY(ids.contains("cpp"));
    QVERIFY(ids.contains("js"));
    QVERIFY(ids.contains("py"));
}

void TestSyntaxPluginRegistry::testGetAllExtensions()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Initially empty
    QVERIFY(registry.getAllExtensions().isEmpty());
    
    // Register C++ plugin (has multiple extensions)
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    
    QStringList extensions = registry.getAllExtensions();
    QVERIFY(extensions.size() >= 3); // At least cpp, h, hpp
    QVERIFY(extensions.contains("cpp"));
    QVERIFY(extensions.contains("h"));
}

void TestSyntaxPluginRegistry::testIsLanguageSupported()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Initially not supported
    QVERIFY(!registry.isLanguageSupported("cpp"));
    
    // Register plugin
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    
    // Now supported
    QVERIFY(registry.isLanguageSupported("cpp"));
    QVERIFY(!registry.isLanguageSupported("rust"));
}

void TestSyntaxPluginRegistry::testIsExtensionSupported()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Initially not supported
    QVERIFY(!registry.isExtensionSupported("cpp"));
    
    // Register plugin
    registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());
    
    // Now supported
    QVERIFY(registry.isExtensionSupported("js"));
    QVERIFY(registry.isExtensionSupported("jsx"));
    QVERIFY(!registry.isExtensionSupported("cpp"));
}

void TestSyntaxPluginRegistry::testPluginReplacement()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Register a plugin
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    ISyntaxPlugin* plugin1 = registry.getPluginByLanguageId("cpp");
    QVERIFY(plugin1 != nullptr);
    
    // Register another plugin with same language ID (should replace)
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    ISyntaxPlugin* plugin2 = registry.getPluginByLanguageId("cpp");
    QVERIFY(plugin2 != nullptr);
    
    // Should still have only one cpp plugin
    QCOMPARE(registry.getAllLanguageIds().size(), 1);
}

QTEST_MAIN(TestSyntaxPluginRegistry)
#include "test_syntaxpluginregistry.moc"
