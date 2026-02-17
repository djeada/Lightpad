#include <QSignalSpy>
#include <QtTest/QtTest>

#include "definition/idefinitionprovider.h"
#include "definition/languagelspdefinitionprovider.h"
#include "definition/lspdefinitionprovider.h"
#include "definition/symbolnavigationservice.h"

class MockDefinitionProvider : public IDefinitionProvider {
  Q_OBJECT

public:
  explicit MockDefinitionProvider(const QString &providerId,
                                  const QStringList &supportedLangs,
                                  QObject *parent = nullptr)
      : IDefinitionProvider(parent), m_id(providerId),
        m_supportedLangs(supportedLangs), m_nextRequestId(1) {}

  QString id() const override { return m_id; }

  bool supports(const QString &languageId) const override {
    return m_supportedLangs.contains(languageId);
  }

  int requestDefinition(const DefinitionRequest &req) override {
    Q_UNUSED(req);
    int reqId = m_nextRequestId++;
    m_lastRequestId = reqId;
    return reqId;
  }

  void simulateReady(const QList<DefinitionTarget> &targets) {
    emit definitionReady(m_lastRequestId, targets);
  }

  void simulateFailed(const QString &error) {
    emit definitionFailed(m_lastRequestId, error);
  }

  int lastRequestId() const { return m_lastRequestId; }

private:
  QString m_id;
  QStringList m_supportedLangs;
  int m_nextRequestId;
  int m_lastRequestId = 0;
};

class TestSymbolNavigation : public QObject {
  Q_OBJECT

private slots:
  void testDefinitionRequestStruct();
  void testDefinitionTargetStruct();
  void testDefinitionTargetValid();

  void testServiceCreation();
  void testServiceNoProvider();
  void testServiceProviderRegistration();
  void testServiceGoToDefinitionSingleResult();
  void testServiceGoToDefinitionNoResult();
  void testServiceGoToDefinitionMultipleResults();
  void testServiceRequestInFlight();
  void testServiceProviderFailed();
  void testServiceCancelPendingRequest();

  void testLspProviderUriConversion();
  void testLspProviderUriConversionRoundTrip();
  void testLspProviderWithoutClient();

  void testDefaultConfigsExist();
  void testDefaultConfigsCoverPopularLanguages();
  void testDefaultConfigsHaveValidFields();
  void testLanguageProviderSupportsConfiguredLanguages();
  void testLanguageProviderRejectsUnconfiguredLanguages();
  void testLanguageProviderIdMatchesConfig();
  void testLanguageProviderServerAvailability();
  void testLanguageProviderWithUnavailableServer();
  void testLanguageProviderRegistrationInService();
};

void TestSymbolNavigation::testDefinitionRequestStruct() {
  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  QCOMPARE(req.filePath, QString("/test/file.cpp"));
  QCOMPARE(req.line, 10);
  QCOMPARE(req.column, 5);
  QCOMPARE(req.languageId, QString("cpp"));
}

void TestSymbolNavigation::testDefinitionTargetStruct() {
  DefinitionTarget target;
  target.filePath = "/test/file.cpp";
  target.line = 20;
  target.column = 3;
  target.label = "MyClass::myMethod";

  QCOMPARE(target.filePath, QString("/test/file.cpp"));
  QCOMPARE(target.line, 20);
  QCOMPARE(target.column, 3);
  QCOMPARE(target.label, QString("MyClass::myMethod"));
}

void TestSymbolNavigation::testDefinitionTargetValid() {
  DefinitionTarget validTarget;
  validTarget.filePath = "/test/file.cpp";
  validTarget.line = 1;
  validTarget.column = 0;
  QVERIFY(validTarget.isValid());

  DefinitionTarget invalidTarget;
  invalidTarget.line = 1;
  invalidTarget.column = 0;
  QVERIFY(!invalidTarget.isValid());
}

void TestSymbolNavigation::testServiceCreation() {
  SymbolNavigationService service;
  QVERIFY(!service.isRequestInFlight());
}

