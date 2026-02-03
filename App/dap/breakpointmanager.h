#ifndef BREAKPOINTMANAGER_H
#define BREAKPOINTMANAGER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include "dapclient.h"

/**
 * @brief User-defined breakpoint
 *
 * Represents a breakpoint as set by the user in the IDE.
 * This is separate from DapBreakpoint which represents the
 * verified/bound breakpoint from the debug adapter.
 */
struct Breakpoint {
  int id = 0;          // Local ID (assigned by manager)
  QString filePath;    // Absolute file path
  int line = 0;        // 1-based line number
  int column = 0;      // Optional column (0 = any)
  bool enabled = true; // Whether breakpoint is active

  // Conditional breakpoint
  QString condition;    // Expression that must be true
  QString hitCondition; // Hit count condition (e.g., ">= 5")

  // Logpoint (doesn't stop, just logs)
  QString logMessage; // Message to log (can include {expressions})
  bool isLogpoint = false;

  // Verification state (from debug adapter)
  bool verified = false;
  QString verificationMessage;
  int boundLine = 0; // Actual line where breakpoint was bound

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

/**
 * @brief Function breakpoint
 */
struct FunctionBreakpoint {
  int id = 0;
  QString functionName;
  bool enabled = true;
  QString condition;
  QString hitCondition;
  bool verified = false;
};

/**
 * @brief Data breakpoint (triggers when data changes)
 *
 * Data breakpoints watch memory locations or variable values
 * and trigger when the value changes (write), is read (read),
 * or accessed at all (readWrite).
 */
struct DataBreakpoint {
  int id = 0;
  QString dataId;       // Identifier for the data location
  QString accessType;   // "read", "write", "readWrite"
  QString condition;    // Optional condition
  QString hitCondition; // Hit count condition
  bool enabled = true;
  bool verified = false;
  QString description; // Human-readable description

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

/**
 * @brief Exception breakpoint configuration
 */
struct ExceptionBreakpoint {
  QString filterId;    // Filter ID from adapter capabilities
  QString filterLabel; // Display label
  bool enabled = false;
  QString condition; // Optional condition (if supported)
};

/**
 * @brief Manages breakpoints across all files
 *
 * The BreakpointManager is responsible for:
 * - Storing and persisting user breakpoints
 * - Syncing breakpoints with the debug adapter
 * - Providing breakpoint information to the editor gutter
 * - Handling breakpoint verification feedback
 */
class BreakpointManager : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static BreakpointManager &instance();

  /**
   * @brief Add or toggle a breakpoint at a line
   *
   * If a breakpoint exists at the line, it's removed.
   * Otherwise, a new breakpoint is added.
   *
   * @param filePath File path
   * @param line 1-based line number
   * @return The breakpoint (new or removed)
   */
  Breakpoint toggleBreakpoint(const QString &filePath, int line);

  /**
   * @brief Add a breakpoint
   *
   * @param bp Breakpoint to add
   * @return ID of the added breakpoint
   */
  int addBreakpoint(const Breakpoint &bp);

  /**
   * @brief Remove a breakpoint by ID
   *
   * @param id Breakpoint ID
   */
  void removeBreakpoint(int id);

  /**
   * @brief Remove a breakpoint at a specific location
   *
   * @param filePath File path
   * @param line Line number
   */
  void removeBreakpoint(const QString &filePath, int line);

  /**
   * @brief Clear all breakpoints
   */
  void clearAll();

  /**
   * @brief Clear breakpoints for a file
   *
   * @param filePath File path
   */
  void clearFile(const QString &filePath);

  /**
   * @brief Get a breakpoint by ID
   */
  Breakpoint breakpoint(int id) const;

  /**
   * @brief Get all breakpoints for a file
   */
  QList<Breakpoint> breakpointsForFile(const QString &filePath) const;

  /**
   * @brief Get all breakpoints
   */
  QList<Breakpoint> allBreakpoints() const;

  /**
   * @brief Check if there's a breakpoint at a line
   */
  bool hasBreakpoint(const QString &filePath, int line) const;

  /**
   * @brief Get the breakpoint at a specific line
   *
   * @return Breakpoint with id=0 if not found
   */
  Breakpoint breakpointAt(const QString &filePath, int line) const;

  /**
   * @brief Enable/disable a breakpoint
   */
  void setEnabled(int id, bool enabled);

  /**
   * @brief Update breakpoint condition
   */
  void setCondition(int id, const QString &condition);

