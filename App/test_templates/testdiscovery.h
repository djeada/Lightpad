#ifndef TESTDISCOVERY_H
#define TESTDISCOVERY_H

#include "testconfiguration.h"
#include <QList>
#include <QObject>
#include <QProcess>

struct DiscoveredTest {
  QString name;
  QString suite;
  QString id;
  QString filePath;
  int line = -1;
};

class ITestDiscoveryAdapter : public QObject {
  Q_OBJECT

public:
  explicit ITestDiscoveryAdapter(QObject *parent = nullptr)
      : QObject(parent) {}
  virtual ~ITestDiscoveryAdapter() = default;

  virtual QString adapterId() const = 0;
  virtual void discover(const QString &buildDir) = 0;
  virtual void cancel() = 0;

signals:
  void discoveryFinished(const QList<DiscoveredTest> &tests);
  void discoveryError(const QString &message);
};

class CTestDiscoveryAdapter : public ITestDiscoveryAdapter {
  Q_OBJECT

public:
  explicit CTestDiscoveryAdapter(QObject *parent = nullptr);
  ~CTestDiscoveryAdapter() override;

  QString adapterId() const override { return "ctest"; }
  void discover(const QString &buildDir) override;
  void cancel() override;

  static QList<DiscoveredTest>
  parseJsonOutput(const QByteArray &data);
  static QList<DiscoveredTest>
  parseDashNOutput(const QString &output);

private slots:
  void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
  QProcess *m_process = nullptr;
  bool m_usingJson = false;
};

class GTestDiscoveryAdapter : public ITestDiscoveryAdapter {
  Q_OBJECT

public:
  explicit GTestDiscoveryAdapter(QObject *parent = nullptr);
  ~GTestDiscoveryAdapter() override;

  QString adapterId() const override { return "gtest"; }
  void discover(const QString &buildDir) override;
  void cancel() override;

  void setExecutablePath(const QString &path);

  static QList<DiscoveredTest>
  parseListTestsOutput(const QString &output);

  static QString buildGTestFilter(const QStringList &testNames);

private slots:
  void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
  QProcess *m_process = nullptr;
  QString m_executablePath;
};

#endif
