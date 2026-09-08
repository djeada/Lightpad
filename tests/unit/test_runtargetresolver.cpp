#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "run_templates/runtargetresolver.h"
#include "run_templates/runtemplatemanager.h"

class TestRunTargetResolver : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();

  void testEmptyPathExplainsItself();
  void testScriptResolvesToTheFileItself();
  void testUnknownExtensionExplainsItself();
  void testCMakeSourceResolvesToOwningExecutable();
  void testCMakeAmbiguityAsksInsteadOfGuessing();
  void testPreferredTargetResolvesAmbiguity();
  void testOwnershipBeatsPreferredTarget();
  void testPythonInCMakeProjectStaysPython();
  void testUnlistedSourceCompilesOnItsOwn();
  void testHeaderWithNoTargetOffersThePicker();
  void testStandaloneSubProjectResolvesItsOwnTarget();
  void testCMakeListsWithOneExecutableNeedsNoPicker();
  void testGlobbedSingleTargetStillResolves();
  void testDeliberateTemplateIsNotOverriddenByCMake();
  void testCTestTemplateRoutesToTestRun();
  void testResolutionIsIndependentOfEditorState();

private:
  QString makeCMakeProject();
  QString writeFile(const QString &relativePath, const QString &contents);

  QTemporaryDir m_tempDir;
};

void TestRunTargetResolver::initTestCase() {
  QStandardPaths::setTestModeEnabled(true);
  QVERIFY(m_tempDir.isValid());
  RunTemplateManager::instance().loadTemplates();
}

void TestRunTargetResolver::init() {
  RunTemplateManager::instance().setWorkspaceFolder(m_tempDir.path());
}

QString TestRunTargetResolver::writeFile(const QString &relativePath,
                                         const QString &contents) {
  const QString absolutePath = m_tempDir.path() + "/" + relativePath;
  QDir().mkpath(QFileInfo(absolutePath).absolutePath());
  QFile file(absolutePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return QString();
  }
  file.write(contents.toUtf8());
  file.close();
  return absolutePath;
}

QString TestRunTargetResolver::makeCMakeProject() {
  const QString root = m_tempDir.path() + "/cmakeproj";
  QDir().mkpath(root);
  QFile lists(root + "/CMakeLists.txt");
  if (!lists.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return QString();
  }
  lists.write("cmake_minimum_required(VERSION 3.16)\n"
              "project(demo)\n"
              "add_executable(demo_app src/main.cpp src/helper.cpp)\n"
              "add_executable(tool_app src/tool.cpp)\n");
  lists.close();

  QDir().mkpath(root + "/src");
  for (const char *name : {"main.cpp", "helper.cpp", "tool.cpp", "loose.cpp"}) {
    QFile source(root + "/src/" + QString::fromLatin1(name));
    if (source.open(QIODevice::WriteOnly | QIODevice::Text)) {
      source.write("int main() { return 0; }\n");
      source.close();
    }
  }
  return root;
}

void TestRunTargetResolver::testEmptyPathExplainsItself() {
  RunTargetContext context;
  const RunTarget target = RunTargetResolver::resolve(context);

  QVERIFY(!target.isValid());
  QCOMPARE(target.kind, RunTarget::Kind::None);
  QVERIFY(!target.unavailableReason.isEmpty());
  QVERIFY(target.commandLine().isEmpty());
}

void TestRunTargetResolver::testScriptResolvesToTheFileItself() {
  const QString scriptPath = writeFile("scripts/hello.py", "print('hi')\n");
  QVERIFY(!scriptPath.isEmpty());

  RunTargetContext context;
  context.filePath = scriptPath;
  context.languageId = "python";
  context.projectRoot = m_tempDir.path();

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::Template);
  QCOMPARE(target.targetPath(), scriptPath);
  QCOMPARE(target.displayName(), QString("hello.py"));
  QVERIFY(!target.requiresBuild);
  QVERIFY(target.commandLine().contains("hello.py"));
  QVERIFY(!target.rationale().isEmpty());
}

