#ifndef PROCESSBACKEDDEBUGADAPTER_H
#define PROCESSBACKEDDEBUGADAPTER_H

#include "abstractdebugadapter.h"
#include "adapterutils.h"

#include <QFileInfo>

class ProcessBackedDebugAdapter : public AbstractDebugAdapter {
public:
  ProcessBackedDebugAdapter(DebugAdapterConfig baseConfig,
                            QString adapterExecutableKey,
                            QString adapterExecutableLabel,
                            QString adapterExecutablePlaceholder,
                            QList<DebugAdapterOption> additionalOptions = {})
      : AbstractDebugAdapter(std::move(baseConfig),
                             prependAdapterExecutableOption(
                                 adapterExecutableKey,
                                 std::move(adapterExecutableLabel),
                                 std::move(adapterExecutablePlaceholder),
                                 std::move(additionalOptions))),
        m_adapterExecutableKey(adapterExecutableKey.trimmed()) {}

  DebugAdapterConfig configForConfiguration(
      const DebugConfiguration &configuration) const override {
    DebugAdapterConfig cfg =
        AbstractDebugAdapter::configForConfiguration(configuration);
    const QString executable = resolvedAdapterExecutable(configuration);
    if (!executable.isEmpty()) {
      cfg.program = executable;
    } else {
      cfg.program = adapterExecutableCandidate(configuration);
    }
    return cfg;
  }

  bool isAvailable() const override {
    return isAvailableForConfiguration(DebugConfiguration{});
  }

  bool isAvailableForConfiguration(
      const DebugConfiguration &configuration) const override {
    const QString executable = resolvedAdapterExecutable(configuration);
    if (executable.isEmpty()) {
      return false;
    }

    const QStringList probeArguments = executableProbeArguments(configuration);
    if (probeArguments.isEmpty()) {
      return true;
    }

    return runProcessProbe(executable, probeArguments,
                           executableProbeTimeoutMs())
        .success;
  }

  QString statusMessage() const override {
    return statusMessageForConfiguration(DebugConfiguration{});
  }

  QString statusMessageForConfiguration(
      const DebugConfiguration &configuration) const override {
    const QString candidate = adapterExecutableCandidate(configuration);
    const QString executable = resolvedAdapterExecutable(configuration);
    if (executable.isEmpty()) {
      return missingExecutableMessage(configuration, candidate);
    }

    const QStringList probeArguments = executableProbeArguments(configuration);
    if (probeArguments.isEmpty()) {
      return readyMessage(configuration, executable, ProcessProbeResult{});
    }

    const ProcessProbeResult probe =
        runProcessProbe(executable, probeArguments, executableProbeTimeoutMs());
    if (!probe.success) {
      return unavailableMessage(configuration, executable, probe);
    }

    return readyMessage(configuration, executable, probe);
  }

protected:
  QString adapterExecutableKey() const { return m_adapterExecutableKey; }

  QString optionValue(const DebugConfiguration &configuration,
                      const QString &key) const {
    return mergedAdapterConfig(configuration)[key].toString().trimmed();
  }

  virtual QString defaultAdapterExecutableCandidate(
      const DebugConfiguration &configuration) const {
    Q_UNUSED(configuration);
    return config().program.trimmed();
  }

  virtual QString
  adapterExecutableCandidate(const DebugConfiguration &configuration) const {
    if (!m_adapterExecutableKey.isEmpty()) {
      const QString configured =
          optionValue(configuration, m_adapterExecutableKey);
      if (!configured.isEmpty()) {
        return configured;
      }
    }
    return defaultAdapterExecutableCandidate(configuration);
  }

  virtual QString
  resolvedAdapterExecutable(const DebugConfiguration &configuration) const {
    return resolveExecutablePath(adapterExecutableCandidate(configuration));
  }

  virtual QStringList
  executableProbeArguments(const DebugConfiguration &configuration) const {
    Q_UNUSED(configuration);
    return {};
  }

  virtual int executableProbeTimeoutMs() const { return 3000; }

  virtual QString
  missingExecutableMessage(const DebugConfiguration &configuration,
                           const QString &candidate) const {
    Q_UNUSED(configuration);
    if (!candidate.trimmed().isEmpty()) {
      return QString("%1 '%2' not found")
          .arg(adapterExecutableLabel(), candidate.trimmed());
    }
    return QString("%1 not configured").arg(adapterExecutableLabel());
  }

  virtual QString unavailableMessage(const DebugConfiguration &configuration,
                                     const QString &executable,
                                     const ProcessProbeResult &probe) const {
    Q_UNUSED(configuration);
    Q_UNUSED(executable);
    Q_UNUSED(probe);
    return QString(
               "%1 is installed but did not respond to the availability probe")
        .arg(adapterExecutableLabel());
  }

  virtual QString readyMessage(const DebugConfiguration &configuration,
                               const QString &executable,
                               const ProcessProbeResult &probe) const {
    Q_UNUSED(configuration);
    const QString line = firstOutputLine(probe.stdoutData, probe.stderrData);
    if (!line.isEmpty()) {
      return QString("Ready (%1)").arg(line);
    }
    return QString("Ready (%1)").arg(QFileInfo(executable).fileName());
  }

private:
  static QList<DebugAdapterOption>
  prependAdapterExecutableOption(const QString &key, QString label,
                                 QString placeholder,
                                 QList<DebugAdapterOption> additionalOptions) {
    QList<DebugAdapterOption> options;
    if (!key.isEmpty()) {
      DebugAdapterOption option;
      option.key = std::move(key);
      option.label = std::move(label);
      option.placeholder = std::move(placeholder);
      options.append(option);
    }
    options.append(additionalOptions);
    return options;
  }

  QString adapterExecutableLabel() const {
    return configurationOptions().isEmpty()
               ? QStringLiteral("Adapter executable")
               : configurationOptions().first().label.trimmed();
  }

  QString m_adapterExecutableKey;
};

#endif
