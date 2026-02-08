#include "breakpointmanager.h"
#include "../core/logging/logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace {
QString normalizePath(const QString &path) {
  if (path.isEmpty()) {
    return path;
  }
  QFileInfo fi(path);
  const QString canonical = fi.canonicalFilePath();
  if (!canonical.isEmpty()) {
    return QDir::cleanPath(canonical);
  }
  if (fi.isAbsolute()) {
    return QDir::cleanPath(fi.absoluteFilePath());
  }
  return QDir::cleanPath(path);
}
} // namespace

BreakpointManager &BreakpointManager::instance() {
  static BreakpointManager instance;
  return instance;
}

BreakpointManager::BreakpointManager()
    : QObject(nullptr), m_nextId(1), m_nextFunctionBpId(1), m_nextDataBpId(1),
      m_dapClient(nullptr) {}

Breakpoint BreakpointManager::toggleBreakpoint(const QString &filePath,
                                               int line) {
  if (hasBreakpoint(filePath, line)) {
    Breakpoint existing = breakpointAt(filePath, line);
    removeBreakpoint(existing.id);
    return existing;
  } else {
    Breakpoint bp;
    bp.filePath = filePath;
    bp.line = line;
    bp.enabled = true;
    addBreakpoint(bp);
    return bp;
  }
}

int BreakpointManager::addBreakpoint(const Breakpoint &bp) {
  Breakpoint newBp = bp;
  newBp.id = m_nextId++;

  m_breakpoints[newBp.id] = newBp;
  m_fileBreakpoints[newBp.filePath].append(newBp.id);

  LOG_DEBUG(QString("Added breakpoint %1 at %2:%3")
                .arg(newBp.id)
                .arg(newBp.filePath)
                .arg(newBp.line));

  emit breakpointAdded(newBp);
  emit fileBreakpointsChanged(newBp.filePath);

  // Sync with debug adapter if debugging
  if (m_dapClient && m_dapClient->isDebugging()) {
    syncFileBreakpoints(newBp.filePath);
  }

  return newBp.id;
}

void BreakpointManager::removeBreakpoint(int id) {
  if (!m_breakpoints.contains(id)) {
    return;
  }

  Breakpoint bp = m_breakpoints.take(id);
  m_fileBreakpoints[bp.filePath].removeAll(id);

  if (m_fileBreakpoints[bp.filePath].isEmpty()) {
    m_fileBreakpoints.remove(bp.filePath);
  }

  LOG_DEBUG(QString("Removed breakpoint %1 at %2:%3")
                .arg(id)
                .arg(bp.filePath)
                .arg(bp.line));

  emit breakpointRemoved(id, bp.filePath, bp.line);
  emit fileBreakpointsChanged(bp.filePath);

  // Sync with debug adapter if debugging
  if (m_dapClient && m_dapClient->isDebugging()) {
    syncFileBreakpoints(bp.filePath);
  }
}

void BreakpointManager::removeBreakpoint(const QString &filePath, int line) {
  Breakpoint bp = breakpointAt(filePath, line);
  if (bp.id > 0) {
    removeBreakpoint(bp.id);
  }
}

void BreakpointManager::clearAll() {
  QStringList affectedFiles = m_fileBreakpoints.keys();

  m_breakpoints.clear();
  m_fileBreakpoints.clear();

  emit allBreakpointsCleared();

  // Sync each affected file with debug adapter
  if (m_dapClient && m_dapClient->isDebugging()) {
    for (const QString &file : affectedFiles) {
      syncFileBreakpoints(file);
    }
  }
}

void BreakpointManager::clearFile(const QString &filePath) {
  QList<int> ids = m_fileBreakpoints.take(filePath);

  for (int id : ids) {
    Breakpoint bp = m_breakpoints.take(id);
    emit breakpointRemoved(id, filePath, bp.line);
  }

  emit fileBreakpointsChanged(filePath);

  // Sync with debug adapter
  if (m_dapClient && m_dapClient->isDebugging()) {
    syncFileBreakpoints(filePath);
  }
}

