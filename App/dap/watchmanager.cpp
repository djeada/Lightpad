#include "watchmanager.h"
#include "../core/logging/logger.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>

WatchManager &WatchManager::instance() {
  static WatchManager instance;
  return instance;
}

WatchManager::WatchManager()
    : QObject(nullptr), m_nextId(1), m_dapClient(nullptr) {}

int WatchManager::addWatch(const QString &expression) {
  if (expression.trimmed().isEmpty()) {
    return 0;
  }

  WatchExpression watch;
  watch.id = m_nextId++;
  watch.expression = expression.trimmed();

  m_watches[watch.id] = watch;

  LOG_DEBUG(QString("Added watch %1: %2").arg(watch.id).arg(watch.expression));
  emit watchAdded(watch);

  return watch.id;
}

void WatchManager::removeWatch(int id) {
  if (m_watches.remove(id) > 0) {
    LOG_DEBUG(QString("Removed watch %1").arg(id));
    emit watchRemoved(id);
  }
}

void WatchManager::updateWatch(int id, const QString &expression) {
  if (!m_watches.contains(id)) {
    return;
  }

  m_watches[id].expression = expression.trimmed();
  m_watches[id].value.clear();
  m_watches[id].type.clear();
  m_watches[id].variablesReference = 0;
  m_watches[id].isError = false;
  m_watches[id].errorMessage.clear();

  emit watchUpdated(m_watches[id]);
}

WatchExpression WatchManager::watch(int id) const {
  return m_watches.value(id);
}

QList<WatchExpression> WatchManager::allWatches() const {
  return m_watches.values();
}

void WatchManager::clearAll() {
  m_watches.clear();
  m_pendingEvaluations.clear();
  m_pendingVariables.clear();
  emit allWatchesCleared();
}

void WatchManager::setDapClient(DapClient *client) {
  if (m_dapClient) {
    disconnect(m_dapClient, nullptr, this, nullptr);
  }

  m_dapClient = client;

  if (m_dapClient) {
    connect(m_dapClient, &DapClient::evaluateResult, this,
            &WatchManager::onEvaluateResult);
    connect(m_dapClient, &DapClient::evaluateError, this,
            &WatchManager::onEvaluateError);
    connect(m_dapClient, &DapClient::variablesReceived, this,
            &WatchManager::onVariablesReceived);
  }
}

void WatchManager::evaluateAll(int frameId) {
  if (!m_dapClient || m_dapClient->state() != DapClient::State::Stopped) {
    return;
  }

  m_pendingEvaluations.clear();

  for (auto it = m_watches.begin(); it != m_watches.end(); ++it) {
    evaluateWatch(it.key(), frameId);
  }
}

void WatchManager::evaluateWatch(int id, int frameId) {
  if (!m_dapClient || !m_watches.contains(id)) {
    return;
  }

  if (m_dapClient->state() != DapClient::State::Stopped) {
    // Can't evaluate while running
    m_watches[id].value = "<not available>";
    m_watches[id].isError = true;
    emit watchUpdated(m_watches[id]);
    return;
  }

  const QString &expression = m_watches[id].expression;
  m_pendingEvaluations[expression] = id;

  m_dapClient->evaluate(expression, frameId, "watch");
}

void WatchManager::getWatchChildren(int watchId, int variablesReference) {
  if (!m_dapClient || variablesReference <= 0) {
    return;
  }

  m_pendingVariables[variablesReference] = watchId;
  m_dapClient->getVariables(variablesReference);
}

void WatchManager::onEvaluateResult(const QString &expression,
                                    const QString &result, const QString &type,
                                    int variablesReference) {
  if (!m_pendingEvaluations.contains(expression)) {
    return;
  }

  int watchId = m_pendingEvaluations.take(expression);

  if (!m_watches.contains(watchId)) {
    return;
  }

  WatchExpression &watch = m_watches[watchId];
  watch.value = result;
  watch.type = type;
  watch.variablesReference = variablesReference;
  watch.isError = false;
  watch.errorMessage.clear();

  emit watchUpdated(watch);
}

