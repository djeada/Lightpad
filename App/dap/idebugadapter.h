#ifndef IDEBUGADAPTER_H
#define IDEBUGADAPTER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

struct DebugAdapterConfig {
  QString id;
  QString name;
  QString type;
  QString program;
  QStringList arguments;
  QStringList languages;

  QStringList extensions;

  QJsonObject defaultLaunchConfig;
  QJsonObject defaultAttachConfig;

  QJsonArray exceptionBreakpointFilters;

  bool supportsRestart = false;
  bool supportsTerminate = true;
  bool supportsFunctionBreakpoints = false;
  bool supportsConditionalBreakpoints = true;
  bool supportsHitConditionalBreakpoints = true;
  bool supportsLogPoints = true;
};

class IDebugAdapter {
public:
  virtual ~IDebugAdapter() = default;

  virtual DebugAdapterConfig config() const = 0;

  virtual bool isAvailable() const = 0;

  virtual QString statusMessage() const = 0;

  virtual QJsonObject
  createLaunchConfig(const QString &filePath,
                     const QString &workingDir = {}) const = 0;

  virtual QJsonObject createAttachConfig(int processId = 0,
                                         const QString &host = {},
                                         int port = 0) const = 0;

  virtual bool canDebug(const QString &filePath) const {
    const DebugAdapterConfig &cfg = config();
    for (const QString &ext : cfg.extensions) {
      if (filePath.endsWith(ext, Qt::CaseInsensitive)) {
        return true;
      }
    }
    return false;
  }

  virtual bool supportsLanguage(const QString &languageId) const {
    return config().languages.contains(languageId, Qt::CaseInsensitive);
  }

  virtual QString installCommand() const { return {}; }

  virtual QString documentationUrl() const { return {}; }
};

#define IDebugAdapter_iid "org.lightpad.IDebugAdapter/1.0"
Q_DECLARE_INTERFACE(IDebugAdapter, IDebugAdapter_iid)

#endif
