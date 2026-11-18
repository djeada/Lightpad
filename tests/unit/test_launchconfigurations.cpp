#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QtTest>

#include "core/pathvariables.h"
#include "dap/adapters/genericdebugadapter.h"
#include "dap/debugadapterregistry.h"
#include "dap/debugconfiguration.h"
#include "dap/debuginlinescope.h"
#include "dap/debugoutputfilter.h"
#include "dap/watchmanager.h"
#include "run_templates/launchplanner.h"
#include "run_templates/launchtemplatecatalog.h"
#include "run_templates/runtargetresolver.h"
#include "run_templates/runtemplatemanager.h"
#include "ui/dialogs/breakpointeditordialog.h"
#include "ui/dialogs/launchconfigurationsdialog.h"
#include "ui/dialogs/launchtemplategallery.h"
#include "ui/widgets/keyvaluetable.h"

class TestLaunchConfigurations : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  void testPathVariablesAcceptBothSpellings();
  void testPathVariablesExpandEnvironment();
  void testLaunchTargetKeysRoundTrip();

  void testCatalogTemplatesAreUsable();
  void testCatalogSuggestsByLanguageAndProject();
  void testUserTemplatesExtendTheCatalog();
  void testCatalogIgnoresBrokenEntries();

  void testRunCommandsPerDebugger();
  void testDebugPlanUsesTheRunSettings();
  void testDebugPlanCompilesWithTheTemplateCompiler();
  void testDebugPlanBuildsADebuggableProgram();
  void testDebugPlanForModulesAndUndebuggableFiles();
  void testCMakeExecutablesKeepFileArguments();

  void testLoadingDoesNotCreateLaunchJson();
  void testLegacySamplesAreHidden();
  void testConfigurationsKeepFileOrder();

  void testDialogEditsStayLocalUntilSave();
  void testDialogSavesSelectedArguments();
  void testDialogDebouncesDebuggerProbe();
  void testDialogFileSettingsRoundTrip();
  void testDialogViewingChangesNothing();
  void testDialogTemplatesAndCompounds();
  void testDialogRunButtonRequestsLaunch();
  void testGalleryFiltersTemplates();

  void testJsonAdaptersAreRegistered();
  void testGenericAdapterBuildsRequests();
  void testGenericAdapterRunCommand();
  void testAdapterDefinitionsParse();
  void testNewDebuggerTemplates();
  void testDebuggerChatterIsFiltered();
  void testInlineValuesStayInTheirFunction();
  void testWatchesAreNotDuplicated();
  void testBreakpointEditorEditsEverythingAtOnce();

private:
  QString writeFile(const QString &relativePath, const QByteArray &content);

  QTemporaryDir m_config;
  std::unique_ptr<QTemporaryDir> m_workspace;
};

void TestLaunchConfigurations::initTestCase() {
  QVERIFY(m_config.isValid());
  // User launch templates live in the config directory.
  qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
  RunTemplateManager::instance().loadTemplates();
}

void TestLaunchConfigurations::init() {
  m_workspace = std::make_unique<QTemporaryDir>();
  QVERIFY(m_workspace->isValid());
  QVERIFY(QDir(m_workspace->path()).mkpath(".git"));
  RunTemplateManager::instance().setWorkspaceFolder(m_workspace->path());
  DebugConfigurationManager::instance().setWorkspaceFolder(m_workspace->path());
  DebugConfigurationManager::instance().replaceConfigurations({});
  DebugConfigurationManager::instance().replaceCompoundConfigurations({});
}

void TestLaunchConfigurations::cleanup() {
  RunTemplateManager::instance().setWorkspaceFolder(QString());
  DebugConfigurationManager::instance().replaceConfigurations({});
  DebugConfigurationManager::instance().replaceCompoundConfigurations({});
  DebugConfigurationManager::instance().setWorkspaceFolder(QString());
  m_workspace.reset();
}

QString TestLaunchConfigurations::writeFile(const QString &relativePath,
                                            const QByteArray &content) {
  const QString path = QDir(m_workspace->path()).filePath(relativePath);
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return {};
  }
  file.write(content);
  return path;
}

// ---------------------------------------------------------------- basics --

void TestLaunchConfigurations::testPathVariablesAcceptBothSpellings() {
  const QMap<QString, QString> vars = PathVariables::forContext(
      "/work/project", "/work/project/src/app.tar.gz");
  QCOMPARE(vars.value("fileDir"), QString("/work/project/src"));
  QCOMPARE(vars.value("fileDirname"), vars.value("fileDir"));
  QCOMPARE(vars.value("fileBasenameNoExt"), QString("app.tar"));
  QCOMPARE(vars.value("fileBasenameNoExtension"), QString("app.tar"));
  QCOMPARE(vars.value("fileExt"), QString("gz"));
  QCOMPARE(vars.value("fileExtname"), QString(".gz"));
  QCOMPARE(vars.value("relativeFile"), QString("src/app.tar.gz"));
  QCOMPARE(vars.value("relativeFileDirname"), QString("src"));
  QCOMPARE(vars.value("workspaceFolderBasename"), QString("project"));

  QCOMPARE(PathVariables::expand("${fileDirname}/${unknown}", vars),
           QString("/work/project/src/${unknown}"));
  QVERIFY(PathVariables::needsFile("${fileBasenameNoExtension}.o"));
  QVERIFY(!PathVariables::needsFile("${workspaceFolder}/build/app"));

  // Run templates and debug configurations share the expansion.
  QCOMPARE(PythonProjectEnvironment::substituteVariables(
               "${fileDirname}|${relativeFile}", "/work/project",
               "/work/project/src/app.py"),
           QString("/work/project/src|src/app.py"));
}

void TestLaunchConfigurations::testPathVariablesExpandEnvironment() {
  qputenv("LIGHTPAD_LAUNCH_TEST", "value with spaces");
  QCOMPARE(PathVariables::expand("a=${env:LIGHTPAD_LAUNCH_TEST};b=${env:"
                                 "LIGHTPAD_LAUNCH_UNSET_VARIABLE}",
                                 {}),
           QString("a=value with spaces;b="));
  qunsetenv("LIGHTPAD_LAUNCH_TEST");
}

void TestLaunchConfigurations::testLaunchTargetKeysRoundTrip() {
  for (const LaunchTarget &target :
       {LaunchTarget::currentFile(), LaunchTarget::configuration("API"),
        LaunchTarget::compound("Server + Client")}) {
    QCOMPARE(LaunchTarget::fromKey(target.toKey()), target);
  }
  // Keys written before Run and Debug shared a target stay readable.
  QCOMPARE(LaunchTarget::fromKey("C++: GDB Launch"),
           LaunchTarget::configuration("C++: GDB Launch"));
  QCOMPARE(LaunchTarget::fromKey("compound:Both"),
           LaunchTarget::compound("Both"));
  QVERIFY(LaunchTarget::fromKey("  ").isCurrentFile());
}