void TestRunTargetResolver::testUnknownExtensionExplainsItself() {
  const QString path = writeFile("notes/readme.zzz", "nothing runnable\n");
  QVERIFY(!path.isEmpty());

  RunTargetContext context;
  context.filePath = path;
  context.projectRoot = m_tempDir.path();

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::None);
  QVERIFY(target.unavailableReason.contains("zzz"));
}

void TestRunTargetResolver::testCMakeSourceResolvesToOwningExecutable() {
  const QString root = makeCMakeProject();
  QVERIFY(!root.isEmpty());

  RunTargetContext context;
  context.filePath = root + "/src/helper.cpp";
  context.languageId = "cpp";
  context.projectRoot = root;

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::CMakeExecutable);
  QCOMPARE(target.cmakeTargetName, QString("demo_app"));
  QVERIFY(target.requiresBuild);
  QVERIFY(!target.needsTargetChoice);

  QCOMPARE(target.targetPath(), root + "/build/demo_app");
  QCOMPARE(target.workingDirectory, root + "/build");
  QVERIFY(target.rationale().contains("demo_app"));
}

void TestRunTargetResolver::testCMakeAmbiguityAsksInsteadOfGuessing() {
  const QString root = makeCMakeProject();
  QVERIFY(!root.isEmpty());

  RunTargetContext context;

  context.filePath = root + "/CMakeLists.txt";
  context.languageId = "cmake";
  context.projectRoot = root;

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::CMakeExecutable);
  QVERIFY(target.needsTargetChoice);
  QVERIFY(target.requiresBuild);
  QVERIFY(target.cmakeTargetName.isEmpty());
  QVERIFY(!target.rationale().isEmpty());
}

void TestRunTargetResolver::testPreferredTargetResolvesAmbiguity() {
  const QString root = makeCMakeProject();
  QVERIFY(!root.isEmpty());

  RunTargetContext context;

  context.filePath = root + "/src/loose.cpp";
  context.languageId = "cpp";
  context.projectRoot = root;
  context.preferredCMakeTarget = "tool_app";

  const RunTarget target = RunTargetResolver::resolve(context);

  QVERIFY(!target.needsTargetChoice);
  QCOMPARE(target.cmakeTargetName, QString("tool_app"));
  QVERIFY(!target.ownedByCMakeTarget);
  QCOMPARE(target.targetPath(), root + "/build/tool_app");
}

void TestRunTargetResolver::testOwnershipBeatsPreferredTarget() {
  const QString root = makeCMakeProject();
  QVERIFY(!root.isEmpty());

  RunTargetContext context;
  context.filePath = root + "/src/helper.cpp";
  context.languageId = "cpp";
  context.projectRoot = root;
  context.preferredCMakeTarget = "tool_app";

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.cmakeTargetName, QString("demo_app"));
  QVERIFY(target.ownedByCMakeTarget);
  QCOMPARE(target.targetPath(), root + "/build/demo_app");
}

void TestRunTargetResolver::testPythonInCMakeProjectStaysPython() {
  const QString root = makeCMakeProject();
  QVERIFY(!root.isEmpty());

  QFile script(root + "/tool.py");
  QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Text));
  script.write("print('hi')\n");
  script.close();

  RunTargetContext context;
  context.filePath = root + "/tool.py";
  context.languageId = "python";
  context.projectRoot = root;
  context.preferredCMakeTarget = "tool_app";

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::Template);
  QCOMPARE(target.targetPath(), root + "/tool.py");
  QVERIFY(!target.requiresBuild);
  QVERIFY(target.commandLine().contains("tool.py"));
}

