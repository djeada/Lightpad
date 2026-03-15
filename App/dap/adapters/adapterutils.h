#ifndef DEBUGADAPTERUTILS_H
#define DEBUGADAPTERUTILS_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

struct ProcessProbeResult {
  bool success = false;
  QByteArray stdoutData;
  QByteArray stderrData;
};

QString resolveExecutablePath(const QString &candidate);
QString debugAdapterSettingValue(const QString &adapterId, const QString &key);
QJsonObject debugAdapterSettingsObject(const QString &adapterId);
ProcessProbeResult runProcessProbe(const QString &program,
                                   const QStringList &arguments,
                                   int timeoutMs = 5000);
QString firstOutputLine(const QByteArray &stdoutData,
                        const QByteArray &stderrData = {});

#endif