// --------------------------------------------------------------- catalog --

void TestLaunchConfigurations::testCatalogTemplatesAreUsable() {
  LaunchTemplateCatalog &catalog = LaunchTemplateCatalog::instance();
  catalog.reload();
  const QList<LaunchTemplate> templates = catalog.templates();
  QVERIFY(templates.size() >= 25);
  QVERIFY(catalog.categories().contains("Python"));
  QVERIFY(catalog.categories().contains("C & C++"));

  QSet<QString> ids;
  for (const LaunchTemplate &entry : templates) {
    QVERIFY2(!ids.contains(entry.id), qPrintable(entry.id));
    ids.insert(entry.id);
    QVERIFY2(!entry.name.isEmpty() && !entry.description.isEmpty(),
             qPrintable(entry.id));
    const DebugConfiguration config = entry.instantiate(entry.name);
    QCOMPARE(config.name, entry.name);
    QVERIFY2(!config.type.isEmpty(), qPrintable(entry.id));
    if (entry.isDebuggable()) {
      // Every debuggable template has a debugger Lightpad knows.
      QVERIFY2(!LaunchPlanner::debuggerNameFor(config).isEmpty(),
               qPrintable(entry.id));
    }
    if (config.request == "launch") {
      // ... and every launch template can also run.
      const RunCommand command = LaunchPlanner::runCommandFor(
          DebugConfigurationManager::instance().resolveVariables(
              config, m_workspace->path() + "/main.py"));
      QVERIFY2(command.error.isEmpty() || entry.id == "tools.command",
               qPrintable(entry.id + ": " + command.error));
    }
  }
}

void TestLaunchConfigurations::testCatalogSuggestsByLanguageAndProject() {
  LaunchTemplateCatalog &catalog = LaunchTemplateCatalog::instance();
  QList<LaunchTemplate> python = catalog.suggestionsFor("python", {});
  QVERIFY(!python.isEmpty());
  QCOMPARE(python.first().id, QString("python.file"));

  // A project marker outranks a language match.
  writeFile("package.json", "{}");
  const QList<LaunchTemplate> js =
      catalog.suggestionsFor("js", m_workspace->path());
  QVERIFY(!js.isEmpty());
  QCOMPARE(js.first().id, QString("node.npm"));
  writeFile("manage.py", "");
  QCOMPARE(catalog.suggestionsFor({}, m_workspace->path()).first().id,
           QString("python.django"));
}

void TestLaunchConfigurations::testUserTemplatesExtendTheCatalog() {
  const QString path = LaunchTemplateCatalog::userTemplatesPath();
  QVERIFY(path.startsWith(m_config.path()));
  QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(R"({"categories": ["Team"], "templates": [
    {"id": "team.server", "category": "Team", "name": "Team Server",
     "description": "Our dev server",
     "configuration": {"type": "command", "program": "make", "args": ["serve"]}},
    {"id": "python.file", "category": "Python", "name": "Python: Mine",
     "description": "Replaced", "configuration": {"type": "debugpy",
     "program": "${file}"}}]})");
  file.close();
  const auto restore = qScopeGuard([&]() {
    QFile::remove(path);
    LaunchTemplateCatalog::instance().reload();
  });

  LaunchTemplateCatalog &catalog = LaunchTemplateCatalog::instance();
  catalog.reload();
  const LaunchTemplate team = catalog.templateById("team.server");
  QVERIFY(team.isValid());
  QVERIFY(!team.isDebuggable());
  QCOMPARE(team.configuration.value("request").toString(), QString("launch"));
  QVERIFY(catalog.categories().contains("Team"));
  QCOMPARE(catalog.templateById("python.file").name, QString("Python: Mine"));
}

void TestLaunchConfigurations::testCatalogIgnoresBrokenEntries() {
  QStringList categories;
  const QList<LaunchTemplate> parsed = LaunchTemplateCatalog::parse(
      R"({"templates": [{"id": "no-config"}, {"configuration": {"type": "x"}},
          {"id": "ok", "configuration": {"type": "command", "program": "x",
           "name": "dropped"}}]})",
      &categories);
  QCOMPARE(parsed.size(), 1);
  QCOMPARE(parsed.first().id, QString("ok"));
  QCOMPARE(parsed.first().category, QString("Other"));
  QVERIFY(!parsed.first().configuration.contains("name"));
  QVERIFY(LaunchTemplateCatalog::parse("not json").isEmpty());
}

// -------------------------------------------------------------- planning --

void TestLaunchConfigurations::testRunCommandsPerDebugger() {
  DebugConfiguration python;
  python.type = "debugpy";
  python.request = "launch";
  python.program = "/p/app.py";
  python.args = {"--port", "8000"};
  python.adapterConfig["python"] = "/opt/py/bin/python3";
  RunCommand command = LaunchPlanner::runCommandFor(python);
  QVERIFY2(command.isValid(), qPrintable(command.error));
  QCOMPARE(command.program, QString("/opt/py/bin/python3"));
  QCOMPARE(command.arguments, QStringList({"/p/app.py", "--port", "8000"}));
  QCOMPARE(command.environment.value("PYTHONUNBUFFERED"), QString("1"));

  python.adapterConfig["module"] = "uvicorn";
  command = LaunchPlanner::runCommandFor(python);
  QCOMPARE(command.arguments, QStringList({"-m", "uvicorn", "--port", "8000"}));

  DebugConfiguration npm;
  npm.type = "node";
  npm.request = "launch";
  npm.adapterConfig["runtimeExecutable"] = "npm";
  npm.adapterConfig["runtimeArgs"] = QJsonArray{"run-script", "start"};
  command = LaunchPlanner::runCommandFor(npm);
  QCOMPARE(command.program, QString("npm"));
  QCOMPARE(command.arguments, QStringList({"run-script", "start"}));

  DebugConfiguration go;
  go.type = "go";
  go.request = "launch";
  go.program = "/p/cmd/server";
  go.args = {"-v"};
  command = LaunchPlanner::runCommandFor(go);
  QCOMPARE(command.program, QString("go"));
  QCOMPARE(command.arguments, QStringList({"run", "/p/cmd/server", "-v"}));
  go.adapterConfig["mode"] = "test";
  command = LaunchPlanner::runCommandFor(go);
  QCOMPARE(command.arguments,
           QStringList({"test", "/p/cmd/server", "-args", "-v"}));

  DebugConfiguration native;
  native.type = "cppdbg";
  native.request = "launch";
  native.program = "/p/build/app";
  native.args = {"x"};
  command = LaunchPlanner::runCommandFor(native);
  QCOMPARE(command.program, native.program);
  QCOMPARE(command.arguments, QStringList({"x"}));

  DebugConfiguration tool;
  tool.type = "command";
  tool.request = "launch";
  tool.program = "docker";
  tool.args = {"compose", "up"};
  QCOMPARE(LaunchPlanner::runCommandFor(tool).program, QString("docker"));
  QVERIFY(LaunchPlanner::debuggerNameFor(tool).isEmpty());

  native.request = "attach";
  QVERIFY(!LaunchPlanner::runCommandFor(native).isValid());
  QVERIFY(LaunchPlanner::runCommandFor(native).error.contains("Debug"));
}