Breakpoint BreakpointManager::breakpoint(int id) const {
  return m_breakpoints.value(id);
}

QList<Breakpoint>
BreakpointManager::breakpointsForFile(const QString &filePath) const {
  QList<Breakpoint> result;
  QList<int> ids = m_fileBreakpoints.value(filePath);
  for (int id : ids) {
    result.append(m_breakpoints.value(id));
  }
  return result;
}

QList<Breakpoint> BreakpointManager::allBreakpoints() const {
  return m_breakpoints.values();
}

bool BreakpointManager::hasBreakpoint(const QString &filePath, int line) const {
  QList<int> ids = m_fileBreakpoints.value(filePath);
  for (int id : ids) {
    const Breakpoint &bp = m_breakpoints.value(id);
    if (bp.line == line) {
      return true;
    }
  }
  return false;
}

Breakpoint BreakpointManager::breakpointAt(const QString &filePath,
                                           int line) const {
  QList<int> ids = m_fileBreakpoints.value(filePath);
  for (int id : ids) {
    const Breakpoint &bp = m_breakpoints.value(id);
    if (bp.line == line) {
      return bp;
    }
  }
  return Breakpoint(); // Return empty breakpoint
}

void BreakpointManager::setEnabled(int id, bool enabled) {
  if (!m_breakpoints.contains(id)) {
    return;
  }

  m_breakpoints[id].enabled = enabled;
  emit breakpointChanged(m_breakpoints[id]);

  if (m_dapClient && m_dapClient->isDebugging()) {
    syncFileBreakpoints(m_breakpoints[id].filePath);
  }
}

void BreakpointManager::setCondition(int id, const QString &condition) {
  if (!m_breakpoints.contains(id)) {
    return;
  }

  m_breakpoints[id].condition = condition;
  m_breakpoints[id].isLogpoint = false;
  emit breakpointChanged(m_breakpoints[id]);

  if (m_dapClient && m_dapClient->isDebugging()) {
    syncFileBreakpoints(m_breakpoints[id].filePath);
  }
}

void BreakpointManager::setHitCondition(int id, const QString &hitCondition) {
  if (!m_breakpoints.contains(id)) {
    return;
  }

  m_breakpoints[id].hitCondition = hitCondition;
  emit breakpointChanged(m_breakpoints[id]);

  if (m_dapClient && m_dapClient->isDebugging()) {
    syncFileBreakpoints(m_breakpoints[id].filePath);
  }
}

void BreakpointManager::setLogMessage(int id, const QString &message) {
  if (!m_breakpoints.contains(id)) {
    return;
  }

  m_breakpoints[id].logMessage = message;
  m_breakpoints[id].isLogpoint = !message.isEmpty();
  emit breakpointChanged(m_breakpoints[id]);

  if (m_dapClient && m_dapClient->isDebugging()) {
    syncFileBreakpoints(m_breakpoints[id].filePath);
  }
}

void BreakpointManager::setDapClient(DapClient *client) {
  m_dapClient = client;
}

void BreakpointManager::syncAllBreakpoints() {
  if (!m_dapClient) {
    return;
  }

  LOG_DEBUG("Syncing all breakpoints with debug adapter");

  for (const QString &filePath : m_fileBreakpoints.keys()) {
    syncFileBreakpoints(filePath);
  }

  syncFunctionBreakpoints();
  syncDataBreakpoints();

  if (!m_enabledExceptionFilters.isEmpty()) {
    m_dapClient->setExceptionBreakpoints(m_enabledExceptionFilters);
  }
}

void BreakpointManager::syncFileBreakpoints(const QString &filePath) {
  if (!m_dapClient) {
    return;
  }

  QList<DapSourceBreakpoint> dapBreakpoints;
  QList<Breakpoint> bps = breakpointsForFile(filePath);

  for (const Breakpoint &bp : bps) {
    if (bp.enabled) {
      dapBreakpoints.append(toSourceBreakpoint(bp));
    }
  }

  m_dapClient->setBreakpoints(normalizePath(filePath), dapBreakpoints);
}

