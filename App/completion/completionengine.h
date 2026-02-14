#ifndef COMPLETIONENGINE_H
#define COMPLETIONENGINE_H

#include "completioncontext.h"
#include "completionitem.h"
#include "completionproviderregistry.h"
#include <QList>
#include <QObject>
#include <QTimer>
#include <memory>

class CompletionEngine : public QObject {
  Q_OBJECT

public:
  explicit CompletionEngine(QObject *parent = nullptr);
  ~CompletionEngine();

  void setLanguage(const QString &languageId);

  QString language() const { return m_languageId; }

  void requestCompletions(const CompletionContext &context);

  void cancelPendingRequests();

  bool isRequestPending() const { return m_pendingProviders > 0; }

  void setMinimumPrefixLength(int length) { m_minPrefixLength = length; }
  int minimumPrefixLength() const { return m_minPrefixLength; }

  void setAutoTriggerDelay(int ms) { m_autoTriggerDelay = ms; }
  int autoTriggerDelay() const { return m_autoTriggerDelay; }

  void setMaxResults(int count) { m_maxResults = count; }
  int maxResults() const { return m_maxResults; }

  QList<CompletionItem> filterResults(const QString &prefix) const;

  QList<CompletionItem> lastResults() const { return m_lastResults; }

signals:

  void completionsReady(const QList<CompletionItem> &items);

  void completionsFailed(const QString &error);

private slots:
  void onDebounceTimeout();

private:
  void collectProviderResults(int requestId,
                              const QList<CompletionItem> &items);
  void mergeAndSortResults();
  void notifyResults();
  void executeCompletionRequest();

  QString m_languageId;
  CompletionContext m_currentContext;
  QList<CompletionItem> m_pendingItems;
  QList<CompletionItem> m_lastResults;
  int m_pendingProviders = 0;
  int m_currentRequestId = 0;

  int m_minPrefixLength = 2;
  int m_autoTriggerDelay = 150;
  int m_maxResults = 50;

  QTimer *m_debounceTimer;
};

#endif
