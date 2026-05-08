#include "language/languagecatalog.h"
#include "syntax/bazelsyntaxplugin.h"
#include "syntax/cmakesyntaxplugin.h"
#include "syntax/cppsyntaxplugin.h"
#include "syntax/csssyntaxplugin.h"
#include "syntax/dockerfilesyntaxplugin.h"
#include "syntax/glslsyntaxplugin.h"
#include "syntax/gosyntaxplugin.h"
#include "syntax/hlslsyntaxplugin.h"
#include "syntax/htmlsyntaxplugin.h"
#include "syntax/javascriptsyntaxplugin.h"
#include "syntax/javasyntaxplugin.h"
#include "syntax/jsonsyntaxplugin.h"
#include "syntax/latexsyntaxplugin.h"
#include "syntax/makesyntaxplugin.h"
#include "syntax/markdownsyntaxplugin.h"
#include "syntax/mesonsyntaxplugin.h"
#include "syntax/metalsyntaxplugin.h"
#include "syntax/ninjasyntaxplugin.h"
#include "syntax/pythonsyntaxplugin.h"
#include "syntax/rustsyntaxplugin.h"
#include "syntax/shellsyntaxplugin.h"
#include "syntax/syntaxpluginregistry.h"
#include "syntax/typescriptsyntaxplugin.h"
#include "syntax/wgslsyntaxplugin.h"
#include "syntax/yamlsyntaxplugin.h"
#include <QRegularExpression>
#include <QtTest/QtTest>
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
  void testLanguageCatalogIncludesLatex();
  void testGlslPlugin();
  void testHlslPlugin();
  void testWgslPlugin();
  void testMetalPlugin();
};

void TestSyntaxPluginRegistry::init() {

  SyntaxPluginRegistry::instance().clear();
}

void TestSyntaxPluginRegistry::cleanup() {

  SyntaxPluginRegistry::instance().clear();
}

void TestSyntaxPluginRegistry::testSingletonInstance() {
  SyntaxPluginRegistry &reg1 = SyntaxPluginRegistry::instance();
  SyntaxPluginRegistry &reg2 = SyntaxPluginRegistry::instance();
  QCOMPARE(&reg1, &reg2);
}

void TestSyntaxPluginRegistry::testRegisterPlugin() {
  auto &registry = SyntaxPluginRegistry::instance();

  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());

  QVERIFY(registry.isLanguageSupported("cpp"));
  QVERIFY(registry.getPluginByLanguageId("cpp") != nullptr);
}

void TestSyntaxPluginRegistry::testGetPluginByLanguageId() {
  auto &registry = SyntaxPluginRegistry::instance();

  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<RustSyntaxPlugin>());

  ISyntaxPlugin *cppPlugin = registry.getPluginByLanguageId("cpp");
  QVERIFY(cppPlugin != nullptr);
  QCOMPARE(cppPlugin->languageId(), QString("cpp"));
  QCOMPARE(cppPlugin->languageName(), QString("C++"));

  ISyntaxPlugin *jsPlugin = registry.getPluginByLanguageId("js");
  QVERIFY(jsPlugin != nullptr);
  QCOMPARE(jsPlugin->languageId(), QString("js"));

  ISyntaxPlugin *rustPlugin = registry.getPluginByLanguageId("rust");
  QVERIFY(rustPlugin != nullptr);
  QCOMPARE(rustPlugin->languageId(), QString("rust"));
  QCOMPARE(rustPlugin->languageName(), QString("Rust"));

  ISyntaxPlugin *luaPlugin = registry.getPluginByLanguageId("lua");
  QVERIFY(luaPlugin == nullptr);
}