void TestRunTargetResolver::testUnlistedSourceCompilesOnItsOwn() {
  const QString root = makeCMakeProject();
  QVERIFY(!root.isEmpty());

  RunTargetContext context;
  context.filePath = root + "/src/loose.cpp";
  context.languageId = "cpp";
  context.projectRoot = root;

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::Template);
  QCOMPARE(target.targetPath(), root + "/src/loose.cpp");
  QVERIFY(target.commandLine().contains("loose"));
}

void TestRunTargetResolver::testHeaderWithNoTargetOffersThePicker() {
  const QString root = makeCMakeProject();
  QVERIFY(!root.isEmpty());

  QFile header(root + "/src/util.hpp");
  QVERIFY(header.open(QIODevice::WriteOnly | QIODevice::Text));
  header.write("#pragma once\n");
  header.close();

  RunTargetContext context;
  context.filePath = root + "/src/util.hpp";
  context.languageId = "cpp";
  context.projectRoot = root;

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::CMakeExecutable);
  QVERIFY(target.needsTargetChoice);
  QVERIFY(target.requiresBuild);
}

void TestRunTargetResolver::testStandaloneSubProjectResolvesItsOwnTarget() {
  const QString root = m_tempDir.path() + "/nested";
  QDir().mkpath(root + "/src/example");

  QFile outer(root + "/CMakeLists.txt");
  QVERIFY(outer.open(QIODevice::WriteOnly | QIODevice::Text));
  outer.write("cmake_minimum_required(VERSION 3.16)\n"
              "project(outer)\n"
              "add_executable(outer_app src/main.cpp)\n");
  outer.close();

  QFile inner(root + "/src/example/CMakeLists.txt");
  QVERIFY(inner.open(QIODevice::WriteOnly | QIODevice::Text));
  inner.write("cmake_minimum_required(VERSION 3.16)\n"
              "project(example)\n"
              "add_executable(vao_example main.cpp)\n");
  inner.close();

  for (const QString &relative : {QStringLiteral("src/main.cpp"),
                                  QStringLiteral("src/example/main.cpp")}) {
    QFile source(root + "/" + relative);
    QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Text));
    source.write("int main() { return 0; }\n");
    source.close();
  }

  RunTargetContext listsContext;
  listsContext.filePath = root + "/src/example/CMakeLists.txt";
  listsContext.languageId = "cmake";
  listsContext.projectRoot = root;

  const RunTarget listsTarget = RunTargetResolver::resolve(listsContext);
  QCOMPARE(listsTarget.kind, RunTarget::Kind::CMakeExecutable);
  QVERIFY(!listsTarget.needsTargetChoice);
  QCOMPARE(listsTarget.cmakeTargetName, QString("vao_example"));
  QCOMPARE(listsTarget.cmakeRoot, root + "/src/example");

  RunTargetContext sourceContext;
  sourceContext.filePath = root + "/src/example/main.cpp";
  sourceContext.languageId = "cpp";
  sourceContext.projectRoot = root;

  const RunTarget sourceTarget = RunTargetResolver::resolve(sourceContext);
  QCOMPARE(sourceTarget.cmakeTargetName, QString("vao_example"));
  QVERIFY(sourceTarget.ownedByCMakeTarget);

  RunTargetContext outerContext;
  outerContext.filePath = root + "/src/main.cpp";
  outerContext.languageId = "cpp";
  outerContext.projectRoot = root;

  const RunTarget outerTarget = RunTargetResolver::resolve(outerContext);
  QCOMPARE(outerTarget.cmakeTargetName, QString("outer_app"));
}

void TestRunTargetResolver::testCMakeListsWithOneExecutableNeedsNoPicker() {
  const QString root = m_tempDir.path() + "/single-exe";
  QDir().mkpath(root);
  QFile lists(root + "/CMakeLists.txt");
  QVERIFY(lists.open(QIODevice::WriteOnly | QIODevice::Text));
  lists.write("cmake_minimum_required(VERSION 3.16)\n"
              "project(single)\n"
              "add_executable(only_app main.cpp)\n");
  lists.close();

  RunTargetContext context;
  context.filePath = root + "/CMakeLists.txt";
  context.languageId = "cmake";
  context.projectRoot = root;

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::CMakeExecutable);
  QVERIFY(!target.needsTargetChoice);
  QCOMPARE(target.cmakeTargetName, QString("only_app"));
}

