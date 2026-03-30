#include "syntax/latexsyntaxplugin.h"
#include "syntax/syntaxpluginregistry.h"
#include <QRegularExpression>
#include <QtTest/QtTest>
#include <memory>

class TestLatexSyntaxPlugin : public QObject {
  Q_OBJECT

private slots:
  void testPluginMetadata();
  void testFileExtensions();
  void testCommentStyle();
  void testSyntaxRulesNotEmpty();
  void testSectioningCommandHighlighting();
  void testEnvironmentHighlighting();
  void testCommentHighlighting();
  void testMathModeHighlighting();
  void testReferenceCommandHighlighting();
  void testGeneralCommandHighlighting();
  void testBibEntryHighlighting();
  void testMultiLineBlocks();
  void testKeywordsNotEmpty();
  void testKeywordsContainCommands();
  void testKeywordsContainEnvironments();
  void testRegistryIntegration();
};

void TestLatexSyntaxPlugin::testPluginMetadata() {
  LatexSyntaxPlugin plugin;
  QCOMPARE(plugin.languageId(), QString("latex"));
  QCOMPARE(plugin.languageName(), QString("LaTeX"));

  auto meta = plugin.metadata();
  QCOMPARE(meta.id, QString("latex"));
  QCOMPARE(meta.name, QString("LaTeX"));
  QCOMPARE(meta.category, QString("syntax"));
  QVERIFY(plugin.isLoaded());
}

void TestLatexSyntaxPlugin::testFileExtensions() {
  LatexSyntaxPlugin plugin;
  QStringList exts = plugin.fileExtensions();

  QVERIFY(exts.contains("tex"));
  QVERIFY(exts.contains("sty"));
  QVERIFY(exts.contains("cls"));
  QVERIFY(exts.contains("bib"));
  QVERIFY(exts.contains("dtx"));
  QVERIFY(exts.contains("ins"));
  QVERIFY(exts.contains("ltx"));
}

void TestLatexSyntaxPlugin::testCommentStyle() {
  LatexSyntaxPlugin plugin;
  auto style = plugin.commentStyle();
  QCOMPARE(style.first, QString("%"));
  QCOMPARE(style.second.first, QString(""));
  QCOMPARE(style.second.second, QString(""));
}

void TestLatexSyntaxPlugin::testSyntaxRulesNotEmpty() {
  LatexSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();
  QVERIFY(!rules.isEmpty());
  QVERIFY(rules.size() > 10);

  for (const auto &rule : rules) {
    QVERIFY(rule.pattern.isValid());
    QVERIFY(!rule.name.isEmpty());
  }
}

void TestLatexSyntaxPlugin::testSectioningCommandHighlighting() {
  LatexSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundSection = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_0" &&
        rule.pattern.globalMatch("\\section{Introduction}").hasNext()) {
      foundSection = true;
      break;
    }
  }
  QVERIFY2(foundSection,
           "Should highlight \\section command as keyword_0");

  bool foundChapter = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_0" &&
        rule.pattern.globalMatch("\\chapter{Methods}").hasNext()) {
      foundChapter = true;
      break;
    }
  }
  QVERIFY2(foundChapter,
           "Should highlight \\chapter command as keyword_0");
}

void TestLatexSyntaxPlugin::testEnvironmentHighlighting() {
  LatexSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundBeginEnd = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_0" &&
        rule.pattern.globalMatch("\\begin{document}").hasNext()) {
      foundBeginEnd = true;
      break;
    }
  }
  QVERIFY2(foundBeginEnd,
           "Should highlight \\begin{document} as keyword_0");
}

void TestLatexSyntaxPlugin::testCommentHighlighting() {
  LatexSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundComment = false;
  for (const auto &rule : rules) {
    if (rule.name == "comment" &&
        rule.pattern.globalMatch("% This is a comment").hasNext()) {
      foundComment = true;
      break;
    }
  }
  QVERIFY2(foundComment,
           "Should highlight % comments");

  // Verify escaped percent is NOT matched as comment
  bool escapedMatched = false;
  for (const auto &rule : rules) {
    if (rule.name == "comment") {
      auto match = rule.pattern.match("50\\% of the time");
      if (match.hasMatch() && match.capturedStart() == 2) {
        escapedMatched = true;
      }
    }
  }
  QVERIFY2(!escapedMatched,
           "Should not highlight escaped \\% as comment");
}

void TestLatexSyntaxPlugin::testMathModeHighlighting() {
  LatexSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundInlineMath = false;
  for (const auto &rule : rules) {
    if (rule.name == "number" &&
        rule.pattern.globalMatch("$x^2 + y^2 = z^2$").hasNext()) {
      foundInlineMath = true;
      break;
    }
  }
  QVERIFY2(foundInlineMath,
           "Should highlight inline math $...$ as number");
}

