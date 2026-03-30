#include "autotestrunner.h"
#include "testrunmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

AutoTestRunner::AutoTestRunner(TestRunManager *runManager, QObject *parent)
    : QObject(parent), m_runManager(runManager) {
  m_debounceTimer.setSingleShot(true);
  m_debounceTimer.setInterval(DEFAULT_DEBOUNCE_MS);
  connect(&m_debounceTimer, &QTimer::timeout, this,
          &AutoTestRunner::onDebounceTimeout);

  if (m_runManager) {
    connect(m_runManager, &TestRunManager::runFinished, this,
            &AutoTestRunner::onRunFinished);
  }
}

AutoTestRunner::~AutoTestRunner() = default;

void AutoTestRunner::setEnabled(bool enabled) {
  if (m_enabled == enabled)
    return;
  m_enabled = enabled;
  if (!m_enabled) {
    m_debounceTimer.stop();
  }
  emit enabledChanged(m_enabled);
}

bool AutoTestRunner::isEnabled() const { return m_enabled; }

void AutoTestRunner::setMode(AutoRunMode mode) {
  if (m_mode == mode)
    return;
  m_mode = mode;
  emit modeChanged(m_mode);
}

AutoRunMode AutoTestRunner::mode() const { return m_mode; }

void AutoTestRunner::setDebounceDelay(int ms) {
  m_debounceDelay = qBound(100, ms, 60000);
  m_debounceTimer.setInterval(m_debounceDelay);
}

int AutoTestRunner::debounceDelay() const { return m_debounceDelay; }

void AutoTestRunner::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
}

QString AutoTestRunner::workspaceFolder() const { return m_workspaceFolder; }

void AutoTestRunner::setConfiguration(const TestConfiguration &config) {
  m_config = config;
}

TestConfiguration AutoTestRunner::configuration() const { return m_config; }

void AutoTestRunner::notifyFileSaved(const QString &filePath) {
  if (!m_enabled || m_mode == AutoRunMode::Off)
    return;

  if (!m_config.isValid())
    return;

  if (m_runManager && m_runManager->isRunning())
    return;

  scheduleRun(filePath);
}

void AutoTestRunner::setLastFilePath(const QString &filePath) {
  m_lastFilePath = filePath;
}

QString AutoTestRunner::lastFilePath() const { return m_lastFilePath; }

bool AutoTestRunner::isRunning() const {
  return m_runManager && m_runManager->isRunning();
}

void AutoTestRunner::scheduleRun(const QString &filePath) {
  m_pendingFilePath = filePath;
  m_debounceTimer.start();
}

void AutoTestRunner::onDebounceTimeout() {
  if (!m_enabled || m_mode == AutoRunMode::Off)
    return;

  if (!m_config.isValid())
    return;

  if (m_runManager && m_runManager->isRunning())
    return;

  switch (m_mode) {
  case AutoRunMode::AllOnSave:
    m_runManager->runAll(m_config, m_workspaceFolder);
    emit autoRunTriggered();
    break;

  case AutoRunMode::CurrentFileOnSave:
    if (!m_pendingFilePath.isEmpty()) {
      m_lastFilePath = m_pendingFilePath;
      m_runManager->runAll(m_config, m_workspaceFolder, m_pendingFilePath);
      emit autoRunTriggered();
    }
    break;

  case AutoRunMode::LastSelection:
    if (!m_lastFilePath.isEmpty()) {
      m_runManager->runAll(m_config, m_workspaceFolder, m_lastFilePath);
      emit autoRunTriggered();
    } else {
      m_runManager->runAll(m_config, m_workspaceFolder);
      emit autoRunTriggered();
    }
    break;

  case AutoRunMode::Off:
    break;
  }
}

void AutoTestRunner::onRunFinished(int passed, int failed, int skipped,
                                   int errored) {
  Q_UNUSED(passed)
  Q_UNUSED(failed)
  Q_UNUSED(skipped)
  Q_UNUSED(errored)
}

void AutoTestRunner::saveSettings(const QString &workspaceFolder) const {
  if (workspaceFolder.isEmpty())
    return;

  QDir dir(workspaceFolder);
  if (!dir.mkpath(".lightpad"))
    return;

  QString path = dir.filePath(".lightpad/autotest.json");

  QJsonObject obj;
  obj["enabled"] = m_enabled;
  obj["mode"] = static_cast<int>(m_mode);
  obj["debounceDelay"] = m_debounceDelay;

  QFile file(path);
  if (file.open(QIODevice::WriteOnly)) {
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
  }
}

void AutoTestRunner::loadSettings(const QString &workspaceFolder) {
  if (workspaceFolder.isEmpty())
    return;

  QString path =
      QDir(workspaceFolder).filePath(".lightpad/autotest.json");

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return;

  QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();

  bool enabled = obj.value("enabled").toBool(false);
  int modeInt = obj.value("mode").toInt(0);
  int debounce = obj.value("debounceDelay").toInt(DEFAULT_DEBOUNCE_MS);

  if (modeInt >= 0 && modeInt <= static_cast<int>(AutoRunMode::LastSelection))
    m_mode = static_cast<AutoRunMode>(modeInt);

  m_debounceDelay = qBound(100, debounce, 60000);
  m_debounceTimer.setInterval(m_debounceDelay);

  setEnabled(enabled);
}
