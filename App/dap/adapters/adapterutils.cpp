#include "adapterutils.h"

#include "../debugsettings.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

QString resolveExecutablePath(const QString &candidate) {
  if (candidate.trimmed().isEmpty()) {
    return {};
  }

  const QFileInfo fileInfo(candidate);
  if (fileInfo.exists() && fileInfo.isExecutable()) {
    return fileInfo.absoluteFilePath();
  }

  return QStandardPaths::findExecutable(candidate.trimmed());
}

QString debugAdapterSettingValue(const QString &adapterId, const QString &key) {
  const QJsonObject adapterObject = debugAdapterSettingsObject(adapterId);
  return adapterObject[key].toString().trimmed();
}

QJsonObject debugAdapterSettingsObject(const QString &adapterId) {
  const QJsonObject adapterSettings =
      DebugSettings::instance().adapterSettings()["adapters"].toObject();
  QJsonObject adapterObject = adapterSettings[adapterId].toObject();

  for (auto it = adapterObject.begin(); it != adapterObject.end();) {
    if (it.key().startsWith('_')) {
      it = adapterObject.erase(it);
    } else {
      ++it;
    }
  }

  return adapterObject;
}

ProcessProbeResult runProcessProbe(const QString &program,
                                   const QStringList &arguments,
                                   int timeoutMs) {
  ProcessProbeResult result;
  if (program.trimmed().isEmpty()) {
    return result;
  }

  QProcess proc;
  proc.start(program, arguments);
  if (!proc.waitForFinished(timeoutMs)) {
    return result;
  }

  result.stdoutData = proc.readAllStandardOutput();
  result.stderrData = proc.readAllStandardError();
  result.success = proc.exitCode() == 0;
  return result;
}

QString firstOutputLine(const QByteArray &stdoutData,
                        const QByteArray &stderrData) {
  const QString combined =
      QString::fromUtf8(stdoutData.isEmpty() ? stderrData : stdoutData);
  return combined.split('\n').first().simplified();
}