void TestLaunchConfigurations::testDebugPlanUsesTheRunSettings() {
  const QString script = writeFile("tools/report.py", "print('hi')\n");
  FileTemplateAssignment assignment;
  assignment.customArgs = {"--day", "${fileBasenameNoExt}"};
  assignment.customEnv.insert("REPORT_MODE", "full");
  assignment.workingDirectory = "${workspaceFolder}/tools";
  QVERIFY(
      RunTemplateManager::instance().assignTemplateToFile(script, assignment));

  const RunTarget target =
      RunTargetResolver::resolve({script, "py", m_workspace->path(), {}, {}});
  QCOMPARE(target.kind, RunTarget::Kind::Template);
  QVERIFY(target.arguments.contains("--day"));

  const FileDebugPlan plan = LaunchPlanner::debugPlanForFile(target);
  QVERIFY2(plan.isValid(), qPrintable(plan.error));
  QCOMPARE(plan.configuration.type, QString("debugpy"));
  QCOMPARE(plan.configuration.program, script);
  QCOMPARE(plan.configuration.args, QStringList({"--day", "report"}));
  QCOMPARE(plan.configuration.cwd, m_workspace->path() + "/tools");
  QCOMPARE(plan.configuration.env.value("REPORT_MODE"), QString("full"));
  QCOMPARE(plan.configuration.env.value("PYTHONUNBUFFERED"), QString("1"));
  QVERIFY(plan.buildSteps.isEmpty());
  QVERIFY(!LaunchPlanner::fileDebuggerName(target).isEmpty());
}

void TestLaunchConfigurations::testDebugPlanCompilesWithTheTemplateCompiler() {
  const QString source =
      writeFile("src/hello.c", "int main(void) { return 0; }\n");
  RunTarget target =
      RunTargetResolver::resolve({source, "cpp", m_workspace->path(), {}, {}});
  QCOMPARE(target.templateId, QString("c_gcc"));
  FileDebugPlan plan = LaunchPlanner::debugPlanForFile(target);
  QVERIFY2(plan.isValid(), qPrintable(plan.error));
  QCOMPARE(plan.buildSteps.size(), 1);
  QCOMPARE(plan.buildSteps.first().command.first(), QString("gcc"));
  QVERIFY(plan.buildSteps.first().command.contains("-g"));
  QCOMPARE(plan.configuration.program, m_workspace->path() + "/src/hello");
  QCOMPARE(plan.configuration.type, QString("cppdbg"));

  FileTemplateAssignment assignment;
  assignment.templateId = "c_clang";
  assignment.compilerFlags = {"-Wall", "-lm"};
  assignment.sourceFiles = {"${fileDir}/util.c"};
  assignment.customArgs = {"input.txt"};
  QVERIFY(
      RunTemplateManager::instance().assignTemplateToFile(source, assignment));
  target =
      RunTargetResolver::resolve({source, "cpp", m_workspace->path(), {}, {}});
  plan = LaunchPlanner::debugPlanForFile(target);
  const QStringList build = plan.buildSteps.first().command;
  QCOMPARE(build.first(), QString("clang"));
  QVERIFY(build.contains(m_workspace->path() + "/src/util.c"));
  // Libraries after the sources so the linker resolves them.
  QVERIFY(build.indexOf("-lm") > build.indexOf(source));
  QCOMPARE(plan.configuration.args, QStringList({"input.txt"}));
}

void TestLaunchConfigurations::testDebugPlanBuildsADebuggableProgram() {
  if (QStandardPaths::findExecutable("gcc").isEmpty()) {
    QSKIP("gcc is not installed");
  }
  const QString source = writeFile(
      "built.c", "#include <stdio.h>\nint main(void) { puts(\"ok\"); }\n");
  // Run's template compiles without -g into the same path; Debug must not
  // reuse that binary.
  QProcess::execute("gcc", {"-o", m_workspace->path() + "/built", source});
  const RunTarget target =
      RunTargetResolver::resolve({source, "cpp", m_workspace->path(), {}, {}});
  const FileDebugPlan plan = LaunchPlanner::debugPlanForFile(target);
  QCOMPARE(plan.buildSteps.size(), 1);
  const LaunchBuildStep step = plan.buildSteps.first();
  QCOMPARE(QProcess::execute(step.command.first(), step.command.mid(1)), 0);
  QProcess objdump;
  objdump.start("objdump", {"-h", plan.configuration.program});
  if (!objdump.waitForFinished(10000)) {
    QSKIP("objdump is not available");
  }
  QVERIFY(objdump.readAllStandardOutput().contains(".debug_info"));
}

void TestLaunchConfigurations::testDebugPlanForModulesAndUndebuggableFiles() {
  const QString module = writeFile("jobs.py", "print(1)\n");
  FileTemplateAssignment assignment;
  assignment.templateId = "python_module";
  QVERIFY(
      RunTemplateManager::instance().assignTemplateToFile(module, assignment));
  FileDebugPlan plan = LaunchPlanner::debugPlanForFile(
      RunTargetResolver::resolve({module, "py", m_workspace->path(), {}, {}}));
  QVERIFY2(plan.isValid(), qPrintable(plan.error));
  QCOMPARE(plan.configuration.adapterConfig.value("module").toString(),
           QString("jobs"));

  // A pytest template debugs pytest on the file, whatever its name.
  const QString checks = writeFile("checks.py", "def test_ok():\n    pass\n");
  assignment.templateId = "python_pytest_file";
  QVERIFY(
      RunTemplateManager::instance().assignTemplateToFile(checks, assignment));
  plan = LaunchPlanner::debugPlanForFile(
      RunTargetResolver::resolve({checks, "py", m_workspace->path(), {}, {}}));
  QVERIFY2(plan.isValid(), qPrintable(plan.error));
  QCOMPARE(plan.configuration.adapterConfig.value("module").toString(),
           QString("pytest"));
  QVERIFY(plan.configuration.args.contains(checks));

  const QString tex = writeFile("paper.tex", "\\documentclass{article}\n");
  const RunTarget texTarget =
      RunTargetResolver::resolve({tex, "latex", m_workspace->path(), {}, {}});
  QVERIFY(texTarget.isValid());
  plan = LaunchPlanner::debugPlanForFile(texTarget);
  QVERIFY(!plan.isValid());
  QVERIFY2(plan.error.contains("LaTeX"), qPrintable(plan.error));
  QString reason;
  QVERIFY(LaunchPlanner::fileDebuggerName(texTarget, &reason).isEmpty());
  QVERIFY(!reason.isEmpty());

  const QString test = writeFile("suite_test.cpp", "int main() {}\n");
  assignment.templateId = "cpp_cmake_ctest";
  QVERIFY(
      RunTemplateManager::instance().assignTemplateToFile(test, assignment));
  plan = LaunchPlanner::debugPlanForFile(
      RunTargetResolver::resolve({test, "cpp", m_workspace->path(), {}, {}}));
  QVERIFY(!plan.isValid());
  QVERIFY(plan.error.contains("Tests panel"));
}