void TestSymbolNavigation::testServiceNoProvider() {
  SymbolNavigationService service;
  QSignalSpy noDefSpy(&service,
                       &SymbolNavigationService::noDefinitionFound);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  service.goToDefinition(req);

  QCOMPARE(noDefSpy.count(), 1);
  QVERIFY(noDefSpy.first().at(0).toString().contains("No definition provider"));
}

void TestSymbolNavigation::testServiceProviderRegistration() {
  SymbolNavigationService service;
  auto *provider =
      new MockDefinitionProvider("mock", {"cpp", "py"}, &service);
  service.registerProvider(provider);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  QSignalSpy startSpy(&service,
                       &SymbolNavigationService::definitionRequestStarted);
  service.goToDefinition(req);

  QCOMPARE(startSpy.count(), 1);
  QVERIFY(service.isRequestInFlight());
}

void TestSymbolNavigation::testServiceGoToDefinitionSingleResult() {
  SymbolNavigationService service;
  auto *provider =
      new MockDefinitionProvider("mock", {"cpp"}, &service);
  service.registerProvider(provider);

  QSignalSpy foundSpy(&service,
                       &SymbolNavigationService::definitionFound);
  QSignalSpy finishedSpy(
      &service, &SymbolNavigationService::definitionRequestFinished);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  service.goToDefinition(req);

  DefinitionTarget target;
  target.filePath = "/test/other.cpp";
  target.line = 20;
  target.column = 3;
  provider->simulateReady({target});

  QCOMPARE(foundSpy.count(), 1);
  auto targets =
      foundSpy.first().at(0).value<QList<DefinitionTarget>>();
  QCOMPARE(targets.size(), 1);
  QCOMPARE(targets.first().filePath, QString("/test/other.cpp"));
  QCOMPARE(targets.first().line, 20);
  QCOMPARE(finishedSpy.count(), 1);
  QVERIFY(!service.isRequestInFlight());
}

void TestSymbolNavigation::testServiceGoToDefinitionNoResult() {
  SymbolNavigationService service;
  auto *provider =
      new MockDefinitionProvider("mock", {"cpp"}, &service);
  service.registerProvider(provider);

  QSignalSpy noDefSpy(&service,
                       &SymbolNavigationService::noDefinitionFound);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  service.goToDefinition(req);
  provider->simulateReady({});

  QCOMPARE(noDefSpy.count(), 1);
  QVERIFY(noDefSpy.first().at(0).toString().contains("No definition found"));
}

void TestSymbolNavigation::testServiceGoToDefinitionMultipleResults() {
  SymbolNavigationService service;
  auto *provider =
      new MockDefinitionProvider("mock", {"cpp"}, &service);
  service.registerProvider(provider);

  QSignalSpy foundSpy(&service,
                       &SymbolNavigationService::definitionFound);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  service.goToDefinition(req);

  DefinitionTarget t1;
  t1.filePath = "/test/a.cpp";
  t1.line = 5;
  t1.column = 0;

  DefinitionTarget t2;
  t2.filePath = "/test/b.cpp";
  t2.line = 15;
  t2.column = 2;

  provider->simulateReady({t1, t2});

  QCOMPARE(foundSpy.count(), 1);
  auto targets =
      foundSpy.first().at(0).value<QList<DefinitionTarget>>();
  QCOMPARE(targets.size(), 2);
}

void TestSymbolNavigation::testServiceRequestInFlight() {
  SymbolNavigationService service;
  auto *provider =
      new MockDefinitionProvider("mock", {"cpp"}, &service);
  service.registerProvider(provider);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  service.goToDefinition(req);
  QVERIFY(service.isRequestInFlight());

  DefinitionRequest req2;
  req2.filePath = "/test/file2.cpp";
  req2.line = 20;
  req2.column = 0;
  req2.languageId = "cpp";

  QSignalSpy startSpy(&service,
                       &SymbolNavigationService::definitionRequestStarted);
  service.goToDefinition(req2);
  QCOMPARE(startSpy.count(), 0);
}

