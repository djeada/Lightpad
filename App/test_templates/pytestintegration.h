#ifndef PYTESTINTEGRATION_H
#define PYTESTINTEGRATION_H

#include "testconfiguration.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QTemporaryDir>

namespace PytestIntegration {
inline QString prefix() { return QStringLiteral("@@LIGHTPAD_PYTEST@@"); }
inline QString selectionValue(const QStringList &targets) {
  return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(targets))
                               .toJson(QJsonDocument::Compact));
}
inline TestConfiguration withTargets(TestConfiguration configuration,
                                     const QStringList &targets) {
  configuration.args << targets;
  configuration.env["LIGHTPAD_PYTEST_SELECTION"] = selectionValue(targets);
  return configuration;
}
inline bool isPytestExecutable(const QString &program) {
  QString name = QFileInfo(program).fileName();
  if (name.endsWith(".exe", Qt::CaseInsensitive))
    name.chop(4);
  return name == "pytest" || name == "py.test";
}
inline bool supports(const QString &program, const QStringList &arguments) {
  const int module = arguments.indexOf("pytest");
  return (module >= 1 && arguments.at(module - 1) == "-m") ||
         isPytestExecutable(program);
}

inline QJsonObject record(const QString &line) {
  const int start = line.indexOf(prefix());
  if (start < 0)
    return {};
  return QJsonDocument::fromJson(line.mid(start + prefix().size()).toUtf8())
      .object();
}

inline bool install(QTemporaryDir &directory, QProcessEnvironment &environment,
                    QStringList &arguments, const QString &program = {}) {
  int insertion = arguments.indexOf("pytest");
  if (insertion >= 1 && arguments.at(insertion - 1) == "-m") {
    ++insertion;
  } else if (isPytestExecutable(program)) {
    insertion = 0;
  } else {
    return false;
  }
  QFile source(":/test_templates/lightpad_pytest.py");
  if (!directory.isValid() || !source.open(QIODevice::ReadOnly))
    return false;
  QSaveFile target(directory.filePath("_lightpad_pytest_reporter.py"));
  const auto contents = source.readAll();
  if (!target.open(QIODevice::WriteOnly) ||
      target.write(contents) != contents.size())
    return false;
  if (!target.commit())
    return false;
#ifdef Q_OS_WIN
  const QString separator = ";";
#else
  const QString separator = ":";
#endif
  const QString inherited = environment.value("PYTHONPATH");
  environment.insert("PYTHONPATH",
                     directory.path() + (inherited.isEmpty()
                                             ? QString()
                                             : separator + inherited));
  arguments.insert(insertion, "-p");
  arguments.insert(insertion + 1, "_lightpad_pytest_reporter");
  return true;
}
} // namespace PytestIntegration
#endif