void TestLaunchConfigurations::testCMakeExecutablesKeepFileArguments() {
  writeFile("CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.16)\nproject(demo CXX)\n"
            "add_executable(demo main.cpp)\n");
  const QString source = writeFile("main.cpp", "int main() { return 0; }\n");
  FileTemplateAssignment assignment;
  assignment.customArgs = {"--level", "3"};
  assignment.customEnv.insert("DEMO", "1");
  QVERIFY(
      RunTemplateManager::instance().assignTemplateToFile(source, assignment));
  const RunTarget target =
      RunTargetResolver::resolve({source, "cpp", m_workspace->path(), {}, {}});
  QCOMPARE(target.kind, RunTarget::Kind::CMakeExecutable);
  QCOMPARE(target.arguments, QStringList({"--level", "3"}));
  QCOMPARE(target.environment.value("DEMO"), QString("1"));
  QCOMPARE(target.workingDirectory, QFileInfo(target.program).absolutePath());

  const FileDebugPlan plan = LaunchPlanner::debugPlanForFile(target);
  QVERIFY2(plan.isValid(), qPrintable(plan.error));
  QVERIFY(plan.requiresCMakeBuild);
  QCOMPARE(plan.configuration.args, QStringList({"--level", "3"}));
  QCOMPARE(plan.configuration.env.value("DEMO"), QString("1"));
}

// ----------------------------------------------------------------- store --

void TestLaunchConfigurations::testLoadingDoesNotCreateLaunchJson() {
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();
  QVERIFY(manager.loadFromLightpadDir());
  QVERIFY(manager.allConfigurations().isEmpty());
  QVERIFY(!QFile::exists(manager.lightpadLaunchConfigPath()));
  QVERIFY(!QDir(m_workspace->path() + "/.lightpad").exists());
}

void TestLaunchConfigurations::testLegacySamplesAreHidden() {
  // Exactly what earlier versions wrote into every project, plus one entry
  // the user added and one sample the user changed.
  const QByteArray legacy = R"({
    "version": "1.0.0", "_comment": "Debug launch configurations.",
    "configurations": [
      {"name": "Python: Current File", "adapterId": "python-debugpy",
       "type": "debugpy", "request": "launch", "program": "${file}",
       "console": "integratedTerminal", "cwd": "${workspaceFolder}",
       "_comment": "Example Python debug configuration"},
      {"name": "C++: GDB Launch", "adapterId": "cppdbg-gdb", "type": "cppdbg",
       "request": "launch", "program": "${workspaceFolder}/a.out", "args": [],
       "stopAtEntry": false, "cwd": "${workspaceFolder}", "environment": [],
       "externalConsole": false, "MIMode": "gdb", "miDebuggerPath": "gdb",
       "setupCommands": [{"description": "Enable pretty-printing for gdb",
                          "text": "-enable-pretty-printing",
                          "ignoreFailures": true}],
       "_comment": "Example C++ GDB debug configuration"},
      {"name": "Go: Current File", "adapterId": "go-delve", "type": "go",
       "request": "launch", "mode": "debug", "program": "${file}",
       "cwd": "${workspaceFolder}",
       "_comment": "Example Go debug configuration using Delve"},
      {"name": "Node.js: Current File", "adapterId": "node-debug",
       "type": "node", "request": "launch", "program": "${workspaceFolder}/srv.js",
       "cwd": "${workspaceFolder}", "console": "integratedTerminal",
       "_comment": "Example Node.js debug configuration"},
      {"name": "Mine", "type": "cppdbg", "request": "launch",
       "program": "/bin/true"}
    ],
    "compounds": [
      {"name": "Server + Client", "configurations": ["Server", "Client"],
       "stopAll": true,
       "_comment": "Example compound configuration for debugging multiple targets"}
    ]})";
  const QString path = writeFile(".lightpad/debug/launch.json", legacy);
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();
  QVERIFY(manager.loadFromFile(path));
  QStringList names;
  for (const DebugConfiguration &config : manager.allConfigurations()) {
    names << config.name;
  }
  QCOMPARE(names, QStringList({"Node.js: Current File", "Mine"}));
  QVERIFY(manager.allCompoundConfigurations().isEmpty());

  // A sample re-saved by the old dialog (empty fields dropped) is still one.
  DebugConfiguration resaved = DebugConfiguration::fromJson(
      QJsonDocument::fromJson(legacy)["configurations"][1].toObject());
  QVERIFY(
      DebugConfigurationManager::isLegacySampleConfiguration(resaved.toJson()));
}

void TestLaunchConfigurations::testConfigurationsKeepFileOrder() {
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();
  QSignalSpy changed(&manager,
                     &DebugConfigurationManager::configurationsChanged);
  DebugConfiguration zulu;
  zulu.name = "Zulu";
  zulu.type = "command";
  zulu.program = "z";
  DebugConfiguration alpha = zulu;
  alpha.name = "Alpha";
  manager.replaceConfigurations({zulu, alpha});
  CompoundDebugConfiguration both;
  both.name = "Both";
  both.configurations = {"Zulu", "Alpha"};
  manager.addCompoundConfiguration(both);
  QVERIFY(changed.count() >= 2);
  QCOMPARE(manager.uniqueName("Alpha"), QString("Alpha (2)"));
  QCOMPARE(manager.uniqueName("Both"), QString("Both (2)"));

  QVERIFY(manager.saveToLightpadDir());
  manager.replaceConfigurations({});
  manager.replaceCompoundConfigurations({});
  QVERIFY(manager.loadFromLightpadDir());
  QCOMPARE(manager.allConfigurations().first().name, QString("Zulu"));
  QCOMPARE(manager.allConfigurations().last().name, QString("Alpha"));
  QCOMPARE(manager.compoundConfiguration("Both").configurations,
           QStringList({"Zulu", "Alpha"}));
}