void WatchManager::onEvaluateError(const QString &expression,
                                   const QString &errorMessage) {
  if (!m_pendingEvaluations.contains(expression)) {
    return;
  }

  int watchId = m_pendingEvaluations.take(expression);

  if (!m_watches.contains(watchId)) {
    return;
  }

  WatchExpression &watch = m_watches[watchId];
  watch.value.clear();
  watch.type.clear();
  watch.variablesReference = 0;
  watch.isError = true;
  watch.errorMessage = errorMessage;

  emit watchUpdated(watch);
}

void WatchManager::onVariablesReceived(int variablesReference,
                                       const QList<DapVariable> &variables) {
  if (!m_pendingVariables.contains(variablesReference)) {
    return;
  }

  int watchId = m_pendingVariables.take(variablesReference);
  emit watchChildrenReceived(watchId, variables);
}

QJsonObject WatchManager::saveToJson() const {
  QJsonObject root;

  QJsonArray watchesArray;
  for (const auto &watch : m_watches) {
    watchesArray.append(watch.toJson());
  }
  root["watches"] = watchesArray;

  return root;
}

void WatchManager::loadFromJson(const QJsonObject &json) {
  clearAll();

  QJsonArray watchesArray = json["watches"].toArray();
  for (const auto &val : watchesArray) {
    WatchExpression watch = WatchExpression::fromJson(val.toObject());
    // Assign a new ID - IDs are session-local and not persisted
    watch.id = m_nextId++;
    m_watches[watch.id] = watch;
  }
}

bool WatchManager::saveToFile(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(QString("Failed to save watches to: %1").arg(filePath));
    return false;
  }

  QJsonDocument doc(saveToJson());
  file.write(doc.toJson());
  return true;
}

bool WatchManager::loadFromFile(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);

  if (error.error != QJsonParseError::NoError) {
    LOG_ERROR(
        QString("Failed to parse watches file: %1").arg(error.errorString()));
    return false;
  }

  loadFromJson(doc.object());
  return true;
}

void WatchManager::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
}

QString WatchManager::lightpadWatchesPath() const {
  if (m_workspaceFolder.isEmpty()) {
    return QString();
  }
  return m_workspaceFolder + "/.lightpad/debug/watches.json";
}

bool WatchManager::loadFromLightpadDir() {
  QString path = lightpadWatchesPath();
  if (path.isEmpty()) {
    LOG_WARNING("Cannot load watches: workspace folder not set");
    return false;
  }

  // Ensure directory exists
  QDir dir(m_workspaceFolder + "/.lightpad/debug");
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  // If file doesn't exist, create a default one
  if (!QFile::exists(path)) {
    LOG_INFO("Creating default watches.json in .lightpad/debug/");

    QJsonObject root;
    root["version"] = "1.0.0";
    root["_comment"] =
        "Watch expressions. Add expressions to monitor during debugging.";
    root["watches"] = QJsonArray();

    QJsonArray examples;
    examples.append("myVariable");
    examples.append("array.length");
    examples.append("object.property");
    root["_examples"] = examples;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
      QJsonDocument doc(root);
      file.write(doc.toJson(QJsonDocument::Indented));
    }
  }

  return loadFromFile(path);
}

bool WatchManager::saveToLightpadDir() {
  QString path = lightpadWatchesPath();
  if (path.isEmpty()) {
    LOG_WARNING("Cannot save watches: workspace folder not set");
    return false;
  }

  // Ensure directory exists
  QDir dir(m_workspaceFolder + "/.lightpad/debug");
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  QJsonObject root;
  root["version"] = "1.0.0";
  root["_comment"] =
      "Watch expressions. Add expressions to monitor during debugging.";

  QJsonArray watchesArray;
  for (const WatchExpression &watch : m_watches) {
    QJsonObject watchObj;
    watchObj["expression"] = watch.expression;
    watchesArray.append(watchObj);
  }
  root["watches"] = watchesArray;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(QString("Failed to save watches to: %1").arg(path));
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));

  LOG_INFO(QString("Saved watches to %1").arg(path));
  return true;
}
