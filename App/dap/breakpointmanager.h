#ifndef BREAKPOINTMANAGER_H
#define BREAKPOINTMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include "dapclient.h"

struct Breakpoint {
  int id = 0;
  QString filePath;
  int line = 0;
  int column = 0;
  bool enabled = true;

  QString condition;
  QString hitCondition;

  QString logMessage;
  bool isLogpoint = false;

  bool verified = false;
  QString verificationMessage;
  int boundLine = 0;

  QJsonObject toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["filePath"] = filePath;
    obj["line"] = line;
    obj["column"] = column;
    obj["enabled"] = enabled;
    obj["condition"] = condition;
    obj["hitCondition"] = hitCondition;
    obj["logMessage"] = logMessage;
    obj["isLogpoint"] = isLogpoint;
    return obj;
  }

  static Breakpoint fromJson(const QJsonObject &obj) {
    Breakpoint bp;
    bp.id = obj["id"].toInt();
    bp.filePath = obj["filePath"].toString();
    bp.line = obj["line"].toInt();
    bp.column = obj["column"].toInt();
    bp.enabled = obj["enabled"].toBool(true);
    bp.condition = obj["condition"].toString();
    bp.hitCondition = obj["hitCondition"].toString();
    bp.logMessage = obj["logMessage"].toString();
    bp.isLogpoint = obj["isLogpoint"].toBool();
    return bp;
  }
};

struct FunctionBreakpoint {
  int id = 0;
  QString functionName;
  bool enabled = true;
  QString condition;
  QString hitCondition;
  bool verified = false;
};

struct DataBreakpoint {
  int id = 0;
  QString dataId;
  QString accessType;
  QString condition;
  QString hitCondition;
  bool enabled = true;
  bool verified = false;
  QString description;

  QJsonObject toJson() const {
    QJsonObject obj;
    obj["dataId"] = dataId;
    obj["accessType"] = accessType;
    if (!condition.isEmpty())
      obj["condition"] = condition;
    if (!hitCondition.isEmpty())
      obj["hitCondition"] = hitCondition;
    return obj;
  }
};

struct ExceptionBreakpoint {
  QString filterId;
  QString filterLabel;
  bool enabled = false;
  QString condition;
};

class BreakpointManager : public QObject {
  Q_OBJECT

public:
  static BreakpointManager &instance();

  Breakpoint toggleBreakpoint(const QString &filePath, int line);

  int addBreakpoint(const Breakpoint &bp);

  void removeBreakpoint(int id);

  void removeBreakpoint(const QString &filePath, int line);

  void clearAll();

  void clearFile(const QString &filePath);

  Breakpoint breakpoint(int id) const;

  QList<Breakpoint> breakpointsForFile(const QString &filePath) const;

  QList<Breakpoint> allBreakpoints() const;

  bool hasBreakpoint(const QString &filePath, int line) const;

  Breakpoint breakpointAt(const QString &filePath, int line) const;

  void setEnabled(int id, bool enabled);

  void setCondition(int id, const QString &condition);

  void setHitCondition(int id, const QString &hitCondition);

  void setLogMessage(int id, const QString &message);

  void setDapClient(DapClient *client);

  void syncAllBreakpoints();

  void syncFileBreakpoints(const QString &filePath);

  void updateVerification(const QString &filePath,
                          const QList<DapBreakpoint> &verified);

  int addFunctionBreakpoint(const QString &functionName);

  void removeFunctionBreakpoint(int id);

  QList<FunctionBreakpoint> allFunctionBreakpoints() const;

  void syncFunctionBreakpoints();

  int addDataBreakpoint(const QString &dataId,
                        const QString &accessType = "write");

  void removeDataBreakpoint(int id);

  QList<DataBreakpoint> allDataBreakpoints() const;

  void syncDataBreakpoints();

  void setExceptionBreakpoints(const QStringList &filterIds);

  QStringList enabledExceptionFilters() const;

  QJsonObject saveToJson() const;

  void loadFromJson(const QJsonObject &json);

  bool saveToFile(const QString &filePath);

  bool loadFromFile(const QString &filePath);

  void setWorkspaceFolder(const QString &folder);

  bool loadFromLightpadDir();

  bool saveToLightpadDir();

  QString lightpadBreakpointsPath() const;

signals:

  void breakpointAdded(const Breakpoint &bp);

  void breakpointRemoved(int id, const QString &filePath, int line);

  void breakpointChanged(const Breakpoint &bp);

  void fileBreakpointsChanged(const QString &filePath);

  void allBreakpointsCleared();

  void dataBreakpointsChanged();

  void exceptionBreakpointsChanged();

private:
  BreakpointManager();
  ~BreakpointManager() = default;
  BreakpointManager(const BreakpointManager &) = delete;
  BreakpointManager &operator=(const BreakpointManager &) = delete;

  DapSourceBreakpoint toSourceBreakpoint(const Breakpoint &bp) const;

  int m_nextId;
  QMap<int, Breakpoint> m_breakpoints;
  QMap<QString, QList<int>> m_fileBreakpoints;

  QMap<int, FunctionBreakpoint> m_functionBreakpoints;
  int m_nextFunctionBpId;

  QMap<int, DataBreakpoint> m_dataBreakpoints;
  int m_nextDataBpId;

  QStringList m_enabledExceptionFilters;

  DapClient *m_dapClient;
  QString m_workspaceFolder;
};

#endif