void TestSymbolNavigation::testServiceProviderFailed() {
  SymbolNavigationService service;
  auto *provider =
      new MockDefinitionProvider("mock", {"cpp"}, &service);
  service.registerProvider(provider);

  QSignalSpy noDefSpy(&service,
                       &SymbolNavigationService::noDefinitionFound);
  QSignalSpy finishedSpy(
      &service, &SymbolNavigationService::definitionRequestFinished);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  service.goToDefinition(req);
  provider->simulateFailed("Server error");

  QCOMPARE(noDefSpy.count(), 1);
  QCOMPARE(noDefSpy.first().at(0).toString(), QString("Server error"));
  QCOMPARE(finishedSpy.count(), 1);
  QVERIFY(!service.isRequestInFlight());
}

void TestSymbolNavigation::testServiceCancelPendingRequest() {
  SymbolNavigationService service;
  auto *provider =
      new MockDefinitionProvider("mock", {"cpp"}, &service);
  service.registerProvider(provider);

  QSignalSpy finishedSpy(
      &service, &SymbolNavigationService::definitionRequestFinished);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  service.goToDefinition(req);
  QVERIFY(service.isRequestInFlight());

  service.cancelPendingRequest();
  QVERIFY(!service.isRequestInFlight());
  QCOMPARE(finishedSpy.count(), 1);
}

void TestSymbolNavigation::testLspProviderUriConversion() {
  QCOMPARE(LspDefinitionProvider::filePathToUri("/home/user/file.cpp"),
           QString("file:///home/user/file.cpp"));

  QCOMPARE(
      LspDefinitionProvider::uriToFilePath("file:///home/user/file.cpp"),
      QString("/home/user/file.cpp"));

  QCOMPARE(LspDefinitionProvider::uriToFilePath("/home/user/file.cpp"),
           QString("/home/user/file.cpp"));
}

void TestSymbolNavigation::testLspProviderUriConversionRoundTrip() {
  QString originalPath = "/home/user/project/src/main.cpp";
  QString uri = LspDefinitionProvider::filePathToUri(originalPath);
  QString roundTripped = LspDefinitionProvider::uriToFilePath(uri);
  QCOMPARE(roundTripped, originalPath);
}

void TestSymbolNavigation::testLspProviderWithoutClient() {
  LspDefinitionProvider provider(nullptr);

  QCOMPARE(provider.id(), QString("lsp"));
  QVERIFY(!provider.supports("cpp"));

  QSignalSpy failedSpy(&provider,
                        &IDefinitionProvider::definitionFailed);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  int reqId = provider.requestDefinition(req);
  QVERIFY(reqId > 0);

  QVERIFY(failedSpy.wait(1000));
  QCOMPARE(failedSpy.count(), 1);
}

void TestSymbolNavigation::testDefaultConfigsExist() {
  QList<LanguageServerConfig> configs =
      LanguageLspDefinitionProvider::defaultConfigs();
  QVERIFY(configs.size() >= 6);
}

void TestSymbolNavigation::testDefaultConfigsCoverPopularLanguages() {
  QList<LanguageServerConfig> configs =
      LanguageLspDefinitionProvider::defaultConfigs();

  QStringList allLanguages;
  for (const LanguageServerConfig &config : configs) {
    allLanguages.append(config.supportedLanguages);
  }

  QVERIFY(allLanguages.contains("cpp"));
  QVERIFY(allLanguages.contains("c"));
  QVERIFY(allLanguages.contains("py"));
  QVERIFY(allLanguages.contains("rust"));
  QVERIFY(allLanguages.contains("go"));
  QVERIFY(allLanguages.contains("ts"));
  QVERIFY(allLanguages.contains("js"));
  QVERIFY(allLanguages.contains("java"));
}

void TestSymbolNavigation::testDefaultConfigsHaveValidFields() {
  QList<LanguageServerConfig> configs =
      LanguageLspDefinitionProvider::defaultConfigs();

  for (const LanguageServerConfig &config : configs) {
    QVERIFY2(!config.providerId.isEmpty(),
             qPrintable(QString("Empty providerId")));
    QVERIFY2(!config.displayName.isEmpty(),
             qPrintable(QString("Empty displayName for %1")
                            .arg(config.providerId)));
    QVERIFY2(!config.supportedLanguages.isEmpty(),
             qPrintable(QString("No supported languages for %1")
                            .arg(config.providerId)));
    QVERIFY2(!config.serverCommand.isEmpty(),
             qPrintable(QString("Empty serverCommand for %1")
                            .arg(config.providerId)));
  }
}

