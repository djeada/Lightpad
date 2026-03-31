#include "syntax/dockerfilesyntaxplugin.h"
#include "syntax/syntaxpluginregistry.h"
#include <QRegularExpression>
#include <QtTest/QtTest>
#include <memory>

class TestDockerfileSyntaxPlugin : public QObject {
  Q_OBJECT

private slots:
  void testPluginMetadata();
  void testFileExtensions();
  void testCommentStyle();
  void testSyntaxRulesNotEmpty();
  void testInstructionHighlighting();
  void testFromHighlighting();
  void testRunHighlighting();
  void testCopyHighlighting();
  void testCommentHighlighting();
  void testVariableHighlighting();
  void testStringHighlighting();
  void testFlagHighlighting();
  void testAsKeywordHighlighting();
  void testImageTagHighlighting();
  void testKeywordsNotEmpty();
  void testKeywordsContainInstructions();
  void testRegistryIntegration();
};

void TestDockerfileSyntaxPlugin::testPluginMetadata() {
  DockerfileSyntaxPlugin plugin;
  QCOMPARE(plugin.languageId(), QString("dockerfile"));
  QCOMPARE(plugin.languageName(), QString("Dockerfile"));

  auto meta = plugin.metadata();
  QCOMPARE(meta.id, QString("dockerfile"));
  QCOMPARE(meta.name, QString("Dockerfile"));
  QCOMPARE(meta.category, QString("syntax"));
  QVERIFY(plugin.isLoaded());
}

void TestDockerfileSyntaxPlugin::testFileExtensions() {
  DockerfileSyntaxPlugin plugin;
  QStringList exts = plugin.fileExtensions();

  QVERIFY(exts.contains("dockerfile"));
  QVERIFY(exts.contains("containerfile"));
}

void TestDockerfileSyntaxPlugin::testCommentStyle() {
  DockerfileSyntaxPlugin plugin;
  auto style = plugin.commentStyle();
  QCOMPARE(style.first, QString("#"));
  QCOMPARE(style.second.first, QString(""));
  QCOMPARE(style.second.second, QString(""));
}

void TestDockerfileSyntaxPlugin::testSyntaxRulesNotEmpty() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();
  QVERIFY(!rules.isEmpty());
  QVERIFY(rules.size() > 10);

  for (const auto &rule : rules) {
    QVERIFY(rule.pattern.isValid());
    QVERIFY(!rule.name.isEmpty());
  }
}

void TestDockerfileSyntaxPlugin::testInstructionHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  QStringList instructions = {"FROM", "RUN", "CMD", "COPY", "ADD", "ENV",
                               "EXPOSE", "WORKDIR", "ENTRYPOINT", "VOLUME",
                               "USER", "ARG", "LABEL", "HEALTHCHECK", "SHELL"};

  for (const QString &instr : instructions) {
    bool found = false;
    for (const auto &rule : rules) {
      if (rule.name == "keyword_0" &&
          rule.pattern.globalMatch(instr + " something").hasNext()) {
        found = true;
        break;
      }
    }
    QVERIFY2(found,
             qPrintable(QString("Should highlight %1 as keyword_0").arg(instr)));
  }
}

void TestDockerfileSyntaxPlugin::testFromHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundFrom = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_0" &&
        rule.pattern.globalMatch("FROM ubuntu:22.04").hasNext()) {
      foundFrom = true;
      break;
    }
  }
  QVERIFY2(foundFrom, "Should highlight FROM instruction");
}

void TestDockerfileSyntaxPlugin::testRunHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundRun = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_0" &&
        rule.pattern.globalMatch("RUN apt-get update").hasNext()) {
      foundRun = true;
      break;
    }
  }
  QVERIFY2(foundRun, "Should highlight RUN instruction");
}

void TestDockerfileSyntaxPlugin::testCopyHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundCopy = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_0" &&
        rule.pattern.globalMatch("COPY . /app").hasNext()) {
      foundCopy = true;
      break;
    }
  }
  QVERIFY2(foundCopy, "Should highlight COPY instruction");
}

void TestDockerfileSyntaxPlugin::testCommentHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundComment = false;
  for (const auto &rule : rules) {
    if (rule.name == "comment" &&
        rule.pattern.globalMatch("# This is a comment").hasNext()) {
      foundComment = true;
      break;
    }
  }
  QVERIFY2(foundComment, "Should highlight # comments");
}