  /**
   * @brief Update hit condition
   */
  void setHitCondition(int id, const QString &hitCondition);

  /**
   * @brief Convert to logpoint
   */
  void setLogMessage(int id, const QString &message);

  /**
   * @brief Set the DAP client to sync breakpoints with
   */
  void setDapClient(DapClient *client);

  /**
   * @brief Sync all breakpoints with the debug adapter
   *
   * Call this after launching/attaching to send all breakpoints.
   */
  void syncAllBreakpoints();

  /**
   * @brief Sync breakpoints for a specific file
   */
  void syncFileBreakpoints(const QString &filePath);

  /**
   * @brief Update breakpoint verification status from debug adapter
   */
  void updateVerification(const QString &filePath,
                          const QList<DapBreakpoint> &verified);

  // Function breakpoints

  /**
   * @brief Add a function breakpoint
   */
  int addFunctionBreakpoint(const QString &functionName);

  /**
   * @brief Remove a function breakpoint
   */
  void removeFunctionBreakpoint(int id);

  /**
   * @brief Get all function breakpoints
   */
  QList<FunctionBreakpoint> allFunctionBreakpoints() const;

  /**
   * @brief Sync function breakpoints with debug adapter
   */
  void syncFunctionBreakpoints();

  // Data breakpoints

  /**
   * @brief Add a data breakpoint
   * @param dataId Data identifier (e.g., variable name, memory address)
   * @param accessType "read", "write", or "readWrite"
   * @return Breakpoint ID
   */
  int addDataBreakpoint(const QString &dataId,
                        const QString &accessType = "write");

  /**
   * @brief Remove a data breakpoint
   */
  void removeDataBreakpoint(int id);

  /**
   * @brief Get all data breakpoints
   */
  QList<DataBreakpoint> allDataBreakpoints() const;

  /**
   * @brief Sync data breakpoints with debug adapter
   */
  void syncDataBreakpoints();

  // Exception breakpoints

  /**
   * @brief Set exception breakpoint filters
   * @param filterIds List of filter IDs to enable
   */
  void setExceptionBreakpoints(const QStringList &filterIds);

  /**
   * @brief Get enabled exception filter IDs
   */
  QStringList enabledExceptionFilters() const;

  // Persistence

  /**
   * @brief Save breakpoints to JSON
   */
  QJsonObject saveToJson() const;

  /**
   * @brief Load breakpoints from JSON
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
   * @brief Load breakpoints from .lightpad/debug/breakpoints.json
   */
  bool loadFromLightpadDir();

  /**
   * @brief Save breakpoints to .lightpad/debug/breakpoints.json
   */
  bool saveToLightpadDir();

  /**
   * @brief Get path to .lightpad breakpoints file
   */
  QString lightpadBreakpointsPath() const;

signals:
  /**
   * @brief Emitted when a breakpoint is added
   */
  void breakpointAdded(const Breakpoint &bp);

  /**
   * @brief Emitted when a breakpoint is removed
   */
  void breakpointRemoved(int id, const QString &filePath, int line);

  /**
   * @brief Emitted when a breakpoint is changed
   */
  void breakpointChanged(const Breakpoint &bp);

  /**
   * @brief Emitted when breakpoints for a file change
   */
  void fileBreakpointsChanged(const QString &filePath);

  /**
   * @brief Emitted when all breakpoints are cleared
   */
  void allBreakpointsCleared();

  /**
   * @brief Emitted when data breakpoints change
   */
  void dataBreakpointsChanged();

  /**
   * @brief Emitted when exception breakpoints change
   */
  void exceptionBreakpointsChanged();

private:
  BreakpointManager();
  ~BreakpointManager() = default;
  BreakpointManager(const BreakpointManager &) = delete;
  BreakpointManager &operator=(const BreakpointManager &) = delete;

  DapSourceBreakpoint toSourceBreakpoint(const Breakpoint &bp) const;

  int m_nextId;
  QMap<int, Breakpoint> m_breakpoints;         // id -> breakpoint
  QMap<QString, QList<int>> m_fileBreakpoints; // filePath -> list of IDs

  QMap<int, FunctionBreakpoint> m_functionBreakpoints;
  int m_nextFunctionBpId;

  QMap<int, DataBreakpoint> m_dataBreakpoints;
  int m_nextDataBpId;

  QStringList m_enabledExceptionFilters;

  DapClient *m_dapClient;
  QString m_workspaceFolder;
};

#endif // BREAKPOINTMANAGER_H