void TestSyntaxPluginRegistry::testGetPluginByExtension() {
  auto &registry = SyntaxPluginRegistry::instance();

  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<PythonSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<RustSyntaxPlugin>());

  ISyntaxPlugin *cppPlugin = registry.getPluginByExtension("cpp");
  QVERIFY(cppPlugin != nullptr);
  QCOMPARE(cppPlugin->languageId(), QString("cpp"));

  ISyntaxPlugin *hPlugin = registry.getPluginByExtension("h");
  QVERIFY(hPlugin != nullptr);
  QCOMPARE(hPlugin->languageId(), QString("cpp"));

  ISyntaxPlugin *pyPlugin = registry.getPluginByExtension("py");
  QVERIFY(pyPlugin != nullptr);
  QCOMPARE(pyPlugin->languageId(), QString("py"));

  ISyntaxPlugin *pyPlugin2 = registry.getPluginByExtension(".py");
  QVERIFY(pyPlugin2 != nullptr);
  QCOMPARE(pyPlugin2->languageId(), QString("py"));

  ISyntaxPlugin *rsPlugin = registry.getPluginByExtension("rs");
  QVERIFY(rsPlugin != nullptr);
  QCOMPARE(rsPlugin->languageId(), QString("rust"));

  ISyntaxPlugin *luaPlugin = registry.getPluginByExtension("lua");
  QVERIFY(luaPlugin == nullptr);
}

void TestSyntaxPluginRegistry::testGetAllLanguageIds() {
  auto &registry = SyntaxPluginRegistry::instance();

  QVERIFY(registry.getAllLanguageIds().isEmpty());

  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<PythonSyntaxPlugin>());

  QStringList ids = registry.getAllLanguageIds();
  QCOMPARE(ids.size(), 3);
  QVERIFY(ids.contains("cpp"));
  QVERIFY(ids.contains("js"));
  QVERIFY(ids.contains("py"));
}

void TestSyntaxPluginRegistry::testGetAllExtensions() {
  auto &registry = SyntaxPluginRegistry::instance();

  QVERIFY(registry.getAllExtensions().isEmpty());

  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());

  QStringList extensions = registry.getAllExtensions();
  QVERIFY(extensions.size() >= 3);
  QVERIFY(extensions.contains("cpp"));
  QVERIFY(extensions.contains("h"));
}

void TestSyntaxPluginRegistry::testIsLanguageSupported() {
  auto &registry = SyntaxPluginRegistry::instance();

  QVERIFY(!registry.isLanguageSupported("cpp"));

  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());

  QVERIFY(registry.isLanguageSupported("cpp"));
  QVERIFY(!registry.isLanguageSupported("lua"));
}

void TestSyntaxPluginRegistry::testIsExtensionSupported() {
  auto &registry = SyntaxPluginRegistry::instance();

  QVERIFY(!registry.isExtensionSupported("cpp"));

  registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());

  QVERIFY(registry.isExtensionSupported("js"));
  QVERIFY(registry.isExtensionSupported("jsx"));
  QVERIFY(!registry.isExtensionSupported("cpp"));
}

void TestSyntaxPluginRegistry::testPluginReplacement() {
  auto &registry = SyntaxPluginRegistry::instance();

  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
  ISyntaxPlugin *plugin1 = registry.getPluginByLanguageId("cpp");
  QVERIFY(plugin1 != nullptr);

  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
  ISyntaxPlugin *plugin2 = registry.getPluginByLanguageId("cpp");
  QVERIFY(plugin2 != nullptr);

  QCOMPARE(registry.getAllLanguageIds().size(), 1);
}

