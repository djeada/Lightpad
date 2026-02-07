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

class TestCompletionEngine : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void testCompletionsReadyEmittedOnce();

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

QTEST_MAIN(TestCompletionEngine)
#include "test_completionengine.moc"