// ---------------------------------------------------------------- dialog --

void TestLaunchConfigurations::testDialogEditsStayLocalUntilSave() {
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();
  DebugConfiguration a;
  a.name = "Dialog Test A";
  a.type = "cppdbg";
  a.request = "launch";
  a.program = "/a";
  a.args = {"one arg", "two"};
  a.adapterConfig["MIMode"] = "gdb";
  DebugConfiguration b;
  b.name = "Dialog Test B";
  b.type = "cppdbg";
  b.request = "launch";
  b.program = "/b";
  manager.replaceConfigurations({a, b});

  {
    LaunchConfigurationsDialog::Context context;
    context.workspaceFolder = m_workspace->path();
    context.selection = LaunchTarget::configuration(a.name);
    LaunchConfigurationsDialog dialog(context);
    QCOMPARE(dialog.m_currentIndex, 0);
    QCOMPARE(dialog.m_argsEdit->text(), QString("\"one arg\" two"));

    dialog.m_advancedEdit->setPlainText("{ not json");
    QTreeWidgetItem *other =
        dialog.m_tree->findItems(b.name, Qt::MatchExactly | Qt::MatchRecursive)
            .value(0);
    QVERIFY(other);
    dialog.m_tree->setCurrentItem(other);
    QCOMPARE(dialog.m_currentIndex, 0);
    QVERIFY(!dialog.m_validationLabel->isHidden());
    QVERIFY(!dialog.save());

    dialog.m_advancedEdit->setPlainText("{\"MIMode\": \"lldb\"}");
    dialog.m_nameEdit->setText(b.name);
    QVERIFY(!dialog.commitCurrentEntry());
    dialog.m_nameEdit->setText("Dialog Test Renamed");
    QVERIFY(dialog.commitCurrentEntry());
    QVERIFY(dialog.m_validationLabel->isHidden());
    // Nothing reaches the manager before Save.
    QCOMPARE(manager.configuration(a.name).adapterConfig.value("MIMode"),
             QJsonValue("gdb"));
    QVERIFY(!manager.hasConfiguration("Dialog Test Renamed"));
    dialog.reject();
  }
  QCOMPARE(manager.allConfigurations().size(), 2);
  QCOMPARE(manager.configuration(a.name).program, QString("/a"));
  QVERIFY(!QFile::exists(manager.lightpadLaunchConfigPath()));
}

void TestLaunchConfigurations::testDialogSavesSelectedArguments() {
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();
  DebugConfiguration a;
  a.name = "A: Other Program";
  a.type = "cppdbg";
  a.program = "/other";
  DebugConfiguration b = a;
  b.name = "B: GDB Launch";
  b.adapterId = "cppdbg-gdb";
  b.request = "launch";
  b.program = "${workspaceFolder}/vulnerable";
  manager.replaceConfigurations({a, b});

  LaunchConfigurationsDialog::Context context;
  context.workspaceFolder = m_workspace->path();
  context.selection = LaunchTarget::configuration(b.name);
  LaunchConfigurationsDialog dialog(context);
  QCOMPARE(dialog.m_nameEdit->text(), b.name);
  dialog.m_argsEdit->setText("AAAA \"two words\"");
  QSignalSpy accepted(&dialog, &QDialog::accepted);
  dialog.m_saveButton->click();
  QCOMPARE(accepted.count(), 1);
  QCOMPARE(dialog.selectedTarget(), LaunchTarget::configuration(b.name));
  QVERIFY(!dialog.requestedLaunch().has_value());

  manager.replaceConfigurations({});
  QVERIFY(manager.loadFromLightpadDir());
  const DebugConfiguration saved =
      manager.resolveVariables(manager.configuration(b.name));
  QCOMPARE(saved.args, QStringList({"AAAA", "two words"}));
  QCOMPARE(saved.program, m_workspace->path() + "/vulnerable");
  QCOMPARE(saved.adapterId, QString("cppdbg-gdb"));
  const auto adapter = DebugAdapterRegistry::instance().adapter("cppdbg-gdb");
  QVERIFY(adapter);
  QCOMPARE(adapter->launchArguments(saved)["args"].toArray(),
           QJsonArray({"AAAA", "two words"}));
}

void TestLaunchConfigurations::testDialogDebouncesDebuggerProbe() {
  DebugConfiguration config;
  config.name = "Probe";
  config.type = "debugpy";
  config.request = "launch";
  DebugConfigurationManager::instance().replaceConfigurations({config});

  LaunchConfigurationsDialog::Context context;
  context.workspaceFolder = m_workspace->path();
  LaunchConfigurationsDialog dialog(context);
  QVERIFY(dialog.m_statusTimer->isActive());
  for (const QChar ch : QString("/tmp/prog")) {
    dialog.m_programEdit->setText(dialog.m_programEdit->text() + ch);
  }
  QTRY_VERIFY_WITH_TIMEOUT(!dialog.m_configurationPreviewTimer->isActive(),
                           5000);
  QVERIFY(dialog.m_debuggerStatus->text().contains("checking"));
  QTRY_VERIFY_WITH_TIMEOUT(
      !dialog.m_debuggerStatus->text().contains("checking"), 30000);
  QCOMPARE(dialog.m_statusCache.size(), 1);
}