void BreakpointManager::updateVerification(
    const QString &filePath, const QList<DapBreakpoint> &verified) {
  const QString requestedPath = normalizePath(filePath);
  QList<int> ids = m_fileBreakpoints.value(filePath);
  if (ids.isEmpty()) {
    for (auto it = m_fileBreakpoints.constBegin(); it != m_fileBreakpoints.constEnd();
         ++it) {
      if (normalizePath(it.key()) == requestedPath) {
        ids = it.value();
        break;
      }
    }
  }

  // Match verified breakpoints to our breakpoints by line
  for (int id : ids) {
    Breakpoint &bp = m_breakpoints[id];

    for (const DapBreakpoint &dapBp : verified) {
      // Match by line (DAP breakpoint might have moved)
      if (dapBp.line == bp.line || dapBp.line == bp.boundLine) {
        bp.verified = dapBp.verified;
        bp.verificationMessage = dapBp.message;
        bp.boundLine = dapBp.line;
        emit breakpointChanged(bp);
        break;
      }
    }
  }
}

int BreakpointManager::addFunctionBreakpoint(const QString &functionName) {
  FunctionBreakpoint fbp;
  fbp.id = m_nextFunctionBpId++;
  fbp.functionName = functionName;
  fbp.enabled = true;

  m_functionBreakpoints[fbp.id] = fbp;

  if (m_dapClient && m_dapClient->isDebugging()) {
    syncFunctionBreakpoints();
  }

  return fbp.id;
}

void BreakpointManager::removeFunctionBreakpoint(int id) {
  m_functionBreakpoints.remove(id);

  if (m_dapClient && m_dapClient->isDebugging()) {
    syncFunctionBreakpoints();
  }
}

QList<FunctionBreakpoint> BreakpointManager::allFunctionBreakpoints() const {
  return m_functionBreakpoints.values();
}

void BreakpointManager::syncFunctionBreakpoints() {
  if (!m_dapClient) {
    return;
  }

  QStringList functionNames;
  for (const FunctionBreakpoint &fbp : m_functionBreakpoints) {
    if (fbp.enabled) {
      functionNames.append(fbp.functionName);
    }
  }

  m_dapClient->setFunctionBreakpoints(functionNames);
}

int BreakpointManager::addDataBreakpoint(const QString &dataId,
                                         const QString &accessType) {
  DataBreakpoint dbp;
  dbp.id = m_nextDataBpId++;
  dbp.dataId = dataId;
  dbp.accessType = accessType;
  dbp.enabled = true;

  m_dataBreakpoints[dbp.id] = dbp;

  LOG_DEBUG(QString("Added data breakpoint %1: %2 (%3)")
                .arg(dbp.id)
                .arg(dataId)
                .arg(accessType));

  emit dataBreakpointsChanged();

  if (m_dapClient && m_dapClient->isDebugging()) {
    syncDataBreakpoints();
  }

  return dbp.id;
}

void BreakpointManager::removeDataBreakpoint(int id) {
  if (m_dataBreakpoints.remove(id) > 0) {
    emit dataBreakpointsChanged();

    if (m_dapClient && m_dapClient->isDebugging()) {
      syncDataBreakpoints();
    }
  }
}

QList<DataBreakpoint> BreakpointManager::allDataBreakpoints() const {
  return m_dataBreakpoints.values();
}

void BreakpointManager::syncDataBreakpoints() {
  if (!m_dapClient) {
    return;
  }

  QList<QJsonObject> dataBreakpointList;
  for (const DataBreakpoint &dbp : m_dataBreakpoints) {
    if (dbp.enabled) {
      dataBreakpointList.append(dbp.toJson());
    }
  }

  m_dapClient->setDataBreakpoints(dataBreakpointList);

  LOG_DEBUG(
      QString("Syncing %1 data breakpoints").arg(dataBreakpointList.size()));
}

