#include "testfileclassifier.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

TestFileClassifier &TestFileClassifier::instance() {
  static TestFileClassifier s_instance;
  return s_instance;
}

TestFileClassifier::TestFileClassifier() : QObject(nullptr) {}

bool TestFileClassifier::isTestFile(const QString &filePath) const {
  if (m_overrides.contains(filePath)) {
    return m_overrides.value(filePath);
  }
  return autoDetectTestFile(filePath);
}

bool TestFileClassifier::isTestDirectory(const QString &dirPath) const {
  return autoDetectTestDirectory(dirPath);
}

bool TestFileClassifier::hasUserOverride(const QString &filePath) const {
  return m_overrides.contains(filePath);
}

void TestFileClassifier::setTestOverride(const QString &filePath, bool isTest) {
  m_overrides[filePath] = isTest;
  saveOverrides();
  emit overrideChanged(filePath, isTest);
}

void TestFileClassifier::clearOverride(const QString &filePath) {
  m_overrides.remove(filePath);
  saveOverrides();
}

void TestFileClassifier::setWorkspaceFolder(const QString &folder) {
  m_workspaceFolder = folder;
}

QString TestFileClassifier::workspaceFolder() const {
  return m_workspaceFolder;
}

void TestFileClassifier::loadOverrides() {
  if (m_workspaceFolder.isEmpty()) {
    return;
  }

  QString path = QDir(m_workspaceFolder).filePath(".lightpad/testfiles.json");
  QFile file(path);
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return;
  }

  QJsonObject root = doc.object();
  QJsonObject overrides = root.value("overrides").toObject();
  m_overrides.clear();
  for (auto it = overrides.begin(); it != overrides.end(); ++it) {
    m_overrides[it.key()] = it.value().toBool();
  }
}

void TestFileClassifier::saveOverrides() const {
  if (m_workspaceFolder.isEmpty()) {
    return;
  }

  QDir dir(m_workspaceFolder);
    dir.mkdir(".lightpad");
  }

  QString path = dir.filePath(".lightpad/testfiles.json");
  QFile file(path);
    return;
  }

  QJsonObject overrides;
  for (auto it = m_overrides.constBegin(); it != m_overrides.constEnd(); ++it) {
    overrides[it.key()] = it.value();
  }

  QJsonObject root;
  root["overrides"] = overrides;

  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool TestFileClassifier::autoDetectTestFile(const QString &filePath) const {
  QFileInfo fi(filePath);
  QString baseName = fi.completeBaseName().toLower();

  static const QStringList testPrefixes = {"test_", "test-", "tests_",
                                           "tests-"};
  static const QStringList testSuffixes = {"_test",  "-test",  "_tests",
                                           "-tests", "_spec",  "-spec",
                                           ".test",  ".spec"};

  for (const QString &prefix : testPrefixes) {
    if (baseName.startsWith(prefix)) {
      return true;
    }
  }

  for (const QString &ts : testSuffixes) {
    if (baseName.endsWith(ts)) {
      return true;
    }
  }

  QString normalized = QDir::cleanPath(filePath).toLower();
  static const QStringList testDirPatterns = {"/tests/", "/test/", "/__tests__/",
                                              "/__test__/", "/spec/", "/specs/"};
  for (const QString &dir : testDirPatterns) {
    if (normalized.contains(dir)) {
      return true;
    }
  }

  return false;
}

bool TestFileClassifier::autoDetectTestDirectory(const QString &dirPath) const {
  QString dirName = QDir(dirPath).dirName().toLower();
  static const QStringList testDirNames = {"tests",    "test",    "__tests__",
                                           "__test__", "spec",    "specs",
                                           "t",        "testing", "test_suite"};
  return testDirNames.contains(dirName);
}
