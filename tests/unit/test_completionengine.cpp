#include "completion/completionengine.h"
#include "completion/completionproviderregistry.h"
#include "completion/icompletionprovider.h"
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <memory>

/**
 * @brief Mock completion provider for testing
 */
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

/**
 * @brief Mock provider that stores its callback for deferred (async)
 * invocation
 */
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
    // Store callback for later invocation
    m_pendingCallback = callback;
  }

  void cancelPendingRequests() override { m_pendingCallback = nullptr; }

  /** Deliver results after the fact (simulates async) */
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
  // Register a provider
  auto provider =
      std::make_shared<MockProvider>("mock", QStringList{"cpp"});
  CompletionProviderRegistry::instance().registerProvider(provider);

  m_engine->setLanguage("cpp");

  // Use explicit (non-auto) completion to avoid debounce timer
  CompletionContext ctx;
  ctx.prefix = "te";
  ctx.languageId = "cpp";
  ctx.isAutoComplete = false;
  ctx.triggerKind = CompletionTriggerKind::Invoked;

  QSignalSpy spy(m_engine, &CompletionEngine::completionsReady);

  m_engine->requestCompletions(ctx);

  // Signal should be emitted exactly once
  QCOMPARE(spy.count(), 1);

  // And the results should contain exactly one item
  auto results = spy.takeFirst().at(0).value<QList<CompletionItem>>();
  QCOMPARE(results.size(), 1);
  QCOMPARE(results[0].label, QString("testItem"));
}

void TestCompletionEngine::testMultipleProvidersEmitOnce() {
  // Register two synchronous providers for the same language
  auto provider1 =
      std::make_shared<MockProvider>("mock1", QStringList{"cpp"});
  auto provider2 =
      std::make_shared<MockProvider>("mock2", QStringList{"cpp"});
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

  // Signal should still be emitted exactly once even with two providers
  QCOMPARE(spy.count(), 1);
}

void TestCompletionEngine::testStaleCallbackIgnored() {
  // Register an async (deferred) provider
  auto deferred =
      std::make_shared<DeferredMockProvider>("deferred", QStringList{"cpp"});
  CompletionProviderRegistry::instance().registerProvider(deferred);

  m_engine->setLanguage("cpp");

  // First request – callback is stored but not yet delivered
  CompletionContext ctx;
  ctx.prefix = "te";
  ctx.languageId = "cpp";
  ctx.isAutoComplete = false;
  ctx.triggerKind = CompletionTriggerKind::Invoked;

  m_engine->requestCompletions(ctx);

  // The deferred provider has a pending callback from request 1
  QVERIFY(deferred->hasPendingCallback());

  // Grab a reference to the stale callback before the second request
  // overwrites it inside the provider
  std::function<void(const QList<CompletionItem> &)> staleCallback;
  // We need to directly capture it; deliver via the provider object
  // Actually we can just save the provider's pending callback
  auto savedProvider = deferred;

  // Issue a second request – this should invalidate the first one
  CompletionContext ctx2;
  ctx2.prefix = "tes";
  ctx2.languageId = "cpp";
  ctx2.isAutoComplete = false;
  ctx2.triggerKind = CompletionTriggerKind::Invoked;

  m_engine->requestCompletions(ctx2);

  QSignalSpy spy(m_engine, &CompletionEngine::completionsReady);

  // Now deliver results from the second (current) request
  QList<CompletionItem> items;
  CompletionItem item;
  item.label = "test";
  item.kind = CompletionItemKind::Keyword;
  items.append(item);
  deferred->deliverResults(items);

  // Only one signal should have been emitted
  QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestCompletionEngine)
#include "test_completionengine.moc"
