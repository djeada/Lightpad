#include "completion/completionproviderregistry.h"
#include "completion/icompletionprovider.h"
#include <QtTest/QtTest>
#include <memory>

class MockCompletionProvider : public ICompletionProvider {
public:
  MockCompletionProvider(const QString &id, const QString &name, int priority,
                         const QStringList &languages,
                         const QStringList &triggers = {})
      : m_id(id), m_name(name), m_priority(priority), m_languages(languages),
        m_triggers(triggers), m_enabled(true) {}

  QString id() const override { return m_id; }
  QString displayName() const override { return m_name; }
  int basePriority() const override { return m_priority; }
  QStringList supportedLanguages() const override { return m_languages; }
  QStringList triggerCharacters() const override { return m_triggers; }

  bool isEnabled() const override { return m_enabled; }
  void setEnabled(bool enabled) override { m_enabled = enabled; }

  void requestCompletions(
      const CompletionContext &context,
      std::function<void(const QList<CompletionItem> &)> callback) override {
    Q_UNUSED(context);

    callback({});
  }

private:
  QString m_id;
  QString m_name;
  int m_priority;
  QStringList m_languages;
  QStringList m_triggers;
  bool m_enabled;
};

class TestCompletionProviderRegistry : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void testSingletonInstance();
  void testRegisterProvider();
  void testRegisterNullProvider();
  void testRegisterEmptyId();
  void testRegisterDuplicateReplace();
  void testUnregisterProvider();
  void testUnregisterNonExistent();
  void testGetProvider();
  void testProvidersForLanguage();
  void testProvidersForLanguageWildcard();
  void testProvidersForLanguagePrioritySorting();
  void testProvidersForLanguageDisabled();
  void testAllProviders();
  void testAllProviderIds();
  void testAllTriggerCharacters();
  void testHasProvidersForLanguage();
  void testProviderCount();
  void testClear();
  void testSignals();
};

void TestCompletionProviderRegistry::init() {

  CompletionProviderRegistry::instance().clear();
}

void TestCompletionProviderRegistry::cleanup() {

  CompletionProviderRegistry::instance().clear();
}

void TestCompletionProviderRegistry::testSingletonInstance() {
  CompletionProviderRegistry &reg1 = CompletionProviderRegistry::instance();
  CompletionProviderRegistry &reg2 = CompletionProviderRegistry::instance();
  QCOMPARE(&reg1, &reg2);
}

void TestCompletionProviderRegistry::testRegisterProvider() {
  auto &registry = CompletionProviderRegistry::instance();

  auto provider = std::make_shared<MockCompletionProvider>(
      "test_provider", "Test Provider", 100, QStringList{"cpp"});

  registry.registerProvider(provider);

  QCOMPARE(registry.providerCount(), 1);
  QVERIFY(registry.getProvider("test_provider") != nullptr);
  QCOMPARE(registry.getProvider("test_provider")->displayName(),
           QString("Test Provider"));
}

void TestCompletionProviderRegistry::testRegisterNullProvider() {
  auto &registry = CompletionProviderRegistry::instance();

  registry.registerProvider(nullptr);

  QCOMPARE(registry.providerCount(), 0);
}

void TestCompletionProviderRegistry::testRegisterEmptyId() {
  auto &registry = CompletionProviderRegistry::instance();

  auto provider = std::make_shared<MockCompletionProvider>(
      "", "Empty ID Provider", 100, QStringList{"cpp"});

  registry.registerProvider(provider);

  QCOMPARE(registry.providerCount(), 0);
}

void TestCompletionProviderRegistry::testRegisterDuplicateReplace() {
  auto &registry = CompletionProviderRegistry::instance();

  auto provider1 = std::make_shared<MockCompletionProvider>(
      "test_id", "Provider 1", 100, QStringList{"cpp"});
  auto provider2 = std::make_shared<MockCompletionProvider>(
      "test_id", "Provider 2", 50, QStringList{"python"});

  registry.registerProvider(provider1);
  QCOMPARE(registry.providerCount(), 1);
  QCOMPARE(registry.getProvider("test_id")->displayName(),
           QString("Provider 1"));

  registry.registerProvider(provider2);
  QCOMPARE(registry.providerCount(), 1);
  QCOMPARE(registry.getProvider("test_id")->displayName(),
           QString("Provider 2"));
}

