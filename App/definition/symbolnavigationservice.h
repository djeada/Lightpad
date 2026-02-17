#ifndef SYMBOLNAVIGATIONSERVICE_H
#define SYMBOLNAVIGATIONSERVICE_H

#include "idefinitionprovider.h"
#include <QList>
#include <QObject>
#include <QTimer>

class SymbolNavigationService : public QObject {
  Q_OBJECT

public:
  explicit SymbolNavigationService(QObject *parent = nullptr);
  ~SymbolNavigationService() override;

  void registerProvider(IDefinitionProvider *provider);

  void goToDefinition(const DefinitionRequest &req);

  bool isRequestInFlight() const;

  void cancelPendingRequest();

signals:
  void definitionFound(const QList<DefinitionTarget> &targets);
  void noDefinitionFound(const QString &message);
  void definitionRequestStarted();
  void definitionRequestFinished();

private:
  IDefinitionProvider *findProvider(const QString &languageId) const;

  QList<IDefinitionProvider *> m_providers;
  int m_activeRequestId;
  QTimer m_timeoutTimer;
  static const int REQUEST_TIMEOUT_MS = 10000;
};

#endif
