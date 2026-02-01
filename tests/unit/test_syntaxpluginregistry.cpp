#include <QtTest/QtTest>
#include "syntax/syntaxpluginregistry.h"
#include "syntax/cppsyntaxplugin.h"
#include "syntax/csssyntaxplugin.h"
#include "syntax/gosyntaxplugin.h"
#include "syntax/htmlsyntaxplugin.h"
#include "syntax/javascriptsyntaxplugin.h"
#include "syntax/javasyntaxplugin.h"
#include "syntax/jsonsyntaxplugin.h"
#include "syntax/markdownsyntaxplugin.h"
#include "syntax/pythonsyntaxplugin.h"
#include "syntax/rustsyntaxplugin.h"
#include "syntax/shellsyntaxplugin.h"
#include "syntax/typescriptsyntaxplugin.h"
#include "syntax/yamlsyntaxplugin.h"
#include <QRegularExpression>
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
    void testAllBuiltInPlugins();
    void testCppPreprocessorAndScopePatterns();
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
    registry.registerPlugin(std::make_unique<RustSyntaxPlugin>());
    
    // Get by language ID
    ISyntaxPlugin* cppPlugin = registry.getPluginByLanguageId("cpp");
    QVERIFY(cppPlugin != nullptr);
    QCOMPARE(cppPlugin->languageId(), QString("cpp"));
    QCOMPARE(cppPlugin->languageName(), QString("C++"));
    
    ISyntaxPlugin* jsPlugin = registry.getPluginByLanguageId("js");
    QVERIFY(jsPlugin != nullptr);
    QCOMPARE(jsPlugin->languageId(), QString("js"));
    
    ISyntaxPlugin* rustPlugin = registry.getPluginByLanguageId("rust");
    QVERIFY(rustPlugin != nullptr);
    QCOMPARE(rustPlugin->languageId(), QString("rust"));
    QCOMPARE(rustPlugin->languageName(), QString("Rust"));
    
    // Non-existent language
    ISyntaxPlugin* luaPlugin = registry.getPluginByLanguageId("lua");
    QVERIFY(luaPlugin == nullptr);
}

void TestSyntaxPluginRegistry::testGetPluginByExtension()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Register plugins
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<PythonSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<RustSyntaxPlugin>());
    
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
    
    // Test rust extension
    ISyntaxPlugin* rsPlugin = registry.getPluginByExtension("rs");
    QVERIFY(rsPlugin != nullptr);
    QCOMPARE(rsPlugin->languageId(), QString("rust"));
    
    // Non-existent extension
    ISyntaxPlugin* luaPlugin = registry.getPluginByExtension("lua");
    QVERIFY(luaPlugin == nullptr);
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
    QVERIFY(!registry.isLanguageSupported("lua"));
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

void TestSyntaxPluginRegistry::testAllBuiltInPlugins()
{
    auto& registry = SyntaxPluginRegistry::instance();
    
    // Register all built-in plugins
    registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<CssSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<GoSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<HtmlSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<JavaSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<JsonSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<MarkdownSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<PythonSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<RustSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<ShellSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<TypeScriptSyntaxPlugin>());
    registry.registerPlugin(std::make_unique<YamlSyntaxPlugin>());
    
    // Verify all 13 languages are registered
    QCOMPARE(registry.getAllLanguageIds().size(), 13);
    
    // Test each plugin has valid language ID and name
    QVERIFY(registry.isLanguageSupported("cpp"));
    QVERIFY(registry.isLanguageSupported("css"));
    QVERIFY(registry.isLanguageSupported("go"));
    QVERIFY(registry.isLanguageSupported("html"));
    QVERIFY(registry.isLanguageSupported("js"));
    QVERIFY(registry.isLanguageSupported("java"));
    QVERIFY(registry.isLanguageSupported("json"));
    QVERIFY(registry.isLanguageSupported("md"));
    QVERIFY(registry.isLanguageSupported("py"));
    QVERIFY(registry.isLanguageSupported("rust"));
    QVERIFY(registry.isLanguageSupported("sh"));
    QVERIFY(registry.isLanguageSupported("ts"));
    QVERIFY(registry.isLanguageSupported("yaml"));
    
    // Test file extension mappings
    QVERIFY(registry.isExtensionSupported("cpp"));
    QVERIFY(registry.isExtensionSupported("css"));
    QVERIFY(registry.isExtensionSupported("go"));
    QVERIFY(registry.isExtensionSupported("html"));
    QVERIFY(registry.isExtensionSupported("js"));
    QVERIFY(registry.isExtensionSupported("java"));
    QVERIFY(registry.isExtensionSupported("json"));
    QVERIFY(registry.isExtensionSupported("md"));
    QVERIFY(registry.isExtensionSupported("py"));
    QVERIFY(registry.isExtensionSupported("rs"));
    QVERIFY(registry.isExtensionSupported("sh"));
    QVERIFY(registry.isExtensionSupported("ts"));
    QVERIFY(registry.isExtensionSupported("yaml"));
    QVERIFY(registry.isExtensionSupported("yml"));
    
    // Verify each plugin returns valid syntax rules and keywords
    for (const QString& langId : registry.getAllLanguageIds()) {
        ISyntaxPlugin* plugin = registry.getPluginByLanguageId(langId);
        QVERIFY(plugin != nullptr);
        QVERIFY(!plugin->languageName().isEmpty());
        QVERIFY(!plugin->fileExtensions().isEmpty());
        QVERIFY(!plugin->syntaxRules().isEmpty());
    }
}

void TestSyntaxPluginRegistry::testCppPreprocessorAndScopePatterns()
{
    CppSyntaxPlugin plugin;
    auto rules = plugin.syntaxRules();
    QRegularExpression preprocessorPattern;
    QRegularExpression scopeQualifierPattern;
    QRegularExpression scopedIdentifierPattern;
    bool foundPreprocessor = false;
    bool foundScopeQualifier = false;
    bool foundScopedIdentifier = false;

    for (const auto& rule : rules) {
        const QString pattern = rule.pattern.pattern();
        if (rule.name == "preprocessor_directive") {
            preprocessorPattern = rule.pattern;
            foundPreprocessor = true;
        }
        if (rule.name == "scope_qualifier" && pattern.contains("(?=::)")) {
            scopeQualifierPattern = rule.pattern;
            foundScopeQualifier = true;
        }
        if (rule.name == "scoped_identifier" && pattern.contains("(?<=::)")) {
            scopedIdentifierPattern = rule.pattern;
            foundScopedIdentifier = true;
        }
    }

    QVERIFY2(foundPreprocessor, "Missing preprocessor rule in C++ syntax rules.");
    QVERIFY2(foundScopeQualifier, "Missing scope qualifier rule in C++ syntax rules.");
    QVERIFY2(foundScopedIdentifier, "Missing scoped identifier rule in C++ syntax rules.");

    QVERIFY(preprocessorPattern.isValid());
    QVERIFY(scopeQualifierPattern.isValid());
    QVERIFY(scopedIdentifierPattern.isValid());

    QVERIFY(preprocessorPattern.globalMatch("#include <iostream>").hasNext());
    QVERIFY(scopeQualifierPattern.globalMatch("std::vector").hasNext());
    QVERIFY(scopedIdentifierPattern.globalMatch("std::vector").hasNext());
}

QTEST_MAIN(TestSyntaxPluginRegistry)
#include "test_syntaxpluginregistry.moc"
