

#include "build/cmakeproject.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <array>
#include <utility>

namespace {

void writeFile(const QString &path, const QString &contents) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "cannot write" << path;
    return;
  }
  file.write(contents.toUtf8());
}

QString makeProject(const QTemporaryDir &dir) {
  const QString root = dir.path() + "/proj";
  if (!QDir().mkpath(root) || !QDir().mkpath(root + "/src")) {
    qWarning() << "cannot create project dirs";
    return QString();
  }

  writeFile(root + "/CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.16)\n"
            "project(Demo CXX)\n"
            "# add_executable(commented_out main.cpp) must be ignored\n"
            "add_subdirectory(src)\n"
            "add_executable(\n"
            "    tool\n"
            "    WIN32\n"
            "    tool.cpp\n"
            ")\n");

  writeFile(root + "/src/CMakeLists.txt",
            "add_library(core STATIC core.cpp)\n"
            "add_executable(app main.cpp core.cpp)\n");

  writeFile(root + "/tool.cpp", "int main() { return 0; }\n");
  writeFile(root + "/src/core.cpp", "int core_value() { return 7; }\n");
  writeFile(root + "/src/main.cpp",
            "int core_value();\n"
            "int main() { return core_value() == 7 ? 0 : 1; }\n");

  return root;
}

} // namespace

class TestCMakeProject : public QObject {
  Q_OBJECT

private slots:
  void testDetection();
  void testFindProjectRoot();
  void testParseTargets();
  void testExecutablePathMapping();
  void testBinaryDirResolution();
  void testTargetForSource();
  void testConfigureCommandRequestsDebugInfo();
  void testRealConfigureAndBuild();

private:
  QTemporaryDir m_dir;
};

void TestCMakeProject::testDetection() {
  QVERIFY(!CMakeProject::isCMakeProject(QString()));
  QVERIFY(!CMakeProject::isCMakeProject(m_dir.path()));

  const QString root = m_dir.path() + "/plain";
  QVERIFY(QDir().mkpath(root));
  QVERIFY(!CMakeProject::isCMakeProject(root));

  writeFile(root + "/CMakeLists.txt", "project(X)\n");
  QVERIFY(CMakeProject::isCMakeProject(root));
}

void TestCMakeProject::testFindProjectRoot() {
  const QString root = makeProject(m_dir);
  QCOMPARE(CMakeProject::findProjectRoot(root), root);

  QCOMPARE(CMakeProject::findProjectRoot(root + "/src"), root + "/src");
  QVERIFY(CMakeProject::findProjectRoot(m_dir.path()).isEmpty());
}

void TestCMakeProject::testParseTargets() {
  const QString root = makeProject(m_dir);

  CMakeProject project;
  const QList<CMakeTargetInfo> all = project.parseTargets(root);

  bool sawTool = false;
  for (const CMakeTargetInfo &target : all) {
    if (target.name == "tool") {
      sawTool = true;
      QVERIFY(target.isExecutable);

      QCOMPARE(target.sources, QStringList{"tool.cpp"});
    }
  }
  QVERIFY(sawTool);

  const QList<CMakeTargetInfo> executables =
      project.parseExecutableTargets(root);
  QStringList names;
  for (const CMakeTargetInfo &target : executables) {
    names << target.name;
  }
  names.sort();
  const QStringList expectedNames = {"app", "tool"};
  QCOMPARE(names, expectedNames);
}

void TestCMakeProject::testExecutablePathMapping() {
  CMakeTargetInfo target;
  target.name = "app";
  target.isExecutable = true;
  const QString buildDir = QStringLiteral("/x/build");

#ifdef Q_OS_WIN
  QCOMPARE(CMakeProject::executablePathFor(target, buildDir),
           QString("/x/build/app.exe"));
#else
  QCOMPARE(CMakeProject::executablePathFor(target, buildDir),
           QString("/x/build/app"));
#endif
  QVERIFY(CMakeProject::executablePathFor(target, QString()).isEmpty());

  CMakeTargetInfo unnamed;
  QVERIFY(CMakeProject::executablePathFor(unnamed, "/x/build").isEmpty());
}

