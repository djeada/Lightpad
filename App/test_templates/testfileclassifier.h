#ifndef TESTFILECLASSIFIER_H
#define TESTFILECLASSIFIER_H

#include <QMap>
#include <QObject>
#include <QString>

class TestFileClassifier : public QObject {
  Q_OBJECT

public:
  static TestFileClassifier &instance();

  bool isTestFile(const QString &filePath) const;
  bool isTestDirectory(const QString &dirPath) const;
  bool hasUserOverride(const QString &filePath) const;
  void setTestOverride(const QString &filePath, bool isTest);
  void clearOverride(const QString &filePath);

  void setWorkspaceFolder(const QString &folder);
  QString workspaceFolder() const;

  void loadOverrides();
  void saveOverrides() const;

signals:
  void overrideChanged(const QString &filePath, bool isTest);

private:
  TestFileClassifier();
  ~TestFileClassifier() = default;
  TestFileClassifier(const TestFileClassifier &) = delete;
  TestFileClassifier &operator=(const TestFileClassifier &) = delete;

  bool autoDetectTestFile(const QString &filePath) const;
  bool autoDetectTestDirectory(const QString &dirPath) const;

  QString m_workspaceFolder;
  QMap<QString, bool> m_overrides;
};

#endif
