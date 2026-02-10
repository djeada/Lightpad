#ifndef WATCHMANAGER_H
#define WATCHMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include "dapclient.h"

/**
 * @brief A watch expression
 *
 * Represents an expression that is evaluated during debugging
 * to monitor its value as the program executes.
 */
struct WatchExpression {
  int id = 0;                 // Local ID
  QString expression;         // The expression to evaluate
  QString value;              // Last evaluated value
  QString type;               // Type of the result
  int variablesReference = 0; // For structured values (expandable)
  bool isError = false;       // True if evaluation failed
  QString errorMessage;       // Error message if evaluation failed

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

/**
 * @brief Manages watch expressions for debugging
 *
 * The WatchManager provides:
 * - Adding/removing watch expressions
 * - Evaluating watches when stopped at a breakpoint
 * - Persisting watches across debug sessions
 * - Support for structured/expandable watch results
 */
class WatchManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static WatchManager &instance();

  /**
   * @brief Add a watch expression
   * @param expression Expression to watch
   * @return ID of the added watch
   */
  int addWatch(const QString &expression);

  /**
   * @brief Remove a watch by ID
   * @param id Watch ID
   */
  void removeWatch(int id);

  /**
   * @brief Update a watch expression
   * @param id Watch ID
   * @param expression New expression
   */
  void updateWatch(int id, const QString &expression);

  /**
   * @brief Get a watch by ID
   */
  WatchExpression watch(int id) const;

  /**
   * @brief Get all watches
   */
  QList<WatchExpression> allWatches() const;

  /**
   * @brief Clear all watches
   */
  void clearAll();

  /**
   * @brief Set the DAP client to use for evaluation
   */
  void setDapClient(DapClient *client);

  /**
   * @brief Evaluate all watches in the current frame context
   * @param frameId Stack frame ID for evaluation context
   */
  void evaluateAll(int frameId);

  /**
   * @brief Evaluate a single watch
   * @param id Watch ID
   * @param frameId Stack frame ID for evaluation context
   */
  void evaluateWatch(int id, int frameId);

  /**
   * @brief Get child variables for an expandable watch
   * @param watchId Watch ID
   * @param variablesReference Reference from the watch value
   */
  void getWatchChildren(int watchId, int variablesReference);

  // Persistence

  /**
   * @brief Save watches to JSON
   */
  QJsonObject saveToJson() const;

  /**
   * @brief Load watches from JSON
   */
  void loadFromJson(const QJsonObject &json);

  /**
   * @brief Save to file
   */
  bool saveToFile(const QString &filePath);

  /**
   * @brief Load from file
   */
  bool loadFromFile(const QString &filePath);

  /**
   * @brief Set the workspace folder for .lightpad storage
   */
  void setWorkspaceFolder(const QString &folder);

  /**
   * @brief Load watches from .lightpad/debug/watches.json
   */
  bool loadFromLightpadDir();

  /**
   * @brief Save watches to .lightpad/debug/watches.json
   */
  bool saveToLightpadDir();

  /**
   * @brief Get path to .lightpad watches file
   */
  QString lightpadWatchesPath() const;

signals:
  /**
   * @brief Emitted when a watch is added
   */
  void watchAdded(const WatchExpression &watch);

  /**
   * @brief Emitted when a watch is removed
   */
  void watchRemoved(int id);

  /**
   * @brief Emitted when a watch value is updated
   */
  void watchUpdated(const WatchExpression &watch);

  /**
   * @brief Emitted when all watches are cleared
   */
  void allWatchesCleared();

  /**
   * @brief Emitted when watch children are received
   */
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
    int fallbackStage = 0; // 0=watch, 1=hover, 2=default/no-context
  };

  WatchManager();
  ~WatchManager() = default;
  WatchManager(const WatchManager &) = delete;
  WatchManager &operator=(const WatchManager &) = delete;

  int m_nextId;
  QMap<int, WatchExpression> m_watches;
  QMap<QString, PendingEvaluation> m_pendingEvaluations; // expression -> state
  QMap<int, int> m_pendingVariables; // variablesReference -> watch ID

  DapClient *m_dapClient;
  QString m_workspaceFolder;
};

#endif // WATCHMANAGER_H