void TestRunTargetResolver::testGlobbedSingleTargetStillResolves() {
  const QString root = m_tempDir.path() + "/globbed";
  QDir().mkpath(root + "/src");
  QFile lists(root + "/CMakeLists.txt");
  QVERIFY(lists.open(QIODevice::WriteOnly | QIODevice::Text));
  lists.write("cmake_minimum_required(VERSION 3.16)\n"
              "project(globbed)\n"
              "add_executable(globbed_app ${SOURCES})\n");
  lists.close();

  QFile source(root + "/src/anything.cpp");
  QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Text));
  source.write("int main() { return 0; }\n");
  source.close();

  RunTargetContext context;
  context.filePath = root + "/src/anything.cpp";
  context.languageId = "cpp";
  context.projectRoot = root;

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::CMakeExecutable);
  QCOMPARE(target.cmakeTargetName, QString("globbed_app"));
  QVERIFY(!target.ownedByCMakeTarget);
}

void TestRunTargetResolver::testDeliberateTemplateIsNotOverriddenByCMake() {
  const QString root = makeCMakeProject();
  QVERIFY(!root.isEmpty());
  const QString sourcePath = root + "/src/main.cpp";

  FileTemplateAssignment assignment;
  assignment.filePath = sourcePath;
  assignment.templateId = "make_run";
  QVERIFY(RunTemplateManager::instance().assignTemplateToFile(sourcePath,
                                                              assignment));

  RunTargetContext context;
  context.filePath = sourcePath;
  context.languageId = "cpp";
  context.projectRoot = root;

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::Template);
  QCOMPARE(target.templateId, QString("make_run"));

  QVERIFY(RunTemplateManager::instance().removeAssignment(sourcePath));
}

void TestRunTargetResolver::testCTestTemplateRoutesToTestRun() {
  const QString root = makeCMakeProject();
  QVERIFY(!root.isEmpty());
  const QString sourcePath = root + "/src/tool.cpp";

  FileTemplateAssignment assignment;
  assignment.filePath = sourcePath;
  assignment.templateId = "cpp_cmake_ctest";
  QVERIFY(RunTemplateManager::instance().assignTemplateToFile(sourcePath,
                                                              assignment));

  RunTargetContext context;
  context.filePath = sourcePath;
  context.languageId = "cpp";
  context.projectRoot = root;

  const RunTarget target = RunTargetResolver::resolve(context);

  QCOMPARE(target.kind, RunTarget::Kind::CTest);
  QCOMPARE(target.templateId, QString("cpp_cmake_ctest"));

  QVERIFY(RunTemplateManager::instance().removeAssignment(sourcePath));
}

void TestRunTargetResolver::testResolutionIsIndependentOfEditorState() {
  const QString first = writeFile("a/one.py", "print(1)\n");
  const QString second = writeFile("b/two.py", "print(2)\n");
  QVERIFY(!first.isEmpty());
  QVERIFY(!second.isEmpty());

  RunTargetContext context;
  context.projectRoot = m_tempDir.path();
  context.languageId = "python";

  context.filePath = first;
  const RunTarget firstTarget = RunTargetResolver::resolve(context);
  context.filePath = second;
  const RunTarget secondTarget = RunTargetResolver::resolve(context);

  QCOMPARE(firstTarget.targetPath(), first);
  QCOMPARE(secondTarget.targetPath(), second);
  QVERIFY(firstTarget.commandLine() != secondTarget.commandLine());
}

QTEST_MAIN(TestRunTargetResolver)
#include "test_runtargetresolver.moc"
