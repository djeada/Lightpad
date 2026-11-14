#include "mainwindow_internal.h"

#include "../python/pythonbackendproject.h"
#include "dialogs/pythonenvironmentdialog.h"
#include "panels/databasepanel.h"
#include "panels/terminal.h"
#include "panels/testpanel.h"
#include "paneltypography.h"
#include <QSaveFile>

void MainWindow::runBackendCommand(const QString &title, const QString &program,
                                   const QStringList &arguments,
                                   const PythonBackendProject &project) {
  showTerminalPanel();
  Terminal *terminal =
      terminalWidget->addNewTerminal(project.workingDirectory, title);
  terminalWidget->setCurrentTerminal(terminal);
  auto environment = project.environment();
  const auto python = PythonProjectEnvironment::resolve(
      project.pythonPreference(), project.root, project.entryFile,
      project.workingDirectory);
  if (program.trimmed().isEmpty()) {
    terminal->appendNotice(tr("No executable is configured. Check the Python "
                              "environment and backend settings."),
                           true);
    return;
  }
  const auto activation =
      PythonProjectEnvironment::activationEnvironment(python);
  for (auto it = activation.begin(); it != activation.end(); ++it)
    environment[it.key()] = it.value();
  terminal->setPythonEnvironmentBanner(python);
  terminal->executeCommand(program, arguments, project.workingDirectory,
                           environment);
}

void MainWindow::openBackendEnvironment(const PythonBackendProject &project) {
  auto *dialog = new PythonEnvironmentDialog(project.root, project.entryFile,
                                             getTheme(), this);
  connect(dialog, &PythonEnvironmentDialog::configurationSaved, this, [this]() {
    invalidateRunTargetCache();
    updatePythonEnvironmentLabel();
    refreshRunTargetButton();
  });
  dialog->show();
}