void BreakpointManager::setExceptionBreakpoints(const QStringList &filterIds) {
  m_enabledExceptionFilters = filterIds;

  emit exceptionBreakpointsChanged();

  if (m_dapClient && m_dapClient->isDebugging()) {
    m_dapClient->setExceptionBreakpoints(filterIds);
  }
}

QStringList BreakpointManager::enabledExceptionFilters() const {
  return m_enabledExceptionFilters;
}

DapSourceBreakpoint
BreakpointManager::toSourceBreakpoint(const Breakpoint &bp) const {
  DapSourceBreakpoint dapBp;
  dapBp.line = bp.line;
  dapBp.column = bp.column;
  dapBp.condition = bp.condition;
  dapBp.hitCondition = bp.hitCondition;
  dapBp.logMessage = bp.logMessage;
  return dapBp;
}

QJsonObject BreakpointManager::saveToJson() const {
  QJsonObject root;

  QJsonArray breakpointsArray;
  for (const Breakpoint &bp : m_breakpoints) {
    breakpointsArray.append(bp.toJson());
  }
  root["breakpoints"] = breakpointsArray;

  QJsonArray functionBpArray;
  for (const FunctionBreakpoint &fbp : m_functionBreakpoints) {
    QJsonObject obj;
    obj["id"] = fbp.id;
    obj["functionName"] = fbp.functionName;
    obj["enabled"] = fbp.enabled;
    obj["condition"] = fbp.condition;
    obj["hitCondition"] = fbp.hitCondition;
    functionBpArray.append(obj);
  }
  root["functionBreakpoints"] = functionBpArray;

  return root;
}

void BreakpointManager::loadFromJson(const QJsonObject &json) {
  clearAll();
  m_functionBreakpoints.clear();

  QJsonArray breakpointsArray = json["breakpoints"].toArray();
  for (const QJsonValue &val : breakpointsArray) {
    Breakpoint bp = Breakpoint::fromJson(val.toObject());
    bp.id = m_nextId++;
    m_breakpoints[bp.id] = bp;
    m_fileBreakpoints[bp.filePath].append(bp.id);
  }

  QJsonArray functionBpArray = json["functionBreakpoints"].toArray();
  for (const QJsonValue &val : functionBpArray) {
    QJsonObject obj = val.toObject();
    FunctionBreakpoint fbp;
    fbp.id = m_nextFunctionBpId++;
    fbp.functionName = obj["functionName"].toString();
    fbp.enabled = obj["enabled"].toBool(true);
    fbp.condition = obj["condition"].toString();
    fbp.hitCondition = obj["hitCondition"].toString();
    m_functionBreakpoints[fbp.id] = fbp;
  }
}

bool BreakpointManager::saveToFile(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(QString("Failed to save breakpoints to: %1").arg(filePath));
    return false;
  }

  QJsonDocument doc(saveToJson());
  file.write(doc.toJson());
  return true;
}

bool BreakpointManager::loadFromFile(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);

  if (error.error != QJsonParseError::NoError) {
    LOG_ERROR(QString("Failed to parse breakpoints file: %1")
                  .arg(error.errorString()));
    return false;
  }

  loadFromJson(doc.object());
  return true;
}

void BreakpointManager::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
}

QString BreakpointManager::lightpadBreakpointsPath() const {
  if (m_workspaceFolder.isEmpty()) {
    return QString();
  }
  return m_workspaceFolder + "/.lightpad/debug/breakpoints.json";
}