void TestCMakeProject::testTargetForSource() {
  const QString root = makeProject(m_dir);

  CMakeProject project;
  const QList<CMakeTargetInfo> executables =
      project.parseExecutableTargets(root);

  QCOMPARE(
      CMakeProject::targetForSource(executables, root, root + "/src/main.cpp"),
      QString("app"));
  QCOMPARE(CMakeProject::targetForSource(executables, root, root + "/tool.cpp"),
           QString("tool"));

  QVERIFY(
      CMakeProject::targetForSource(executables, root, root + "/src/unused.cpp")
          .isEmpty());
  QVERIFY(
      CMakeProject::targetForSource(executables, root, QString()).isEmpty());
  QVERIFY(
      CMakeProject::targetForSource({}, root, root + "/tool.cpp").isEmpty());
}

void TestCMakeProject::testConfigureCommandRequestsDebugInfo() {
  const QStringList command =
      CMakeProject::configureCommand("/proj", "/proj/build");

  QCOMPARE(command.first(), QString("cmake"));
  QVERIFY(command.contains("/proj"));
  QVERIFY(command.contains("/proj/build"));

  QVERIFY(command.contains("-DCMAKE_BUILD_TYPE=Debug"));
}

void TestCMakeProject::testBinaryDirResolution() {
  QCOMPARE(CMakeProject::resolveBinaryDirectory("/proj", QString()),
           QString("/proj/build"));
  QCOMPARE(CMakeProject::resolveBinaryDirectory("/proj", "out/custom"),
           QString("/proj/out/custom"));
  QCOMPARE(CMakeProject::resolveBinaryDirectory("/proj", "/abs/build"),
           QString("/abs/build"));

  QCOMPARE(CMakeProject::defaultBinaryDir("/proj"), QString("/proj/build"));

  QVERIFY(CMakeProject::needsConfigure(QStringLiteral("/nonexistent-build")));
}

void TestCMakeProject::testRealConfigureAndBuild() {
  const QString cmake = QStandardPaths::findExecutable("cmake");
  if (cmake.isEmpty()) {
    QSKIP("cmake not installed");
  }

  const QString root = makeProject(m_dir);
  const QString binaryDir = CMakeProject::defaultBinaryDir(root);

  CMakeProject project;

  QVERIFY(CMakeProject::needsConfigure(binaryDir));

  QString error;
  QString output;
  const bool configured = project.configure(root, binaryDir, &output, &error);
  if (!configured) {
    qWarning() << "configure error:" << error;
  }
  QVERIFY(configured);
  QVERIFY(!CMakeProject::needsConfigure(binaryDir));

  QString secondOutput;
  QVERIFY(project.configure(root, binaryDir, &secondOutput, &error));

  QVERIFY(project.build(binaryDir, 2, &output, &error));
  QVERIFY(error.isEmpty());

  for (const auto &expected :
       std::array{std::pair<QString, QString>{"app", "src"},
                  std::pair<QString, QString>{"tool", ""}}) {
    CMakeTargetInfo info;
    info.name = expected.first;
    info.isExecutable = true;
    info.relativeDir = expected.second;
    const QString exePath = CMakeProject::executablePathFor(info, binaryDir);
    QVERIFY2(QFileInfo::exists(exePath),
             qPrintable(QStringLiteral("missing %1").arg(exePath)));
    QVERIFY(QFileInfo(exePath).isExecutable());
  }

  QVERIFY(project.build(binaryDir, 2, &output, &error));

  writeFile(root + "/src/core.cpp", "int broken() { return ; }\n");
  QVERIFY(!project.build(binaryDir, 2, &output, &error));
  QVERIFY(!error.trimmed().isEmpty());
}

QTEST_MAIN(TestCMakeProject)
#include "test_cmakeproject.moc"