void TestLatexSyntaxPlugin::testReferenceCommandHighlighting() {
  LatexSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundRef = false;
  for (const auto &rule : rules) {
    if (rule.name == "function" &&
        rule.pattern.globalMatch("\\ref{fig:plot}").hasNext()) {
      foundRef = true;
      break;
    }
  }
  QVERIFY2(foundRef, "Should highlight \\ref as function");

  bool foundCite = false;
  for (const auto &rule : rules) {
    if (rule.name == "function" &&
        rule.pattern.globalMatch("\\cite{knuth1984}").hasNext()) {
      foundCite = true;
      break;
    }
  }
  QVERIFY2(foundCite, "Should highlight \\cite as function");
}

void TestLatexSyntaxPlugin::testGeneralCommandHighlighting() {
  LatexSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundGeneral = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_1" &&
        rule.pattern.globalMatch("\\maketitle").hasNext()) {
      foundGeneral = true;
      break;
    }
  }
  QVERIFY2(foundGeneral,
           "Should highlight general commands as keyword_1");
}

void TestLatexSyntaxPlugin::testBibEntryHighlighting() {
  LatexSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundBib = false;
  for (const auto &rule : rules) {
    if (rule.name == "class" &&
        rule.pattern.globalMatch("@article{knuth1984").hasNext()) {
      foundBib = true;
      break;
    }
  }
  QVERIFY2(foundBib,
           "Should highlight @article{key as class");
}

void TestLatexSyntaxPlugin::testMultiLineBlocks() {
  LatexSyntaxPlugin plugin;
  auto blocks = plugin.multiLineBlocks();

  QVERIFY(!blocks.isEmpty());

  bool foundVerbatim = false;
  for (const auto &block : blocks) {
    if (block.startPattern.globalMatch("\\begin{verbatim}").hasNext() &&
        block.endPattern.globalMatch("\\end{verbatim}").hasNext()) {
      foundVerbatim = true;
      break;
    }
  }
  QVERIFY2(foundVerbatim,
           "Should have verbatim multi-line block");
}

void TestLatexSyntaxPlugin::testKeywordsNotEmpty() {
  LatexSyntaxPlugin plugin;
  QStringList keywords = plugin.keywords();
  QVERIFY(!keywords.isEmpty());
  QVERIFY(keywords.size() > 50);
}

void TestLatexSyntaxPlugin::testKeywordsContainCommands() {
  LatexSyntaxPlugin plugin;
  QStringList keywords = plugin.keywords();

  QVERIFY(keywords.contains("\\section"));
  QVERIFY(keywords.contains("\\subsection"));
  QVERIFY(keywords.contains("\\frac"));
  QVERIFY(keywords.contains("\\alpha"));
  QVERIFY(keywords.contains("\\textbf"));
  QVERIFY(keywords.contains("\\usepackage"));
  QVERIFY(keywords.contains("\\documentclass"));
  QVERIFY(keywords.contains("\\ref"));
  QVERIFY(keywords.contains("\\cite"));
  QVERIFY(keywords.contains("\\label"));
  QVERIFY(keywords.contains("\\maketitle"));
  QVERIFY(keywords.contains("\\tableofcontents"));
}

void TestLatexSyntaxPlugin::testKeywordsContainEnvironments() {
  LatexSyntaxPlugin plugin;
  QStringList keywords = plugin.keywords();

  QVERIFY(keywords.contains("\\begin{document}"));
  QVERIFY(keywords.contains("\\end{document}"));
  QVERIFY(keywords.contains("\\begin{figure}"));
  QVERIFY(keywords.contains("\\end{figure}"));
  QVERIFY(keywords.contains("\\begin{equation}"));
  QVERIFY(keywords.contains("\\end{equation}"));
  QVERIFY(keywords.contains("\\begin{align}"));
  QVERIFY(keywords.contains("\\end{align}"));
  QVERIFY(keywords.contains("\\begin{itemize}"));
  QVERIFY(keywords.contains("\\end{itemize}"));
  QVERIFY(keywords.contains("\\begin{tikzpicture}"));
}

void TestLatexSyntaxPlugin::testRegistryIntegration() {
  auto &registry = SyntaxPluginRegistry::instance();
  registry.clear();

  registry.registerPlugin(std::make_unique<LatexSyntaxPlugin>());

  QVERIFY(registry.isLanguageSupported("latex"));
  QVERIFY(registry.isExtensionSupported("tex"));
  QVERIFY(registry.isExtensionSupported("bib"));
  QVERIFY(registry.isExtensionSupported("sty"));
  QVERIFY(registry.isExtensionSupported("cls"));

  ISyntaxPlugin *plugin = registry.getPluginByExtension("tex");
  QVERIFY(plugin != nullptr);
  QCOMPARE(plugin->languageId(), QString("latex"));

  ISyntaxPlugin *bibPlugin = registry.getPluginByExtension("bib");
  QVERIFY(bibPlugin != nullptr);
  QCOMPARE(bibPlugin->languageId(), QString("latex"));

  registry.clear();
}

QTEST_MAIN(TestLatexSyntaxPlugin)
#include "test_latexsyntaxplugin.moc"