void TestLaunchConfigurations::testDialogFileSettingsRoundTrip() {
  RunTemplateManager &templates = RunTemplateManager::instance();
  const QString source = writeFile("quote_roundtrip.cpp", "int main() {}\n");
  FileTemplateAssignment original;
  original.templateId = "cpp_clang";
  original.customArgs = {"--gtest_filter", "Suite Name.*"};
  original.compilerFlags = {"-DTEST_LABEL=With Space", "-O2"};
  original.customEnv.insert("MODE", "x y");
  QVERIFY(templates.assignTemplateToFile(source, original));

  LaunchConfigurationsDialog::Context context;
  context.workspaceFolder = m_workspace->path();
  context.currentFilePath = source;
  context.languageId = "cpp";
  LaunchConfigurationsDialog dialog(context);
  QCOMPARE(dialog.m_currentKind, LaunchConfigurationsDialog::EntryKind::File);
  QCOMPARE(dialog.m_fileTemplateCombo->currentData().toString(),
           QString("cpp_clang"));
  QVERIFY(dialog.m_fileArgsEdit->text().contains("\"Suite Name.*\""));
  QVERIFY(
      dialog.m_fileFlagsEdit->text().contains("\"-DTEST_LABEL=With Space\""));
  QVERIFY(dialog.m_fileFlagsEdit->isVisibleTo(&dialog));

  // The preview shows exactly what Run and Debug will do with the edits.
  dialog.m_fileArgsEdit->setText("--fast");
  dialog.refreshFilePreview();
  const QString preview = dialog.m_filePreview->text();
  QVERIFY2(preview.contains("--fast"), qPrintable(preview));
  QVERIFY2(preview.contains("clang"), qPrintable(preview));
  QVERIFY(templates.getAssignmentForFile(source).customArgs ==
          original.customArgs);

  QVERIFY(dialog.save());
  const FileTemplateAssignment saved = templates.getAssignmentForFile(source);
  QCOMPARE(saved.templateId, QString("cpp_clang"));
  QCOMPARE(saved.customArgs, QStringList({"--fast"}));
  QCOMPARE(saved.compilerFlags, original.compilerFlags);
  QCOMPARE(saved.customEnv, original.customEnv);

  // Reset to Defaults forgets the file's settings.
  dialog.m_fileResetButton->click();
  QCOMPARE(dialog.m_fileTemplateCombo->currentIndex(), 0);
  QVERIFY(dialog.save());
  QVERIFY(templates.getAssignmentForFile(source).isEmpty());
}

void TestLaunchConfigurations::testDialogViewingChangesNothing() {
  const QString script = writeFile("app.py", "print(1)\n");
  DebugConfiguration config;
  config.name = "Python App";
  config.type = "debugpy";
  config.request = "launch";
  config.program = "${workspaceFolder}/app.py";
  DebugConfigurationManager::instance().replaceConfigurations({config});

  LaunchConfigurationsDialog::Context context;
  context.workspaceFolder = m_workspace->path();
  context.currentFilePath = script;
  context.languageId = "py";
  LaunchConfigurationsDialog dialog(context);
  // Visit every entry, then save without editing anything.
  for (QTreeWidgetItemIterator it(dialog.m_tree); *it; ++it) {
    dialog.m_tree->setCurrentItem(*it);
  }
  QVERIFY(dialog.save());
  QVERIFY(!QFile::exists(m_workspace->path() + "/.lightpad/run_config.json"));
  QVERIFY(!QFile::exists(
      DebugConfigurationManager::instance().lightpadLaunchConfigPath()));
  QCOMPARE(DebugConfigurationManager::instance()
               .configuration("Python App")
               .toJson(),
           config.toJson());
}

void TestLaunchConfigurations::testDialogTemplatesAndCompounds() {
  LaunchConfigurationsDialog::Context context;
  context.workspaceFolder = m_workspace->path();
  LaunchConfigurationsDialog dialog(context);
  QCOMPARE(dialog.m_pages->currentIndex(), 3);

  const QString server = dialog.addConfigurationFromTemplate("python.fastapi");
  QCOMPARE(server, QString("Python: FastAPI (uvicorn)"));
  QCOMPARE(dialog.m_moduleEdit->text(), QString("uvicorn"));
  QVERIFY(dialog.m_moduleEdit->isVisibleTo(&dialog));
  QVERIFY(!dialog.m_runtimeArgsEdit->isVisibleTo(&dialog));
  const QString client = dialog.addConfigurationFromTemplate("tools.command");
  QVERIFY(!dialog.m_debuggerGroup->isVisibleTo(&dialog));
  QVERIFY(!dialog.m_debugButton->isEnabled());
  dialog.m_programEdit->setText("curl");
  QVERIFY(dialog.addConfigurationFromTemplate("missing.template").isEmpty());

  dialog.addCompound();
  QCOMPARE(dialog.m_compounds.size(), 1);
  QCOMPARE(dialog.m_compounds.first().configurations,
           QStringList({server, client}));

  // Renaming a configuration updates the compounds that start it.
  QTreeWidgetItem *serverItem =
      dialog.m_tree->findItems(server, Qt::MatchExactly | Qt::MatchRecursive)
          .value(0);
  QVERIFY(serverItem);
  dialog.m_tree->setCurrentItem(serverItem);
  dialog.m_nameEdit->setText("API");
  QVERIFY(dialog.commitCurrentEntry());
  QCOMPARE(dialog.m_compounds.first().configurations,
           QStringList({"API", client}));
  dialog.removeCurrent();
  QCOMPARE(dialog.m_compounds.first().configurations, QStringList({client}));

  QVERIFY(dialog.save());
  DebugConfigurationManager &manager = DebugConfigurationManager::instance();
  QCOMPARE(manager.allConfigurations().size(), 1);
  QCOMPARE(manager.configuration(client).program, QString("curl"));
  QCOMPARE(manager.configuration(client).type, QString("command"));
  QCOMPARE(manager.allCompoundConfigurations().size(), 1);
  QVERIFY(QFile::exists(manager.lightpadLaunchConfigPath()));
}

void TestLaunchConfigurations::testDialogRunButtonRequestsLaunch() {
  const QString script = writeFile("main.py", "print(1)\n");
  LaunchConfigurationsDialog::Context context;
  context.workspaceFolder = m_workspace->path();
  context.currentFilePath = script;
  context.languageId = "py";
  LaunchConfigurationsDialog dialog(context);
  QVERIFY(dialog.m_runButton->isEnabled());
  QVERIFY(dialog.m_debugButton->isEnabled());
  dialog.m_debugButton->click();
  QCOMPARE(dialog.result(), int(QDialog::Accepted));
  QCOMPARE(dialog.requestedLaunch(),
           std::optional<LaunchMode>(LaunchMode::Debug));
  QVERIFY(dialog.selectedTarget().isCurrentFile());
}

void TestLaunchConfigurations::testGalleryFiltersTemplates() {
  LaunchTemplateGallery gallery("py", m_workspace->path());
  QCOMPARE(gallery.selectedTemplateId(), QString("python.file"));
  gallery.m_search->setText("fastapi");
  QCOMPARE(gallery.selectedTemplateId(), QString("python.fastapi"));
  QVERIFY(gallery.m_createButton->isEnabled());
  gallery.m_search->setText("gdbserver remote");
  QCOMPARE(gallery.selectedTemplateId(), QString("cpp.gdbserver"));
  gallery.m_search->setText("no such template at all");
  QVERIFY(gallery.selectedTemplateId().isEmpty());
  QVERIFY(!gallery.m_createButton->isEnabled());
}