void TestDockerfileSyntaxPlugin::testVariableHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundBraceVar = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_1" &&
        rule.pattern.globalMatch("${MY_VAR}").hasNext()) {
      foundBraceVar = true;
      break;
    }
  }
  QVERIFY2(foundBraceVar, "Should highlight ${VAR} variables");

  bool foundVar = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_1" &&
        rule.pattern.globalMatch("$HOME").hasNext()) {
      foundVar = true;
      break;
    }
  }
  QVERIFY2(foundVar, "Should highlight $VAR variables");
}

void TestDockerfileSyntaxPlugin::testStringHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundString = false;
  for (const auto &rule : rules) {
    if (rule.name == "string" &&
        rule.pattern.globalMatch("\"hello world\"").hasNext()) {
      foundString = true;
      break;
    }
  }
  QVERIFY2(foundString, "Should highlight double-quoted strings");
}

void TestDockerfileSyntaxPlugin::testFlagHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundFlag = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_2" &&
        rule.pattern.globalMatch("--from=builder").hasNext()) {
      foundFlag = true;
      break;
    }
  }
  QVERIFY2(foundFlag, "Should highlight --flags");
}

void TestDockerfileSyntaxPlugin::testAsKeywordHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundAs = false;
  for (const auto &rule : rules) {
    if (rule.name == "keyword_1" &&
        rule.pattern.globalMatch("FROM ubuntu AS builder").hasNext()) {
      foundAs = true;
      break;
    }
  }
  QVERIFY2(foundAs, "Should highlight AS keyword");
}

void TestDockerfileSyntaxPlugin::testImageTagHighlighting() {
  DockerfileSyntaxPlugin plugin;
  auto rules = plugin.syntaxRules();

  bool foundImage = false;
  for (const auto &rule : rules) {
    if (rule.name == "function" &&
        rule.pattern.globalMatch("FROM python:3.11-slim").hasNext()) {
      foundImage = true;
      break;
    }
  }
  QVERIFY2(foundImage, "Should highlight image:tag as function");
}

void TestDockerfileSyntaxPlugin::testKeywordsNotEmpty() {
  DockerfileSyntaxPlugin plugin;
  QStringList keywords = plugin.keywords();
  QVERIFY(!keywords.isEmpty());
  QVERIFY(keywords.size() >= 15);
}

void TestDockerfileSyntaxPlugin::testKeywordsContainInstructions() {
  DockerfileSyntaxPlugin plugin;
  QStringList keywords = plugin.keywords();

  QVERIFY(keywords.contains("FROM"));
  QVERIFY(keywords.contains("RUN"));
  QVERIFY(keywords.contains("CMD"));
  QVERIFY(keywords.contains("COPY"));
  QVERIFY(keywords.contains("ADD"));
  QVERIFY(keywords.contains("ENV"));
  QVERIFY(keywords.contains("EXPOSE"));
  QVERIFY(keywords.contains("WORKDIR"));
  QVERIFY(keywords.contains("ENTRYPOINT"));
  QVERIFY(keywords.contains("VOLUME"));
  QVERIFY(keywords.contains("USER"));
  QVERIFY(keywords.contains("ARG"));
  QVERIFY(keywords.contains("LABEL"));
  QVERIFY(keywords.contains("HEALTHCHECK"));
  QVERIFY(keywords.contains("SHELL"));
}

void TestDockerfileSyntaxPlugin::testRegistryIntegration() {
  auto &registry = SyntaxPluginRegistry::instance();
  registry.clear();

  registry.registerPlugin(std::make_unique<DockerfileSyntaxPlugin>());

  QVERIFY(registry.isLanguageSupported("dockerfile"));
  QVERIFY(registry.isExtensionSupported("dockerfile"));
  QVERIFY(registry.isExtensionSupported("containerfile"));

  ISyntaxPlugin *plugin = registry.getPluginByExtension("dockerfile");
  QVERIFY(plugin != nullptr);
  QCOMPARE(plugin->languageId(), QString("dockerfile"));

  ISyntaxPlugin *containerPlugin =
      registry.getPluginByExtension("containerfile");
  QVERIFY(containerPlugin != nullptr);
  QCOMPARE(containerPlugin->languageId(), QString("dockerfile"));

  registry.clear();
}

QTEST_MAIN(TestDockerfileSyntaxPlugin)
#include "test_dockerfilesyntaxplugin.moc"