void TestSyntaxPluginRegistry::testAllBuiltInPlugins() {
  auto &registry = SyntaxPluginRegistry::instance();

  registry.registerPlugin(std::make_unique<BazelSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<CppSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<CssSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<DockerfileSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<GlslSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<GoSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<HlslSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<HtmlSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<JavaScriptSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<JavaSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<JsonSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<LatexSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<MakeSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<MarkdownSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<MesonSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<MetalSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<NinjaSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<CMakeSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<PythonSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<RustSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<ShellSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<TypeScriptSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<WgslSyntaxPlugin>());
  registry.registerPlugin(std::make_unique<YamlSyntaxPlugin>());

  QCOMPARE(registry.getAllLanguageIds().size(), 24);

  QVERIFY(registry.isLanguageSupported("bazel"));
  QVERIFY(registry.isLanguageSupported("cpp"));
  QVERIFY(registry.isLanguageSupported("css"));
  QVERIFY(registry.isLanguageSupported("dockerfile"));
  QVERIFY(registry.isLanguageSupported("glsl"));
  QVERIFY(registry.isLanguageSupported("go"));
  QVERIFY(registry.isLanguageSupported("hlsl"));
  QVERIFY(registry.isLanguageSupported("html"));
  QVERIFY(registry.isLanguageSupported("js"));
  QVERIFY(registry.isLanguageSupported("java"));
  QVERIFY(registry.isLanguageSupported("json"));
  QVERIFY(registry.isLanguageSupported("latex"));
  QVERIFY(registry.isLanguageSupported("make"));
  QVERIFY(registry.isLanguageSupported("md"));
  QVERIFY(registry.isLanguageSupported("meson"));
  QVERIFY(registry.isLanguageSupported("metal"));
  QVERIFY(registry.isLanguageSupported("ninja"));
  QVERIFY(registry.isLanguageSupported("cmake"));
  QVERIFY(registry.isLanguageSupported("py"));
  QVERIFY(registry.isLanguageSupported("rust"));
  QVERIFY(registry.isLanguageSupported("sh"));
  QVERIFY(registry.isLanguageSupported("ts"));
  QVERIFY(registry.isLanguageSupported("wgsl"));
  QVERIFY(registry.isLanguageSupported("yaml"));

  QVERIFY(registry.isExtensionSupported("bzl"));
  QVERIFY(registry.isExtensionSupported("cpp"));
  QVERIFY(registry.isExtensionSupported("css"));
  QVERIFY(registry.isExtensionSupported("dockerfile"));
  QVERIFY(registry.isExtensionSupported("containerfile"));
  QCOMPARE(registry.getPluginByExtension("dockerfile")->languageId(),
           QString("dockerfile"));
  QCOMPARE(registry.getPluginByExtension("containerfile")->languageId(),
           QString("dockerfile"));
  QVERIFY(registry.isExtensionSupported("glsl"));
  QVERIFY(registry.isExtensionSupported("vert"));
  QVERIFY(registry.isExtensionSupported("frag"));
  QVERIFY(registry.isExtensionSupported("hlsl"));
  QVERIFY(registry.isExtensionSupported("fx"));
  QVERIFY(registry.isExtensionSupported("go"));
  QVERIFY(registry.isExtensionSupported("html"));
  QVERIFY(registry.isExtensionSupported("js"));
  QVERIFY(registry.isExtensionSupported("java"));
  QVERIFY(registry.isExtensionSupported("json"));
  QVERIFY(registry.isExtensionSupported("tex"));
  QVERIFY(registry.isExtensionSupported("bib"));
  QVERIFY(registry.isExtensionSupported("mk"));
  QVERIFY(registry.isExtensionSupported("makefile"));
  QVERIFY(registry.isExtensionSupported("md"));
  QVERIFY(registry.isExtensionSupported("metal"));
  QVERIFY(registry.isExtensionSupported("meson"));
  QVERIFY(registry.isExtensionSupported("ninja"));
  QVERIFY(registry.isExtensionSupported("cmake"));
  QVERIFY(registry.isExtensionSupported("cmakelists.txt"));
  QVERIFY(registry.isExtensionSupported("py"));
  QVERIFY(registry.isExtensionSupported("rs"));
  QVERIFY(registry.isExtensionSupported("sh"));
  QVERIFY(registry.isExtensionSupported("ts"));
  QVERIFY(registry.isExtensionSupported("wgsl"));
  QVERIFY(registry.isExtensionSupported("yaml"));
  QVERIFY(registry.isExtensionSupported("yml"));

  for (const QString &langId : registry.getAllLanguageIds()) {
    ISyntaxPlugin *plugin = registry.getPluginByLanguageId(langId);
    QVERIFY(plugin != nullptr);
    QVERIFY(!plugin->languageName().isEmpty());
    QVERIFY(!plugin->fileExtensions().isEmpty());
    QVERIFY(!plugin->syntaxRules().isEmpty());
  }
}