bool BreakpointManager::loadFromLightpadDir() {
  QString path = lightpadBreakpointsPath();
  if (path.isEmpty()) {
    LOG_WARNING("Cannot load breakpoints: workspace folder not set");
    return false;
  }

  // Ensure directory exists
  QDir dir(m_workspaceFolder + "/.lightpad/debug");
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  // If file doesn't exist, create a default one
  if (!QFile::exists(path)) {
    LOG_INFO("Creating default breakpoints.json in .lightpad/debug/");

    QJsonObject root;
    root["version"] = "1.0.0";
    root["_comment"] = "Breakpoints configuration. This file is auto-saved but "
                       "can be manually edited.";
    root["breakpoints"] = QJsonArray();
    root["functionBreakpoints"] = QJsonArray();
    root["dataBreakpoints"] = QJsonArray();

    QJsonObject exceptionBreakpoints;
    exceptionBreakpoints["uncaught"] = true;
    exceptionBreakpoints["raised"] = false;
    root["exceptionBreakpoints"] = exceptionBreakpoints;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
      QJsonDocument doc(root);
      file.write(doc.toJson(QJsonDocument::Indented));
    }
  }

  return loadFromFile(path);
}

bool BreakpointManager::saveToLightpadDir() {
  QString path = lightpadBreakpointsPath();
  if (path.isEmpty()) {
    LOG_WARNING("Cannot save breakpoints: workspace folder not set");
    return false;
  }

  // Ensure directory exists
  QDir dir(m_workspaceFolder + "/.lightpad/debug");
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  // Create enhanced JSON with all breakpoint types
  QJsonObject root;
  root["version"] = "1.0.0";
  root["_comment"] = "Breakpoints configuration. This file is auto-saved but "
                     "can be manually edited.";

  // Source breakpoints organized by file
  QJsonObject sourceBreakpoints;
  for (const QString &filePath : m_fileBreakpoints.keys()) {
    QJsonArray bpArray;
    for (int bpId : m_fileBreakpoints[filePath]) {
      if (m_breakpoints.contains(bpId)) {
        const Breakpoint &bp = m_breakpoints[bpId];
        QJsonObject bpObj;
        bpObj["line"] = bp.line;
        if (bp.column > 0)
          bpObj["column"] = bp.column;
        bpObj["enabled"] = bp.enabled;
        if (!bp.condition.isEmpty())
          bpObj["condition"] = bp.condition;
        if (!bp.hitCondition.isEmpty())
          bpObj["hitCondition"] = bp.hitCondition;
        if (bp.isLogpoint) {
          bpObj["logMessage"] = bp.logMessage;
        }
        bpArray.append(bpObj);
      }
    }
    if (!bpArray.isEmpty()) {
      sourceBreakpoints[filePath] = bpArray;
    }
  }
  root["sourceBreakpoints"] = sourceBreakpoints;

  // Function breakpoints
  QJsonArray functionBpArray;
  for (const FunctionBreakpoint &fbp : m_functionBreakpoints) {
    QJsonObject obj;
    obj["functionName"] = fbp.functionName;
    obj["enabled"] = fbp.enabled;
    if (!fbp.condition.isEmpty())
      obj["condition"] = fbp.condition;
    if (!fbp.hitCondition.isEmpty())
      obj["hitCondition"] = fbp.hitCondition;
    functionBpArray.append(obj);
  }
  root["functionBreakpoints"] = functionBpArray;

  // Data breakpoints
  QJsonArray dataBpArray;
  for (const DataBreakpoint &dbp : m_dataBreakpoints) {
    dataBpArray.append(dbp.toJson());
  }
  root["dataBreakpoints"] = dataBpArray;

  // Exception breakpoints
  QJsonObject exceptionBreakpoints;
  exceptionBreakpoints["_comment"] =
      "Exception breakpoint filters. Set to true to enable.";
  for (const QString &filter : m_enabledExceptionFilters) {
    exceptionBreakpoints[filter] = true;
  }
  root["exceptionBreakpoints"] = exceptionBreakpoints;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(QString("Failed to save breakpoints to: %1").arg(path));
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));

  LOG_INFO(QString("Saved breakpoints to %1").arg(path));
  return true;
}