void TestCompletionProviderRegistry::testUnregisterProvider() {
  auto &registry = CompletionProviderRegistry::instance();

  auto provider = std::make_shared<MockCompletionProvider>(
      "to_remove", "Provider to Remove", 100, QStringList{"*"});

  registry.registerProvider(provider);
  QCOMPARE(registry.providerCount(), 1);

  bool result = registry.unregisterProvider("to_remove");
  QVERIFY(result);
  QCOMPARE(registry.providerCount(), 0);
  QVERIFY(registry.getProvider("to_remove") == nullptr);
}

void TestCompletionProviderRegistry::testUnregisterNonExistent() {
  auto &registry = CompletionProviderRegistry::instance();

  bool result = registry.unregisterProvider("does_not_exist");
  QVERIFY(!result);
}

void TestCompletionProviderRegistry::testGetProvider() {
  auto &registry = CompletionProviderRegistry::instance();

  auto provider = std::make_shared<MockCompletionProvider>(
      "my_provider", "My Provider", 100, QStringList{"cpp"});
  registry.registerProvider(provider);

  auto retrieved = registry.getProvider("my_provider");
  QVERIFY(retrieved != nullptr);
  QCOMPARE(retrieved->id(), QString("my_provider"));

  auto notFound = registry.getProvider("nonexistent");
  QVERIFY(notFound == nullptr);
}

void TestCompletionProviderRegistry::testProvidersForLanguage() {
  auto &registry = CompletionProviderRegistry::instance();

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "cpp_provider", "C++ Provider", 50, QStringList{"cpp"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "python_provider", "Python Provider", 50, QStringList{"python"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "multi_provider", "Multi Provider", 50, QStringList{"cpp", "python"}));

  auto cppProviders = registry.providersForLanguage("cpp");
  QCOMPARE(cppProviders.size(), 2);

  QStringList cppIds;
  for (auto &p : cppProviders) {
    cppIds.append(p->id());
  }
  QVERIFY(cppIds.contains("cpp_provider"));
  QVERIFY(cppIds.contains("multi_provider"));
  QVERIFY(!cppIds.contains("python_provider"));

  auto pyProviders = registry.providersForLanguage("python");
  QCOMPARE(pyProviders.size(), 2);

  auto rustProviders = registry.providersForLanguage("rust");
  QCOMPARE(rustProviders.size(), 0);
}

void TestCompletionProviderRegistry::testProvidersForLanguageWildcard() {
  auto &registry = CompletionProviderRegistry::instance();

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "universal", "Universal Provider", 100, QStringList{"*"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "cpp_only", "C++ Only", 50, QStringList{"cpp"}));

  auto cppProviders = registry.providersForLanguage("cpp");
  QCOMPARE(cppProviders.size(), 2);

  auto pythonProviders = registry.providersForLanguage("python");
  QCOMPARE(pythonProviders.size(), 1);
  QCOMPARE(pythonProviders[0]->id(), QString("universal"));

  auto randomProviders = registry.providersForLanguage("some_random_language");
  QCOMPARE(randomProviders.size(), 1);
}

void TestCompletionProviderRegistry::testProvidersForLanguagePrioritySorting() {
  auto &registry = CompletionProviderRegistry::instance();

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "low_priority", "Low", 100, QStringList{"*"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "high_priority", "High", 10, QStringList{"*"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "medium_priority", "Medium", 50, QStringList{"*"}));

  auto providers = registry.providersForLanguage("cpp");
  QCOMPARE(providers.size(), 3);

  QCOMPARE(providers[0]->id(), QString("high_priority"));
  QCOMPARE(providers[1]->id(), QString("medium_priority"));
  QCOMPARE(providers[2]->id(), QString("low_priority"));
}

void TestCompletionProviderRegistry::testProvidersForLanguageDisabled() {
  auto &registry = CompletionProviderRegistry::instance();

  auto provider = std::make_shared<MockCompletionProvider>(
      "disabled_provider", "Disabled", 50, QStringList{"*"});
  provider->setEnabled(false);
  registry.registerProvider(provider);

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "enabled_provider", "Enabled", 50, QStringList{"*"}));

  auto providers = registry.providersForLanguage("cpp");
  QCOMPARE(providers.size(), 1);
  QCOMPARE(providers[0]->id(), QString("enabled_provider"));
}

void TestCompletionProviderRegistry::testAllProviders() {
  auto &registry = CompletionProviderRegistry::instance();

  QCOMPARE(registry.allProviders().size(), 0);

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "p1", "Provider 1", 100, QStringList{"*"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "p2", "Provider 2", 100, QStringList{"*"}));

  auto all = registry.allProviders();
  QCOMPARE(all.size(), 2);
}

