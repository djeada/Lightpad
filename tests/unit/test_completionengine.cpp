#include "completion/completionengine.h"
#include "completion/completionproviderregistry.h"
#include "completion/icompletionprovider.h"
#include "completion/providers/lspcompletionprovider.h"
#include "completion/snippetregistry.h"
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <memory>

class MockProvider : public ICompletionProvider {
public:
  MockProvider(const QString &id, const QStringList &languages)
      : m_id(id), m_languages(languages), m_enabled(true) {}

  QString id() const override { return m_id; }
  QString displayName() const override { return m_id; }
  int basePriority() const override { return 100; }
  QStringList supportedLanguages() const override { return m_languages; }
  QStringList triggerCharacters() const override { return {}; }

  bool isEnabled() const override { return m_enabled; }
  void setEnabled(bool enabled) override { m_enabled = enabled; }

  void requestCompletions(
      const CompletionContext &context,
      std::function<void(const QList<CompletionItem> &)> callback) override {
    Q_UNUSED(context);
    QList<CompletionItem> items;
    CompletionItem item;
    item.label = "testItem";
    item.kind = CompletionItemKind::Keyword;
    items.append(item);
    callback(items);
  }

private:
  QString m_id;
  QStringList m_languages;
  bool m_enabled;
};

class DeferredMockProvider : public ICompletionProvider {
public:
  DeferredMockProvider(const QString &id, const QStringList &languages)
      : m_id(id), m_languages(languages), m_enabled(true) {}

  QString id() const override { return m_id; }
  QString displayName() const override { return m_id; }
  int basePriority() const override { return 50; }
  QStringList supportedLanguages() const override { return m_languages; }
  QStringList triggerCharacters() const override { return {}; }

  bool isEnabled() const override { return m_enabled; }
  void setEnabled(bool enabled) override { m_enabled = enabled; }

  void requestCompletions(
      const CompletionContext &context,
      std::function<void(const QList<CompletionItem> &)> callback) override {
    Q_UNUSED(context);

    m_pendingCallback = callback;
  }

  void cancelPendingRequests() override { m_pendingCallback = nullptr; }

  void deliverResults(const QList<CompletionItem> &items) {
    if (m_pendingCallback)
      m_pendingCallback(items);
  }

  bool hasPendingCallback() const { return m_pendingCallback != nullptr; }

private:
  QString m_id;
  QStringList m_languages;
  bool m_enabled;
  std::function<void(const QList<CompletionItem> &)> m_pendingCallback;
};

class TestCompletionEngine : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void testCompletionsReadyEmittedOnce();
  void testMultipleProvidersEmitOnce();
  void testStaleCallbackIgnored();
  void testSnippetMirrorsTakePlaceholderDefault();
  void testSnippetNestedPlaceholders();
  void testSnippetEscapesAndChoices();
  void testSnippetWithoutPlaceholders();
  void testLspProviderSurvivesClientDeletion();

private:
  CompletionEngine *m_engine;
};

void TestCompletionEngine::init() {
  CompletionProviderRegistry::instance().clear();
  m_engine = new CompletionEngine(this);
}

void TestCompletionEngine::cleanup() {
  delete m_engine;
  m_engine = nullptr;
  CompletionProviderRegistry::instance().clear();
}

void TestCompletionEngine::testCompletionsReadyEmittedOnce() {

  auto provider = std::make_shared<MockProvider>("mock", QStringList{"cpp"});
  CompletionProviderRegistry::instance().registerProvider(provider);

  m_engine->setLanguage("cpp");

  CompletionContext ctx;
  ctx.prefix = "te";
  ctx.languageId = "cpp";
  ctx.isAutoComplete = false;
  ctx.triggerKind = CompletionTriggerKind::Invoked;

  QSignalSpy spy(m_engine, &CompletionEngine::completionsReady);

  m_engine->requestCompletions(ctx);

  QCOMPARE(spy.count(), 1);

  auto results = spy.takeFirst().at(0).value<QList<CompletionItem>>();
  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].label, QString("testItem"));
}