void MainWindow::setupBackendMenu() {
  QMenu *menu = new QMenu(tr("Backend"), this);
  menu->setObjectName("menuBackend");
  menuBar()->insertMenu(ui->menuHelp->menuAction(), menu);
  QAction *summary = menu->addAction(tr("Open a Python backend project"));
  summary->setEnabled(false);

  // Keep actions stable so the command palette can register them at startup.
  auto addAction = [this, menu](
                       const QString &label, const QString &id,
                       std::function<void(const PythonBackendProject &)> fn) {
    QAction *action = menu->addAction(label);
    action->setObjectName(id);
    connect(action, &QAction::triggered, this, [this, fn]() {
      const auto project = PythonBackendProject::discover(m_projectRootPath);
      if (!project.isValid()) {
        ThemedMessageBox::warning(
            this, tr("Backend"),
            project.error.isEmpty()
                ? tr("Open a FastAPI or Flask project, or configure "
                     ".lightpad/backend.json.")
                : project.error);
        return;
      }
      fn(project);
    });
    return action;
  };

  addAction(tr("Run Backend Server"), "backendRun",
            [this](const auto &project) {
              executeRunTarget(RunTargetResolver::backendTarget(project));
            });
  addAction(tr("Stop Backend Server"), "backendStop", [this](const auto &) {
    if (m_backendTerminal && m_backendTerminalRoot == m_projectRootPath)
      m_backendTerminal->stopProcess();
  });
  addAction(
      tr("Debug Backend Server"), "backendDebug", [this](const auto &project) {
        if (!project.entryFile.isEmpty()) {
          startDebuggingForFile(project.entryFile);
          return;
        }
        DebugConfiguration config;
        config.name = "Debug " + project.name();
        config.type = "debugpy";
        config.request = "launch";
        config.cwd = project.workingDirectory;
        config.args = project.serverArguments(true);
        config.env = project.environment();
        config.adapterConfig["module"] = project.module();
        const auto preference = project.pythonPreference();
        config.pythonMode = preference.mode;
        config.pythonInterpreter = preference.customInterpreter;
        config.pythonVenvPath = preference.venvPath;
        config.pythonRequirementsFile = preference.requirementsFile;
        const QString id = DebugSessionManager::instance().startSession(config);
        if (!id.isEmpty()) {
          attachDebugSession(id);
          debugDock->show();
          debugDock->raise();
        } else {
          ThemedMessageBox::warning(
              this, tr("Debug Backend"),
              DebugSessionManager::instance().lastError());
        }
      });
  menu->addSeparator();
  addAction(tr("Run Backend Tests"), "backendTests", [this](const auto &) {
    ensureTestPanel();
    if (testPanel) {
      testPanel->runTestsForPath(m_projectRootPath);
      revealTestDock();
      syncViewToggleActionStates();
    }
  });
  addAction(tr("Discover Backend Tests"), "backendDiscoverTests",
            [this](const auto &) {
              ensureTestPanel();
              if (testPanel) {
                testPanel->discoverTests();
                revealTestDock();
                syncViewToggleActionStates();
              }
            });
  addAction(tr("Debug Backend Tests"), "backendDebugTests",
            [this](const auto &) {
              ensureTestPanel();
              if (testPanel)
                testPanel->debugAllTests();
            });
  addAction(tr("Check with Ruff"), "backendLint", [this](auto project) {
    project.workingDirectory = project.root;
    const auto python = PythonProjectEnvironment::resolve(
        project.pythonPreference(), project.root);
    runBackendCommand(tr("Ruff"), python.interpreter,
                      {"-m", "ruff", "check", "."}, project);
  });
  addAction(tr("Python Environment and Dependencies…"), "backendEnvironment",
            [this](const auto &project) { openBackendEnvironment(project); });
  addAction(tr("Install Development Dependencies"), "backendInstall",
            [this](auto project) {
              const auto python = PythonProjectEnvironment::resolve(
                  project.pythonPreference(), project.root);
              if (!python.isVirtualEnvironment()) {
                openBackendEnvironment(project);
                return;
              }
              const auto plan =
                  PythonProjectEnvironment::requirementsInstallPlan(
                      python, project.root, project.entryFile, project.root,
                      project.requirementsFile());
              if (plan.arguments.isEmpty())
                return;
              project.workingDirectory = plan.workingDirectory;
              runBackendCommand(tr("Install dependencies"), python.interpreter,
                                plan.arguments, project);
            });
  addAction(tr("Install Python Debugger"), "backendInstallDebugger",
            [this](const auto &project) {
              const auto python = PythonProjectEnvironment::resolve(
                  project.pythonPreference(), project.root);
              if (!python.isVirtualEnvironment()) {
                openBackendEnvironment(project);
                return;
              }
              runBackendCommand(tr("Install debugpy"), python.interpreter,
                                {"-m", "pip", "install", "debugpy"}, project);
            });
  menu->addSeparator();
  addAction(tr("Open Website"), "backendWebsite", [](const auto &project) {
    QDesktopServices::openUrl(QUrl(project.baseUrl()));
  });
  addAction(tr("Open API Documentation"), "backendApiDocs",
            [](const auto &project) {
              QDesktopServices::openUrl(
                  QUrl(project.baseUrl() +
                       project.settings.value("docsPath").toString("/docs")));
            });
  addAction(
      tr("Open Health Endpoint"), "backendHealth", [](const auto &project) {
        QDesktopServices::openUrl(
            QUrl(project.baseUrl() +
                 project.settings.value("healthPath").toString("/health")));
      });
  QAction *database = menu->addAction(tr("Open Project Database…"));
  database->setObjectName("backendDatabase");
  connect(database, &QAction::triggered, this,
          &MainWindow::openBackendDatabase);
  if (m_databaseMenu)
    m_databaseMenu->addAction(database);
  addAction(tr("Open Environment File…"), "backendEnvFile",
            [this](const auto &project) {
              if (QFileInfo::exists(project.envFile))
                openFileAndAddToNewTab(project.envFile);
              else
                statusBar()->showMessage(
                    tr("No environment file at %1").arg(project.envFile), 8000);
            });

  menu->addSeparator();
  addAction(tr("Docker Compose Up"), "backendComposeUp", [this](auto project) {
    if (project.composeFile().isEmpty())
      return;
    project.workingDirectory = project.root;
    runBackendCommand(tr("Compose"), "docker",
                      {"compose", "-f", project.composeFile(), "up", "--build"},
                      project);
  });
  addAction(
      tr("Docker Compose Down"), "backendComposeDown", [this](auto project) {
        if (project.composeFile().isEmpty())
          return;
        project.workingDirectory = project.root;
        runBackendCommand(tr("Compose down"), "docker",
                          {"compose", "-f", project.composeFile(), "down"},
                          project);
      });
  QMenu *tasks = menu->addMenu(tr("Project Tasks"));
  connect(tasks, &QMenu::aboutToShow, this, [this, tasks]() {
    tasks->clear();
    const auto project = PythonBackendProject::discover(m_projectRootPath);
    for (const auto &value : project.settings.value("tasks").toArray()) {
      const auto task = value.toObject();
      const QString command = task.value("command").toString();
      const QString name = task.value("name").toString();
      if (name.isEmpty() || command.isEmpty())
        continue;
      QAction *action = tasks->addAction(name);
      connect(action, &QAction::triggered, this,
              [this, project = project, task, name, command]() mutable {
                QStringList args;
                for (const auto &arg : task.value("args").toArray())
                  args << arg.toString();
                QString program = command;
                if (program == "${python}")
                  program = PythonProjectEnvironment::resolve(
                                project.pythonPreference(), project.root)
                                .interpreter;
                const QString directory =
                    task.value("workingDirectory").toString();
                if (!directory.isEmpty())
                  project.workingDirectory =
                      QDir(project.root).absoluteFilePath(directory);
                const auto preference = project.pythonPreference();
                for (QString &arg : args)
                  arg = PythonProjectEnvironment::substituteVariables(
                      arg, project.root, project.entryFile,
                      project.workingDirectory, preference);
                auto environment = project.settings.value("env").toObject();
                const auto taskEnvironment = task.value("env").toObject();
                for (auto it = taskEnvironment.begin();
                     it != taskEnvironment.end(); ++it)
                  environment[it.key()] = it.value();
                project.settings["env"] = environment;
                runBackendCommand(name, program, args, project);
              });
    }
    if (tasks->isEmpty())
      tasks->addAction(tr("Add tasks in .lightpad/backend.json"))
          ->setEnabled(false);
  });
  QAction *configure = menu->addAction(tr("Configure Backend…"));
  configure->setObjectName("backendConfigure");
  connect(configure, &QAction::triggered, this, &MainWindow::configureBackend);
  connect(menu, &QMenu::aboutToShow, this, [this, summary]() {
    const auto project = PythonBackendProject::discover(m_projectRootPath);
    summary->setText(project.isValid() ? project.name()
                     : !project.error.isEmpty()
                         ? project.error
                         : tr("No Python backend detected"));
  });
}