void TestCompletionProviderRegistry::testAllProviderIds() {
  auto &registry = CompletionProviderRegistry::instance();

  QVERIFY(registry.allProviderIds().isEmpty());

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "alpha", "Alpha", 100, QStringList{"*"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "beta", "Beta", 100, QStringList{"*"}));

  QStringList ids = registry.allProviderIds();
  QCOMPARE(ids.size(), 2);
  QVERIFY(ids.contains("alpha"));
  QVERIFY(ids.contains("beta"));
}

void TestCompletionProviderRegistry::testAllTriggerCharacters() {
  auto &registry = CompletionProviderRegistry::instance();

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "cpp_lsp", "C++ LSP", 10, QStringList{"cpp"},
      QStringList{".", "::", "->"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "py_lsp", "Python LSP", 10, QStringList{"python"}, QStringList{"."}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "keywords", "Keywords", 100, QStringList{"*"}, QStringList{}));

  QStringList cppTriggers = registry.allTriggerCharacters("cpp");
  QVERIFY(cppTriggers.contains("."));
  QVERIFY(cppTriggers.contains("::"));
  QVERIFY(cppTriggers.contains("->"));

  QStringList pyTriggers = registry.allTriggerCharacters("python");
  QVERIFY(pyTriggers.contains("."));
  QCOMPARE(pyTriggers.size(), 1);

  QStringList rustTriggers = registry.allTriggerCharacters("rust");
  QVERIFY(rustTriggers.isEmpty());
}

void TestCompletionProviderRegistry::testHasProvidersForLanguage() {
  auto &registry = CompletionProviderRegistry::instance();

  QVERIFY(!registry.hasProvidersForLanguage("cpp"));

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "cpp_provider", "C++", 50, QStringList{"cpp"}));

  QVERIFY(registry.hasProvidersForLanguage("cpp"));
  QVERIFY(!registry.hasProvidersForLanguage("python"));

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "universal", "Universal", 100, QStringList{"*"}));

  QVERIFY(registry.hasProvidersForLanguage("python"));
  QVERIFY(registry.hasProvidersForLanguage("any_language"));
}

void TestCompletionProviderRegistry::testProviderCount() {
  auto &registry = CompletionProviderRegistry::instance();

  QCOMPARE(registry.providerCount(), 0);

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "p1", "P1", 100, QStringList{"*"}));
  QCOMPARE(registry.providerCount(), 1);

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "p2", "P2", 100, QStringList{"*"}));
  QCOMPARE(registry.providerCount(), 2);

  registry.unregisterProvider("p1");
  QCOMPARE(registry.providerCount(), 1);
}

void TestCompletionProviderRegistry::testClear() {
  auto &registry = CompletionProviderRegistry::instance();

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "p1", "P1", 100, QStringList{"*"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "p2", "P2", 100, QStringList{"*"}));

  QCOMPARE(registry.providerCount(), 2);

  registry.clear();

  QCOMPARE(registry.providerCount(), 0);
  QVERIFY(registry.allProviderIds().isEmpty());
  QVERIFY(registry.getProvider("p1") == nullptr);
}

void TestCompletionProviderRegistry::testSignals() {
  auto &registry = CompletionProviderRegistry::instance();

  QSignalSpy registeredSpy(&registry,
                           &CompletionProviderRegistry::providerRegistered);
  QSignalSpy unregisteredSpy(&registry,
                             &CompletionProviderRegistry::providerUnregistered);

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "signal_test", "Signal Test", 100, QStringList{"*"}));

  QCOMPARE(registeredSpy.count(), 1);
  QCOMPARE(registeredSpy.takeFirst().at(0).toString(), QString("signal_test"));

  registry.unregisterProvider("signal_test");

  QCOMPARE(unregisteredSpy.count(), 1);
  QCOMPARE(unregisteredSpy.takeFirst().at(0).toString(),
           QString("signal_test"));

  registeredSpy.clear();
  unregisteredSpy.clear();

  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "dup_test", "Dup 1", 100, QStringList{"*"}));
  registry.registerProvider(std::make_shared<MockCompletionProvider>(
      "dup_test", "Dup 2", 100, QStringList{"*"}));

  QCOMPARE(registeredSpy.count(), 2);
  QCOMPARE(unregisteredSpy.count(), 1);
}

QTEST_MAIN(TestCompletionProviderRegistry)
#include "test_completionproviderregistry.moc"
