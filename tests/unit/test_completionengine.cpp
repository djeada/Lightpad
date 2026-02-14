#include "completion/completionengine.h"
#include "completion/completionproviderregistry.h"
#include "completion/icompletionprovider.h"
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

QTEST_MAIN(TestCompletionEngine)
#include "test_completionengine.moc"