void TestLaunchConfigurations::testJsonAdaptersAreRegistered() {
  DebugAdapterRegistry &registry = DebugAdapterRegistry::instance();
  for (const QString &id :
       {QStringLiteral("lldb-dap"), QStringLiteral("netcoredbg"),
        QStringLiteral("dart"), QStringLiteral("flutter"),
        QStringLiteral("ocamlearlybird")}) {
    QVERIFY2(registry.adapter(id), qPrintable(id));
  }
  QVERIFY(!registry.adaptersForType("coreclr").isEmpty());
  QVERIFY(!registry.adaptersForLanguage("dart").isEmpty());
  const auto lldb = registry.adapter("lldb-dap");
  QVERIFY(lldb->config().extensions.contains(".swift"));
  QVERIFY(lldb->canDebug("/w/main.zig"));
  // A missing adapter explains how to get it.
  if (!lldb->isAvailable()) {
    QVERIFY(lldb->statusMessage().contains("LLVM"));
  }
  // The C++ adapters are still there.
  QVERIFY(registry.adapter("debugpy") ||
          !registry.adaptersForType("debugpy").isEmpty());
}

void TestLaunchConfigurations::testGenericAdapterBuildsRequests() {
  const auto adapter = DebugAdapterRegistry::instance().adapter("lldb-dap");
  QVERIFY(adapter);
  const QJsonObject launch = adapter->createLaunchConfig("/w/src/tool.c", "");
  QCOMPARE(launch.value("type").toString(), QString("lldb-dap"));
  QCOMPARE(launch.value("request").toString(), QString("launch"));
  QCOMPARE(launch.value("program").toString(), QString("/w/src/tool"));
  QCOMPARE(launch.value("cwd").toString(), QString("/w/src"));

  DebugConfiguration attach;
  attach.type = "lldb-dap";
  attach.request = "attach";
  attach.processId = 4242;
  const QJsonObject arguments = adapter->attachArguments(attach);
  QCOMPARE(arguments.value("pid").toInt(), 4242);
  QVERIFY(!arguments.contains("processId"));
  QCOMPARE(adapter->createAttachConfig(7).value("pid").toInt(), 7);

  const auto dotnet = DebugAdapterRegistry::instance().adapter("netcoredbg");
  QVERIFY(!dotnet->config().exceptionBreakpointFilters.isEmpty());
  QCOMPARE(dotnet->configForConfiguration(DebugConfiguration{}).arguments,
           QStringList{"--interpreter=vscode"});
}

void TestLaunchConfigurations::testGenericAdapterRunCommand() {
  DebugConfiguration config;
  config.type = "coreclr";
  config.request = "launch";
  config.program = "/w/bin/app.dll";
  config.args = {"--port", "80"};
  config.cwd = "/w";
  RunCommand command = LaunchPlanner::runCommandFor(config);
  QCOMPARE(command.program, QString("dotnet"));
  QCOMPARE(command.arguments, QStringList({"/w/bin/app.dll", "--port", "80"}));

  config.type = "flutter";
  config.program = "lib/main.dart";
  config.args = {"-d", "linux"};
  command = LaunchPlanner::runCommandFor(config);
  QCOMPARE(command.program, QString("flutter"));
  QCOMPARE(command.arguments,
           QStringList({"run", "--target", "lib/main.dart", "-d", "linux"}));

  // Without a run recipe the program itself runs.
  config.type = "lldb-dap";
  config.program = "/w/build/app";
  config.args = {"x"};
  command = LaunchPlanner::runCommandFor(config);
  QCOMPARE(command.program, QString("/w/build/app"));
  QCOMPARE(command.arguments, QStringList{"x"});
}

void TestLaunchConfigurations::testAdapterDefinitionsParse() {
  const QJsonObject root = QJsonDocument::fromJson(R"({
    "adapters": [
      {"id": "mine", "name": "My Debugger", "type": "mylang",
       "command": "my-dap", "arguments": ["--stdio"], "extensions": ["my"],
       "languages": ["mylang"], "transport": "server",
       "supports": {"logPoints": false},
       "launch": {"program": "${file}", "flags": ["${fileBasenameNoExtension}"]}},
      {"id": "no-type", "command": ["x"]},
      {"id": "no-command", "type": "t"}
    ]})")
                               .object();
  const QList<DebugAdapterDefinition> definitions =
      GenericDebugAdapter::parse(root);
  QCOMPARE(definitions.size(), 1);
  const DebugAdapterDefinition &d = definitions.first();
  QCOMPARE(d.commands, QStringList{"my-dap"});
  QCOMPARE(d.extensions, QStringList{".my"});
  QVERIFY(d.serverTransport);
  QVERIFY(!d.supportsLogPoints);
  GenericDebugAdapter adapter(d);
  QCOMPARE(adapter.config().id, QString("mine"));
  QVERIFY(!adapter.config().supportsLogPoints);
  const QJsonObject launch = adapter.createLaunchConfig("/p/demo.my", "/p");
  QCOMPARE(launch.value("program").toString(), QString("/p/demo.my"));
  QCOMPARE(launch.value("flags").toArray().first().toString(), QString("demo"));
  QCOMPARE(launch.value("cwd").toString(), QString("/p"));
  QVERIFY(adapter.canDebug("/p/demo.my"));
  QVERIFY(!adapter.isAvailable());
  QVERIFY(adapter.statusMessage().contains("my-dap"));
  QVERIFY(GenericDebugAdapter::userDefinitionsPath().endsWith(
      "debug_adapters.json"));
}

void TestLaunchConfigurations::testNewDebuggerTemplates() {
  const LaunchTemplateCatalog &catalog = LaunchTemplateCatalog::instance();
  for (const QString &id :
       {QStringLiteral("cpp.lldb-dap"), QStringLiteral("dotnet.launch"),
        QStringLiteral("dotnet.attach"), QStringLiteral("dart.file"),
        QStringLiteral("flutter.app"), QStringLiteral("ocaml.bytecode")}) {
    const LaunchTemplate entry = catalog.templateById(id);
    QVERIFY2(entry.isValid(), qPrintable(id));
    QVERIFY(entry.isDebuggable());
    const DebugConfiguration config = entry.instantiate(entry.name);
    QVERIFY2(!DebugAdapterRegistry::instance()
                  .adaptersForConfiguration(config)
                  .isEmpty(),
             qPrintable(id));
    QVERIFY(!LaunchPlanner::debuggerNameFor(config).isEmpty());
  }
  // Glob markers: a .csproj makes the .NET template a suggestion.
  QTemporaryDir dir;
  QFile project(dir.filePath("Shop.csproj"));
  QVERIFY(project.open(QIODevice::WriteOnly));
  project.close();
  const QList<LaunchTemplate> suggestions =
      catalog.suggestionsFor("cs", dir.path());
  QVERIFY(!suggestions.isEmpty());
  QCOMPARE(suggestions.first().category, QString(".NET"));
}