void TestSymbolNavigation::testLanguageProviderSupportsConfiguredLanguages() {
  LanguageServerConfig config;
  config.providerId = "test-provider";
  config.displayName = "Test Provider";
  config.supportedLanguages = {"cpp", "c"};
  config.serverCommand = "nonexistent-test-server";

  LanguageLspDefinitionProvider provider(config);

  QVERIFY(provider.supports("cpp"));
  QVERIFY(provider.supports("c"));
}

void TestSymbolNavigation::testLanguageProviderRejectsUnconfiguredLanguages() {
  LanguageServerConfig config;
  config.providerId = "test-provider";
  config.displayName = "Test Provider";
  config.supportedLanguages = {"cpp", "c"};
  config.serverCommand = "nonexistent-test-server";

  LanguageLspDefinitionProvider provider(config);

  QVERIFY(!provider.supports("py"));
  QVERIFY(!provider.supports("java"));
  QVERIFY(!provider.supports("rust"));
}

void TestSymbolNavigation::testLanguageProviderIdMatchesConfig() {
  LanguageServerConfig config;
  config.providerId = "my-custom-provider";
  config.displayName = "Custom Provider";
  config.supportedLanguages = {"py"};
  config.serverCommand = "nonexistent-test-server";

  LanguageLspDefinitionProvider provider(config);

  QCOMPARE(provider.id(), QString("my-custom-provider"));
}

void TestSymbolNavigation::testLanguageProviderServerAvailability() {
  LanguageServerConfig config;
  config.providerId = "test";
  config.displayName = "Test";
  config.supportedLanguages = {"cpp"};
  config.serverCommand = "nonexistent-binary-xyz-12345";

  LanguageLspDefinitionProvider provider(config);

  QVERIFY(!provider.isServerAvailable());
}

void TestSymbolNavigation::testLanguageProviderWithUnavailableServer() {
  LanguageServerConfig config;
  config.providerId = "test";
  config.displayName = "Test LSP";
  config.supportedLanguages = {"cpp"};
  config.serverCommand = "nonexistent-binary-xyz-12345";

  LanguageLspDefinitionProvider provider(config);

  QSignalSpy failedSpy(&provider,
                        &IDefinitionProvider::definitionFailed);

  DefinitionRequest req;
  req.filePath = "/test/file.cpp";
  req.line = 10;
  req.column = 5;
  req.languageId = "cpp";

  int reqId = provider.requestDefinition(req);
  QVERIFY(reqId > 0);

  QVERIFY(failedSpy.wait(1000));
  QCOMPARE(failedSpy.count(), 1);

  QString errorMsg = failedSpy.first().at(1).toString();
  QVERIFY(errorMsg.contains("not available"));
}

void TestSymbolNavigation::testLanguageProviderRegistrationInService() {
  SymbolNavigationService service;

  LanguageServerConfig cppConfig;
  cppConfig.providerId = "clangd";
  cppConfig.displayName = "clangd";
  cppConfig.supportedLanguages = {"cpp", "c"};
  cppConfig.serverCommand = "nonexistent-clangd";

  LanguageServerConfig pyConfig;
  pyConfig.providerId = "pylsp";
  pyConfig.displayName = "pylsp";
  pyConfig.supportedLanguages = {"py"};
  pyConfig.serverCommand = "nonexistent-pylsp";

  auto *cppProvider = new LanguageLspDefinitionProvider(cppConfig, &service);
  auto *pyProvider = new LanguageLspDefinitionProvider(pyConfig, &service);

  service.registerProvider(cppProvider);
  service.registerProvider(pyProvider);

  QSignalSpy startSpy(&service,
                       &SymbolNavigationService::definitionRequestStarted);

  DefinitionRequest cppReq;
  cppReq.filePath = "/test/file.cpp";
  cppReq.line = 10;
  cppReq.column = 5;
  cppReq.languageId = "cpp";

  service.goToDefinition(cppReq);
  QCOMPARE(startSpy.count(), 1);
}

QTEST_MAIN(TestSymbolNavigation)
#include "test_symbolnavigation.moc"
