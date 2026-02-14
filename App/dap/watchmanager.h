#ifndef WATCHMANAGER_H
#define WATCHMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include "dapclient.h"

struct WatchExpression {
  int id = 0;
  QString expression;
  QString value;
  QString type;
  int variablesReference = 0;
  bool isError = false;
  QString errorMessage;

  QJsonObject toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["expression"] = expression;
    return obj;
  }

  static WatchExpression fromJson(const QJsonObject &obj) {
    WatchExpression watch;
    watch.id = obj["id"].toInt();
    watch.expression = obj["expression"].toString();
    return watch;
  }
};

class WatchManager : public QObject {
  Q_OBJECT

public:
  static WatchManager &instance();

  int addWatch(const QString &expression);

  void removeWatch(int id);

  void updateWatch(int id, const QString &expression);

  WatchExpression watch(int id) const;

  QList<WatchExpression> allWatches() const;

  void clearAll();

  void setDapClient(DapClient *client);

  void evaluateAll(int frameId);

  void evaluateWatch(int id, int frameId);

  void getWatchChildren(int watchId, int variablesReference);

  QJsonObject saveToJson() const;

  void loadFromJson(const QJsonObject &json);

  bool saveToFile(const QString &filePath);

  bool loadFromFile(const QString &filePath);

  void setWorkspaceFolder(const QString &folder);

  bool loadFromLightpadDir();

  bool saveToLightpadDir();

  QString lightpadWatchesPath() const;

signals:

  void watchAdded(const WatchExpression &watch);

  void watchRemoved(int id);

  void watchUpdated(const WatchExpression &watch);

  void allWatchesCleared();

  void watchChildrenReceived(int watchId, const QList<DapVariable> &children);

private slots:
  void onEvaluateResult(const QString &expression, const QString &result,
                        const QString &type, int variablesReference);
  void onEvaluateError(const QString &expression, const QString &errorMessage);
  void onVariablesReceived(int variablesReference,
                           const QList<DapVariable> &variables);

private:
  struct PendingEvaluation {
    int watchId = 0;
    int frameId = 0;
    int fallbackStage = 0;
  };

  WatchManager();
  ~WatchManager() = default;
  WatchManager(const WatchManager &) = delete;
  WatchManager &operator=(const WatchManager &) = delete;

  int m_nextId;
  QMap<int, WatchExpression> m_watches;
  QMap<QString, PendingEvaluation> m_pendingEvaluations;
  QMap<int, int> m_pendingVariables;

  DapClient *m_dapClient;
  QString m_workspaceFolder;
};

#endif