void TestLaunchConfigurations::testDebuggerChatterIsFiltered() {
  const QString banner =
      "GNU gdb (Ubuntu 15.0.50) 15.0.50\n"
      "Copyright (C) 2024 Free Software Foundation, Inc.\n"
      "License GPLv3+: GNU GPL version 3 or later "
      "<http://gnu.org/licenses/gpl.html>\n"
      "Type \"apropos word\" to search for commands related to \"word\".\n"
      "No source file named /w/prog.c.\n"
      "Breakpoint 1 (-source /w/prog.c -line 9) pending.\n";
  QCOMPARE(DebugOutputFilter::withoutAdapterChatter(banner), QString());
  QCOMPARE(DebugOutputFilter::withoutAdapterChatter(
               "Copyright (C) 2024 Free Software Foundation, Inc.\n"
               "Program received signal SIGSEGV\n"),
           QString("Program received signal SIGSEGV\n"));
  const QString program = "Copyright notice of my app\n";
  QCOMPARE(DebugOutputFilter::withoutAdapterChatter(program), program);
}

void TestLaunchConfigurations::testInlineValuesStayInTheirFunction() {
  const QStringList python = {
      "class Account:",                 // 1
      "    def __init__(self, owner):", // 2
      "        self.owner = owner",     // 3
      "",                               // 4
      "    def deposit(self, amount):", // 5
      "        self.balance += amount", // 6
      "        return self.balance",    // 7
      "",                               // 8
      "def main():",                    // 9
      "    total = 0",                  // 10
      "    for a in accounts:",         // 11
      "        total += a.deposit(5)",  // 12
      "        main_helper()",          // 13
  };
  QCOMPARE(DebugInlineScope::scopeStartLine(python, 7, "deposit"), 5);
  QCOMPARE(DebugInlineScope::scopeStartLine(python, 6, "Account.deposit"), 5);
  QCOMPARE(DebugInlineScope::scopeStartLine(python, 13, "main"), 9);
  // Module level and anonymous frames fall back to a window above the line.
  QCOMPARE(DebugInlineScope::scopeStartLine(python, 12, "<module>"), 1);
  QCOMPARE(DebugInlineScope::scopeStartLine(python, 12, "nowhere"), 1);

  const QStringList cpp = {
      "static int area(const point *a, const point *b) {", // 1
      "  int w = b->x - a->x;",                            // 2
      "  return w * area2(a);",                            // 3
      "}",                                                 // 4
      "int main(int argc, char **argv) {",                 // 5
      "  int r = area(&p, &q);",                           // 6
      "  return r;",                                       // 7
      "}",                                                 // 8
  };
  QCOMPARE(DebugInlineScope::scopeStartLine(cpp, 3, "area"), 1);
  QCOMPARE(DebugInlineScope::scopeStartLine(cpp, 7, "main"), 5);
  // A recursive call at the body's depth is not taken for the definition.
  const QStringList recursive = {"int fact(int n) {",          // 1
                                 "  int below = fact(n - 1);", // 2
                                 "  return n * below;", "}"};  // 3
  QCOMPARE(
      DebugInlineScope::scopeStartLine(recursive, 3, "ns::Math::fact(int)"), 1);
  // Stopped on the signature itself (function breakpoint).
  QCOMPARE(DebugInlineScope::scopeStartLine(cpp, 5, "main"), 5);
}

void TestLaunchConfigurations::testWatchesAreNotDuplicated() {
  WatchManager &watches = WatchManager::instance();
  const int before = watches.allWatches().size();
  const int first = watches.addWatch("len(items)");
  const int again = watches.addWatch("  len(items) ");
  QCOMPARE(again, first);
  QCOMPARE(watches.allWatches().size(), before + 1);
  watches.removeWatch(first);
}

void TestLaunchConfigurations::testBreakpointEditorEditsEverythingAtOnce() {
  BreakpointManager &manager = BreakpointManager::instance();
  manager.clearAll();
  const QString file = "/w/app.py";
  QVERIFY(BreakpointEditorDialog::conditionExample(file).contains("total"));
  QVERIFY(BreakpointEditorDialog::conditionExample("/w/a.c").contains("=="));

  // A new breakpoint: condition, hit count and a log message in one go.
  Breakpoint fresh;
  fresh.filePath = file;
  fresh.line = 12;
  BreakpointEditorDialog dialog(fresh);
  QVERIFY(!dialog.findChild<QPushButton *>("breakpointRemove"));
  dialog.findChild<QLineEdit *>("breakpointCondition")->setText(" total > 3 ");
  dialog.findChild<QLineEdit *>("breakpointHitCondition")->setText(">= 2");
  dialog.findChild<QRadioButton *>("breakpointLogMode")->setChecked(true);
  dialog.findChild<QLineEdit *>("breakpointLogMessage")->setText("t={total}");
  const Breakpoint edited = dialog.breakpoint();
  QCOMPARE(edited.condition, QString("total > 3"));
  QVERIFY(edited.isLogpoint);
  BreakpointEditorDialog::apply(edited);
  Breakpoint stored = manager.breakpointAt(file, 12);
  QVERIFY(stored.id > 0);
  QCOMPARE(stored.condition, QString("total > 3"));
  QCOMPARE(stored.hitCondition, QString(">= 2"));
  QCOMPARE(stored.logMessage, QString("t={total}"));
  QVERIFY(stored.isLogpoint);

  // Editing it back to a plain, disabled breakpoint.
  BreakpointEditorDialog again(stored);
  QVERIFY(again.findChild<QPushButton *>("breakpointRemove"));
  QVERIFY(
      !again.findChild<QLineEdit *>("breakpointLogMessage")->text().isEmpty());
  again.findChild<QCheckBox *>("breakpointEnabled")->setChecked(false);
  again.findChild<QLineEdit *>("breakpointCondition")->clear();
  again.findChild<QRadioButton *>("breakpointLogMode")->setChecked(false);
  for (auto *radio : again.findChildren<QRadioButton *>()) {
    if (radio != again.findChild<QRadioButton *>("breakpointLogMode")) {
      radio->setChecked(true);
    }
  }
  BreakpointEditorDialog::apply(again.breakpoint());
  stored = manager.breakpointAt(file, 12);
  QVERIFY(!stored.enabled);
  QVERIFY(stored.condition.isEmpty());
  QVERIFY(!stored.isLogpoint);
  QCOMPARE(manager.allBreakpoints().size(), 1);
  manager.clearAll();
}

QTEST_MAIN(TestLaunchConfigurations)
#include "test_launchconfigurations.moc"