void MainWindow::openBackendDatabase() {
  const auto project = PythonBackendProject::discover(m_projectRootPath);
  const QStringList files = project.databaseFiles();
  if (files.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Project Database"),
        tr("No existing SQLite databases found. Start the backend to create "
           "its database, then try again. Configure remote databases using "
           "Database → New Connection."));
    return;
  }
  QString path = files.first();
  if (files.size() > 1) {
    QStringList labels;
    for (const QString &file : files)
      labels << QDir(project.root).relativeFilePath(file);
    QInputDialog dialog(this);
    dialog.setObjectName("backendDatabasePicker");
    dialog.setWindowTitle(tr("Project Database"));
    dialog.setLabelText(tr("Open an existing database:"));
    dialog.setComboBoxItems(labels);
    dialog.setComboBoxEditable(false);
    dialog.setTextValue(labels.first());
    PanelTypography::apply(&dialog, settings.mainFont);
    dialog.setMinimumWidth(480);
    if (dialog.exec() != QDialog::Accepted)
      return;
    const int selected = labels.indexOf(dialog.textValue());
    if (selected < 0)
      return;
    path = files.at(selected);
  }
  showDatabasePanel();
  databasePanel->openSqliteFile(path);
}

void MainWindow::configureBackend() {
  if (m_projectRootPath.isEmpty())
    return;
  const QString path =
      QDir(m_projectRootPath).filePath(".lightpad/backend.json");
  if (!QFileInfo::exists(path)) {
    const auto project = PythonBackendProject::discover(m_projectRootPath);
    QJsonObject settings;
    settings["framework"] =
        project.framework.isEmpty() ? "fastapi" : project.framework;
    settings["application"] =
        project.application.isEmpty() ? "main:app" : project.application;
    if (!project.entryFile.isEmpty())
      settings["entryFile"] =
          QDir(project.root).relativeFilePath(project.entryFile);
    settings["workingDirectory"] = ".";
    settings["sourceDirectory"] =
        project.sourceDirectory.isEmpty()
            ? "."
            : QDir(project.root).relativeFilePath(project.sourceDirectory);
    settings["envFile"] = ".env";
    settings["port"] = project.port > 0 ? project.port : 8000;
    QDir(m_projectRootPath).mkpath(".lightpad");
    QSaveFile file(path);
    const QByteArray data =
        QJsonDocument(settings).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() ||
        !file.commit()) {
      ThemedMessageBox::warning(this, tr("Backend"),
                                tr("Could not write %1").arg(path));
      return;
    }
  }
  openFileAndAddToNewTab(path);
}