void TestCompletionEngine::testMultipleProvidersEmitOnce() {

  auto provider1 = std::make_shared<MockProvider>("mock1", QStringList{"cpp"});
  auto provider2 = std::make_shared<MockProvider>("mock2", QStringList{"cpp"});
  CompletionProviderRegistry::instance().registerProvider(provider1);
  CompletionProviderRegistry::instance().registerProvider(provider2);

  m_engine->setLanguage("cpp");

  CompletionContext ctx;
  ctx.prefix = "te";
  ctx.languageId = "cpp";
  ctx.isAutoComplete = false;
  ctx.triggerKind = CompletionTriggerKind::Invoked;

  QSignalSpy spy(m_engine, &CompletionEngine::completionsReady);

  m_engine->requestCompletions(ctx);

  QCOMPARE(spy.count(), 1);
}

void TestCompletionEngine::testStaleCallbackIgnored() {

  auto deferred =
      std::make_shared<DeferredMockProvider>("deferred", QStringList{"cpp"});
  CompletionProviderRegistry::instance().registerProvider(deferred);

  m_engine->setLanguage("cpp");

  CompletionContext ctx;
  ctx.prefix = "te";
  ctx.languageId = "cpp";
  ctx.isAutoComplete = false;
  ctx.triggerKind = CompletionTriggerKind::Invoked;

  m_engine->requestCompletions(ctx);

  QVERIFY(deferred->hasPendingCallback());

  CompletionContext ctx2;
  ctx2.prefix = "tes";
  ctx2.languageId = "cpp";
  ctx2.isAutoComplete = false;
  ctx2.triggerKind = CompletionTriggerKind::Invoked;

  m_engine->requestCompletions(ctx2);

  QSignalSpy spy(m_engine, &CompletionEngine::completionsReady);

  QList<CompletionItem> items;
  CompletionItem item;
  item.label = "test";
  item.kind = CompletionItemKind::Keyword;
  items.append(item);
  deferred->deliverResults(items);

  QCOMPARE(spy.count(), 1);
}

void TestCompletionEngine::testSnippetMirrorsTakePlaceholderDefault() {
  QCOMPARE(Snippet::expand("for (${1:int} ${2:i} = 0; $2 < ${3:count}; "
                           "$2++) {\n\t$0\n}"),
           QString("for (int i = 0; i < count; i++) {\n\t\n}"));
  QCOMPARE(Snippet::expand("${1} and ${1:x} and $1"), QString("x and x and x"));

  Snippet snippet;
  snippet.body = "while (${1:cond}) { $1; }";
  QCOMPARE(snippet.expandedBody(), QString("while (cond) { cond; }"));
}

void TestCompletionEngine::testSnippetNestedPlaceholders() {
  QCOMPARE(Snippet::expand("${1:outer ${2:inner} end} $2"),
           QString("outer inner end inner"));
  QCOMPARE(Snippet::expand("${1:a{b}c}"), QString("a{bc}"));
  QCOMPARE(Snippet::expand("${TM_FILENAME:default} $UNKNOWN."),
           QString("default ."));
}

void TestCompletionEngine::testSnippetEscapesAndChoices() {
  QCOMPARE(Snippet::expand("cost: \\$5 \\} \\\\"), QString("cost: $5 } \\"));
  QCOMPARE(Snippet::expand("${1|one,two,three|} $1"), QString("one one"));
  QCOMPARE(Snippet::expand("${1:\\}x}"), QString("}x"));
  QCOMPARE(Snippet::expand("$ ${ ${x"), QString("$ ${ ${x"));
}

void TestCompletionEngine::testSnippetWithoutPlaceholders() {
  QCOMPARE(Snippet::expand("plain text"), QString("plain text"));
  QCOMPARE(Snippet::expand(""), QString());
}

void TestCompletionEngine::testLspProviderSurvivesClientDeletion() {
  auto *client = new LspClient();
  LspCompletionProvider provider(client);
  QVERIFY(provider.client() == client);
  delete client;
  QVERIFY(provider.client() == nullptr);
  QVERIFY(!provider.isEnabled());

  bool called = false;
  CompletionContext context;
  context.documentUri = "file:///x.cpp";
  provider.requestCompletions(context,
                              [&called](const QList<CompletionItem> &items) {
                                called = true;
                                QVERIFY(items.isEmpty());
                              });
  QVERIFY(called);
  provider.setClient(nullptr);
}

QTEST_MAIN(TestCompletionEngine)
#include "test_completionengine.moc"