void TestSyntaxPluginRegistry::testCppPreprocessorAndScopePatterns() {
  CppSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();
  QRegularExpression preprocessorPattern;
  QRegularExpression scopeQualifierPattern;
  QRegularExpression scopedIdentifierPattern;
  bool foundPreprocessor = false;
  bool foundScopeQualifier = false;
  bool foundScopedIdentifier = false;

  for (const auto &rule : rules) {
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
  QVERIFY2(foundScopeQualifier,
           "Missing scope qualifier rule in C++ syntax rules.");
  QVERIFY2(foundScopedIdentifier,
           "Missing scoped identifier rule in C++ syntax rules.");

  QVERIFY(preprocessorPattern.isValid());
  QVERIFY(scopeQualifierPattern.isValid());
  QVERIFY(scopedIdentifierPattern.isValid());

  QVERIFY(preprocessorPattern.globalMatch("#include <iostream>").hasNext());
  QVERIFY(scopeQualifierPattern.globalMatch("std::vector").hasNext());
  QVERIFY(scopedIdentifierPattern.globalMatch("std::vector").hasNext());
}

void TestSyntaxPluginRegistry::testLanguageCatalogIncludesLatex() {
  const auto languages = LanguageCatalog::builtInLanguages();
  auto latexIt =
      std::find_if(languages.begin(), languages.end(),
                   [](const LanguageInfo &info) { return info.id == "latex"; });

  QVERIFY(latexIt != languages.end());
  QCOMPARE(latexIt->displayName, QString("LaTeX"));
  QVERIFY(latexIt->extensions.contains("tex"));
  QVERIFY(latexIt->extensions.contains("bib"));
  QVERIFY(latexIt->extensions.contains("sty"));
  QVERIFY(latexIt->extensions.contains("cls"));
  QCOMPARE(LanguageCatalog::normalize("latex"), QString("latex"));
  QCOMPARE(LanguageCatalog::normalize(".tex"), QString("latex"));
  QCOMPARE(LanguageCatalog::normalize(".sty"), QString("latex"));
  QCOMPARE(LanguageCatalog::languageForExtension("bib"), QString("latex"));
  QCOMPARE(LanguageCatalog::displayName("latex"), QString("LaTeX"));
}

void TestSyntaxPluginRegistry::testGlslPlugin() {
  GlslSyntaxPlugin plugin;

  QCOMPARE(plugin.languageId(), QString("glsl"));
  QCOMPARE(plugin.languageName(), QString("GLSL"));
  QVERIFY(plugin.fileExtensions().contains("glsl"));
  QVERIFY(plugin.fileExtensions().contains("vert"));
  QVERIFY(plugin.fileExtensions().contains("frag"));
  QVERIFY(plugin.fileExtensions().contains("comp"));

  QVector<SyntaxRule> rules = plugin.syntaxRules();
  QVERIFY(!rules.isEmpty());

  // Verify type keywords are present
  QStringList keywords = plugin.keywords();
  QVERIFY(keywords.contains("vec4"));
  QVERIFY(keywords.contains("mat4"));
  QVERIFY(keywords.contains("sampler2D"));
  QVERIFY(keywords.contains("uniform"));
  QVERIFY(keywords.contains("gl_Position"));

  // Verify all rules have valid patterns
  for (const auto &rule : rules) {
    QVERIFY2(rule.pattern.isValid(),
             qPrintable(QString("Invalid pattern in GLSL rule: %1")
                            .arg(rule.pattern.errorString())));
  }

  // Verify comment and multiline blocks
  QVector<MultiLineBlock> blocks = plugin.multiLineBlocks();
  QVERIFY(!blocks.isEmpty());
}

void TestSyntaxPluginRegistry::testHlslPlugin() {
  HlslSyntaxPlugin plugin;

  QCOMPARE(plugin.languageId(), QString("hlsl"));
  QCOMPARE(plugin.languageName(), QString("HLSL"));
  QVERIFY(plugin.fileExtensions().contains("hlsl"));
  QVERIFY(plugin.fileExtensions().contains("fx"));
  QVERIFY(plugin.fileExtensions().contains("fxh"));

  QVector<SyntaxRule> rules = plugin.syntaxRules();
  QVERIFY(!rules.isEmpty());

  QStringList keywords = plugin.keywords();
  QVERIFY(keywords.contains("float4"));
  QVERIFY(keywords.contains("Texture2D"));
  QVERIFY(keywords.contains("cbuffer"));
  QVERIFY(keywords.contains("SamplerState"));
  QVERIFY(keywords.contains("SV_Position"));

  for (const auto &rule : rules) {
    QVERIFY2(rule.pattern.isValid(),
             qPrintable(QString("Invalid pattern in HLSL rule: %1")
                            .arg(rule.pattern.errorString())));
  }

  QVector<MultiLineBlock> blocks = plugin.multiLineBlocks();
  QVERIFY(!blocks.isEmpty());
}

void TestSyntaxPluginRegistry::testWgslPlugin() {
  WgslSyntaxPlugin plugin;

  QCOMPARE(plugin.languageId(), QString("wgsl"));
  QCOMPARE(plugin.languageName(), QString("WGSL"));
  QVERIFY(plugin.fileExtensions().contains("wgsl"));

  QVector<SyntaxRule> rules = plugin.syntaxRules();
  QVERIFY(!rules.isEmpty());

  QStringList keywords = plugin.keywords();
  QVERIFY(keywords.contains("vec4f"));
  QVERIFY(keywords.contains("fn"));
  QVERIFY(keywords.contains("var"));
  QVERIFY(keywords.contains("@vertex"));
  QVERIFY(keywords.contains("@fragment"));

  for (const auto &rule : rules) {
    QVERIFY2(rule.pattern.isValid(),
             qPrintable(QString("Invalid pattern in WGSL rule: %1")
                            .arg(rule.pattern.errorString())));
  }
}

void TestSyntaxPluginRegistry::testMetalPlugin() {
  MetalSyntaxPlugin plugin;

  QCOMPARE(plugin.languageId(), QString("metal"));
  QCOMPARE(plugin.languageName(), QString("Metal"));
  QVERIFY(plugin.fileExtensions().contains("metal"));

  QVector<SyntaxRule> rules = plugin.syntaxRules();
  QVERIFY(!rules.isEmpty());

  QStringList keywords = plugin.keywords();
  QVERIFY(keywords.contains("float4"));
  QVERIFY(keywords.contains("texture2d"));
  QVERIFY(keywords.contains("kernel"));
  QVERIFY(keywords.contains("vertex"));
  QVERIFY(keywords.contains("fragment"));
  QVERIFY(keywords.contains("constant"));
  QVERIFY(keywords.contains("device"));
  QVERIFY(keywords.contains("threadgroup"));

  for (const auto &rule : rules) {
    QVERIFY2(rule.pattern.isValid(),
             qPrintable(QString("Invalid pattern in Metal rule: %1")
                            .arg(rule.pattern.errorString())));
  }

  QVector<MultiLineBlock> blocks = plugin.multiLineBlocks();
  QVERIFY(!blocks.isEmpty());
}

QTEST_MAIN(TestSyntaxPluginRegistry)
#include "test_syntaxpluginregistry.moc"
