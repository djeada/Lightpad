// Starting programs: the shared Run/Debug target, the launch menus and
// buttons, the configurations dialog, and the run (terminal) and debug
// (DAP session) paths for the current file, saved configurations and
// compounds.
#include "mainwindow_internal.h"

#include "../core/pathvariables.h"
#include "../run_templates/launchtemplatecatalog.h"
#include "dialogs/launchconfigurationsdialog.h"

#include <QScopeGuard>

namespace {
const QString kActiveLaunchTargetKey = QStringLiteral("activeLaunchTarget");
const QString kActiveLaunchTargetProjectKey =
    QStringLiteral("activeLaunchTargetProject");
const QString kLegacyDebugTargetKey = QStringLiteral("activeDebugTarget");
const QString kLegacyDebugTargetProjectKey =
    QStringLiteral("activeDebugTargetProject");
const QString kPickProcess = QStringLiteral("${command:pickProcess}");
const char *kLaunchChainName = "lightpadLaunchChain";

QPair<QString, QStringList> shellInvocation(const QString &commandLine) {
#ifdef Q_OS_WIN
  return {QStringLiteral("cmd"), {QStringLiteral("/C"), commandLine}};
#else
  return {QStringLiteral("bash"), {QStringLiteral("-lc"), commandLine}};
#endif
}

QString shellJoin(const QStringList &command) {
  QStringList quoted;
  for (const QString &part : command) {
    quoted << PythonProjectEnvironment::shellQuote(part);
  }
  return quoted.join(QLatin1Char(' '));
}

// QMenu::addSection draws only a line in Lightpad's styles; a disabled bold
// row reads as a heading in every theme.
void addMenuHeading(QMenu *menu, const QString &text) {
  menu->addSeparator();
  QAction *heading = menu->addAction(text);
  heading->setEnabled(false);
  QFont font = heading->font();
  font.setBold(true);
  font.setPointSizeF(font.pointSizeF() * 0.9);
  heading->setFont(font);
}

QString htmlRow(const QString &caption, const QString &value) {
  return QStringLiteral("<b>%1</b> %2")
      .arg(caption.toHtmlEscaped(), value.toHtmlEscaped());
}

bool usesCurrentFile(const DebugConfiguration &config) {
  if (PathVariables::needsFile(config.program) ||
      PathVariables::needsFile(config.cwd) ||
      PathVariables::needsFile(config.preLaunchTask)) {
    return true;
  }
  for (const QString &arg : config.args) {
    if (PathVariables::needsFile(arg)) {
      return true;
    }
  }
  return PathVariables::needsFile(QString::fromUtf8(
      QJsonDocument(config.adapterConfig).toJson(QJsonDocument::Compact)));
}
} // namespace

// ------------------------------------------------------- target selection --

LaunchTarget MainWindow::activeLaunchTarget() const {
  const SettingsManager &settings = SettingsManager::instance();
  QString key = settings.getValue(kActiveLaunchTargetKey, QString()).toString();
  QString owner =
      settings.getValue(kActiveLaunchTargetProjectKey, QString()).toString();
  if (key.isEmpty() && owner.isEmpty()) {
    // Selections made before Run and Debug shared one target.
    key = settings.getValue(kLegacyDebugTargetKey, QString()).toString();
    owner =
        settings.getValue(kLegacyDebugTargetProjectKey, QString()).toString();
  }
  if (key.trimmed().isEmpty() ||
      QDir::cleanPath(owner) != QDir::cleanPath(m_projectRootPath)) {
    return LaunchTarget::currentFile();
  }
  const LaunchTarget target = LaunchTarget::fromKey(key);
  const DebugConfigurationManager &configs =
      DebugConfigurationManager::instance();
  if ((target.kind == LaunchTarget::Kind::Configuration &&
       !configs.hasConfiguration(target.name)) ||
      (target.kind == LaunchTarget::Kind::Compound &&
       !configs.hasCompoundConfiguration(target.name))) {
    return LaunchTarget::currentFile();
  }
  return target;
}

void MainWindow::setActiveLaunchTarget(const LaunchTarget &target) {
  SettingsManager &settings = SettingsManager::instance();
  settings.setValue(kActiveLaunchTargetKey, target.toKey());
  settings.setValue(kActiveLaunchTargetProjectKey,
                    target.isCurrentFile() ? QString() : m_projectRootPath);
  settings.setValue(kLegacyDebugTargetKey, QString());
  settings.setValue(kLegacyDebugTargetProjectKey, QString());
  settings.saveSettings();
  refreshLaunchButtons();
}

QString MainWindow::runSourceFilePath() const {
  if (!m_pinnedRunFilePath.isEmpty() &&
      QFileInfo::exists(m_pinnedRunFilePath) &&
      (m_projectRootPath.isEmpty() ||
       FileDirTreeModel::isInside(m_projectRootPath, m_pinnedRunFilePath))) {
    return m_pinnedRunFilePath;
  }
  return activeEditorFilePath();
}

QString MainWindow::effectivePreferredCMakeTarget() const {
  if (m_preferredCMakeTarget.isEmpty() ||
      m_preferredCMakeTargetRoot.isEmpty() || m_projectRootPath.isEmpty()) {
    return m_preferredCMakeTarget;
  }
  return QDir::cleanPath(m_preferredCMakeTargetRoot) ==
                 QDir::cleanPath(m_projectRootPath)
             ? m_preferredCMakeTarget
             : QString();
}

void MainWindow::invalidateRunTargetCache() {
  m_cachedRunTargetKey.clear();
  m_cachedRunTarget = RunTarget();
}

RunTarget MainWindow::resolveRunTargetForFile(const QString &filePath) {
  const QString key = filePath + QLatin1Char('\n') + m_projectRootPath +
                      QLatin1Char('\n') + effectivePreferredCMakeTarget();
  if (!m_cachedRunTargetKey.isEmpty() && m_cachedRunTargetKey == key) {
    return m_cachedRunTarget;
  }

  RunTargetContext context;
  context.filePath = filePath;
  context.projectRoot = m_projectRootPath;
  context.preferredCMakeTarget = effectivePreferredCMakeTarget();
  if (!filePath.isEmpty()) {
    context.languageId = effectiveLanguageIdForFile(filePath);
  }

  m_cachedRunTarget = RunTargetResolver::resolve(context);
  m_cachedRunTargetKey = key;
  return m_cachedRunTarget;
}

RunTarget MainWindow::currentRunTarget() {
  return resolveRunTargetForFile(runSourceFilePath());
}

void MainWindow::setPinnedRunFilePath(const QString &filePath) {
  const QString normalized =
      filePath.isEmpty() ? QString() : QFileInfo(filePath).absoluteFilePath();
  if (m_pinnedRunFilePath == normalized) {
    return;
  }
  m_pinnedRunFilePath = normalized;
  invalidateRunTargetCache();
  SettingsManager::instance().setValue("pinnedRunTarget", m_pinnedRunFilePath);
  SettingsManager::instance().saveSettings();
  refreshLaunchButtons();
}

void MainWindow::setPreferredCMakeTarget(const QString &targetName) {
  const QString trimmed = targetName.trimmed();
  if (m_preferredCMakeTarget == trimmed) {
    return;
  }
  m_preferredCMakeTarget = trimmed;
  m_preferredCMakeTargetRoot =
      trimmed.isEmpty() ? QString() : m_projectRootPath;
  invalidateRunTargetCache();
  SettingsManager::instance().setValue("preferredCMakeTarget",
                                       m_preferredCMakeTarget);
  SettingsManager::instance().setValue("preferredCMakeTargetRoot",
                                       m_preferredCMakeTargetRoot);
  SettingsManager::instance().saveSettings();
  refreshLaunchButtons();
}

// ------------------------------------------------------ buttons and menus --

void MainWindow::refreshLaunchButtons() {
  if (!ui->runButton || !ui->debugButton) {
    return;
  }
  const LaunchTarget target = activeLaunchTarget();
  DebugConfigurationManager &configs = DebugConfigurationManager::instance();

  QString label;
  QStringList runLines;
  QStringList debugLines;
  QString runStatus;
  switch (target.kind) {
  case LaunchTarget::Kind::CurrentFile: {
    const QString sourcePath = runSourceFilePath();
    const RunTarget fileTarget = resolveRunTargetForFile(sourcePath);
    const bool pinned = !m_pinnedRunFilePath.isEmpty();
    if (!fileTarget.isValid()) {
      label =
          sourcePath.isEmpty() ? tr("Run") : QFileInfo(sourcePath).fileName();
      runLines << QStringLiteral("<b>%1</b>")
                      .arg(tr("Nothing to run").toHtmlEscaped())
               << fileTarget.unavailableReason.toHtmlEscaped();
      runStatus = fileTarget.unavailableReason;
    } else {
      label = fileTarget.needsTargetChoice ? tr("Choose target…")
                                           : fileTarget.displayName();
      runLines << htmlRow(tr("Runs:"),
                          QDir::toNativeSeparators(fileTarget.targetPath()));
      const QString commandLine = fileTarget.commandLine();
      if (!commandLine.isEmpty() && commandLine != fileTarget.targetPath()) {
        runLines << htmlRow(tr("Command:"), commandLine);
      }
      if (!fileTarget.workingDirectory.isEmpty()) {
        runLines << htmlRow(tr("Directory:"), QDir::toNativeSeparators(
                                                  fileTarget.workingDirectory));
      }
      if (!fileTarget.rationale().isEmpty()) {
        runLines << fileTarget.rationale().toHtmlEscaped();
      }
      runStatus =
          tr("Run %1").arg(QDir::toNativeSeparators(fileTarget.targetPath()));
    }
    if (pinned) {
      label = tr("%1 (pinned)").arg(label);
      runLines << tr("Pinned, so switching tabs will not change this.")
                      .toHtmlEscaped();
    }
    QString reason;
    const QString debugger =
        LaunchPlanner::fileDebuggerName(fileTarget, &reason);
    debugLines << (debugger.isEmpty() ? reason.toHtmlEscaped()
                                      : htmlRow(tr("Debugger:"), debugger));
    if (!debugger.isEmpty()) {
      debugLines << tr("Uses the same arguments, environment and working "
                       "directory as Run.")
                        .toHtmlEscaped();
    }
    break;
  }
  case LaunchTarget::Kind::Configuration: {
    const DebugConfiguration config = configs.configuration(target.name);
    label = config.name;
    const DebugConfiguration resolved =
        configs.resolveVariables(config, activeEditorFilePath());
    const RunCommand command = LaunchPlanner::runCommandFor(resolved);
    runLines << htmlRow(tr("Configuration:"), config.name);
    if (command.isValid()) {
      runLines << htmlRow(
          tr("Command:"),
          LaunchPlanner::commandLine(command.program, command.arguments));
      if (!resolved.cwd.isEmpty()) {
        runLines << htmlRow(tr("Directory:"),
                            QDir::toNativeSeparators(resolved.cwd));
      }
    } else {
      runLines << command.error.toHtmlEscaped();
    }
    if (!resolved.preLaunchTask.isEmpty()) {
      runLines << htmlRow(tr("Before launch:"), resolved.preLaunchTask);
    }
    runStatus = tr("Run configuration %1").arg(config.name);
    const QString debugger = config.type == QLatin1String("command")
                                 ? QString()
                                 : LaunchPlanner::debuggerNameFor(config);
    debugLines << (debugger.isEmpty()
                       ? tr("This configuration has no debugger; it can "
                            "only run.")
                             .toHtmlEscaped()
                       : htmlRow(tr("Debugger:"), debugger));
    break;
  }
  case LaunchTarget::Kind::Compound: {
    const CompoundDebugConfiguration compound =
        configs.compoundConfiguration(target.name);
    label = compound.name;
    runLines << htmlRow(tr("Compound:"), compound.name)
             << htmlRow(tr("Starts:"),
                        compound.configurations.join(QStringLiteral(", ")));
    debugLines << runLines;
    runStatus = tr("Run compound %1").arg(compound.name);
    break;
  }
  }

  const auto tooltip = [](const QString &title, const QStringList &lines) {
    return QStringLiteral("<div style='white-space:pre-wrap;'><b>%1</b><br>%2"
                          "</div>")
        .arg(title.toHtmlEscaped(), lines.join(QStringLiteral("<br>")));
  };
  const QString runTip = tooltip(tr("Run %1 (Ctrl+F5)").arg(label), runLines);
  const QString debugTip = tooltip(tr("Debug %1 (F5)").arg(label), debugLines);

  ui->runButton->setText(label);
  ui->runButton->setToolTip(runTip);
  ui->runButton->setStatusTip(runStatus);
  ui->runButton->setAccessibleName(tr("Run %1").arg(label));
  ui->debugButton->setText(tr("Debug"));
  ui->debugButton->setToolTip(debugTip);
  ui->debugButton->setStatusTip(tr("Debug %1").arg(label));
  ui->debugButton->setAccessibleName(tr("Debug %1").arg(label));
  if (ui->actionRun_file_name) {
    ui->actionRun_file_name->setText(tr("Run %1").arg(label));
    ui->actionRun_file_name->setToolTip(runTip);
  }
  if (ui->actionDebug_file_name) {
    ui->actionDebug_file_name->setText(tr("Debug %1").arg(label) +
                                       QLatin1String("\tF5"));
  }
}

void MainWindow::populateLaunchMenu(QMenu *menu) {
  if (!menu) {
    return;
  }
  menu->clear();
  const LaunchTarget active = activeLaunchTarget();
  DebugConfigurationManager &configs = DebugConfigurationManager::instance();
  const QString label = ui->runButton ? ui->runButton->text() : QString();

  QAction *run =
      menu->addAction(ui->runButton->icon(),
                      tr("Run %1").arg(label) + QLatin1String("\tCtrl+F5"));
  connect(run, &QAction::triggered, this,
          [this]() { launchActiveTarget(LaunchMode::Run); });
  QAction *debug =
      menu->addAction(ui->debugButton->icon(),
                      tr("Debug %1").arg(label) + QLatin1String("\tF5"));
  connect(debug, &QAction::triggered, this,
          [this]() { launchActiveTarget(LaunchMode::Debug); });

  QString commandLine;
  if (active.isCurrentFile()) {
    commandLine = resolveRunTargetForFile(runSourceFilePath()).commandLine();
  } else if (active.kind == LaunchTarget::Kind::Configuration) {
    const RunCommand command =
        LaunchPlanner::runCommandFor(configs.resolveVariables(
            configs.configuration(active.name), activeEditorFilePath()));
    commandLine =
        LaunchPlanner::commandLine(command.program, command.arguments);
  }
  if (!commandLine.isEmpty()) {
    QAction *copy = menu->addAction(tr("Copy Command Line"));
    connect(copy, &QAction::triggered, this, [commandLine]() {
      QGuiApplication::clipboard()->setText(commandLine);
    });
  }

  auto *targets = new QActionGroup(menu);
  targets->setExclusive(true);
  auto addTarget = [this, menu, targets, active](const QString &text,
                                                 const LaunchTarget &target,
                                                 const QString &toolTip) {
    QAction *action = menu->addAction(text);
    action->setCheckable(true);
    action->setChecked(target == active);
    action->setToolTip(toolTip);
    targets->addAction(action);
    connect(action, &QAction::triggered, this,
            [this, target]() { setActiveLaunchTarget(target); });
    return action;
  };

  addMenuHeading(menu, tr("Current File"));
  const QString sourcePath = runSourceFilePath();
  const RunTarget fileTarget = resolveRunTargetForFile(sourcePath);
  QString fileText = sourcePath.isEmpty() ? tr("The file open in the editor")
                                          : QFileInfo(sourcePath).fileName();
  if (fileTarget.isValid() && !fileTarget.templateName.isEmpty() &&
      fileTarget.kind != RunTarget::Kind::CMakeExecutable) {
    fileText = tr("%1 · %2").arg(fileText, fileTarget.templateName);
  } else if (fileTarget.kind == RunTarget::Kind::CMakeExecutable &&
             !fileTarget.cmakeTargetName.isEmpty()) {
    fileText =
        tr("%1 · CMake target %2")
            .arg(QFileInfo(sourcePath).fileName(), fileTarget.cmakeTargetName);
  }
  addTarget(fileText, LaunchTarget::currentFile(),
            fileTarget.isValid() ? fileTarget.rationale()
                                 : fileTarget.unavailableReason);
  addRunTemplateActions(menu, sourcePath);
  addCMakeTargetActions(menu, sourcePath);

  const QString activePath = activeEditorFilePath();
  if (!m_pinnedRunFilePath.isEmpty() || !activePath.isEmpty()) {
    const bool pinned = !m_pinnedRunFilePath.isEmpty();
    QAction *pin = menu->addAction(
        tr("Always Run %1")
            .arg(QFileInfo(pinned ? m_pinnedRunFilePath : activePath)
                     .fileName()));
    pin->setCheckable(true);
    pin->setChecked(pinned);
    pin->setToolTip(tr("Keep this file as the Current File target while you "
                       "switch tabs"));
    connect(pin, &QAction::triggered, this, [this, activePath](bool checked) {
      setPinnedRunFilePath(checked ? activePath : QString());
    });
  }

  const QList<DebugConfiguration> saved = configs.allConfigurations();
  const QList<CompoundDebugConfiguration> compounds =
      configs.allCompoundConfigurations();
  if (!saved.isEmpty() || !compounds.isEmpty()) {
    addMenuHeading(menu, tr("Saved Configurations"));
    for (const DebugConfiguration &config : saved) {
      const bool runOnly = config.type == QLatin1String("command");
      addTarget(runOnly ? tr("%1  (run only)").arg(config.name) : config.name,
                LaunchTarget::configuration(config.name),
                runOnly ? tr("Runs without a debugger")
                        : LaunchPlanner::debuggerNameFor(config));
    }
    for (const CompoundDebugConfiguration &compound : compounds) {
      addTarget(tr("%1  (compound)").arg(compound.name),
                LaunchTarget::compound(compound.name),
                compound.configurations.join(QStringLiteral(", ")));
    }
  }

  menu->addSeparator();
  QMenu *add = menu->addMenu(tr("Add Configuration"));
  const QString languageId =
      sourcePath.isEmpty() ? QString() : effectiveLanguageIdForFile(sourcePath);
  const QList<LaunchTemplate> suggestions =
      LaunchTemplateCatalog::instance().suggestionsFor(languageId,
                                                       m_projectRootPath);
  for (int i = 0; i < suggestions.size() && i < 6; ++i) {
    const LaunchTemplate &entry = suggestions.at(i);
    QAction *action = add->addAction(entry.name);
    action->setToolTip(entry.description);
    connect(action, &QAction::triggered, this, [this, id = entry.id]() {
      addLaunchConfigurationFromTemplate(id);
    });
  }
  if (!suggestions.isEmpty()) {
    add->addSeparator();
  }
  QAction *more = add->addAction(tr("All Templates…"));
  connect(more, &QAction::triggered, this,
          [this]() { addLaunchConfigurationFromTemplate(QString()); });
  add->setEnabled(!m_projectRootPath.isEmpty() || !sourcePath.isEmpty());

  QAction *edit = menu->addAction(tr("Edit Configurations…"));
  connect(edit, &QAction::triggered, this, [this]() {
    openLaunchConfigurations(runSourceFilePath(), activeLaunchTarget());
  });
}

void MainWindow::addCMakeTargetActions(QMenu *menu, const QString &filePath) {
  if (!menu || filePath.isEmpty()) {
    return;
  }

  const QString cmakeRoot =
      RunTargetResolver::cmakeRootFor(filePath, m_projectRootPath);
  if (cmakeRoot.isEmpty()) {
    return;
  }

  CMakeProject project;
  const QList<CMakeTargetInfo> executables =
      project.parseExecutableTargets(cmakeRoot);
  if (executables.size() < 2) {
    return;
  }

  QMenu *targetMenu = menu->addMenu(tr("CMake Target"));
  auto *group = new QActionGroup(targetMenu);
  QAction *autoAction =
      targetMenu->addAction(tr("Automatic (the target that owns the file)"));
  autoAction->setCheckable(true);
  autoAction->setChecked(effectivePreferredCMakeTarget().isEmpty());
  group->addAction(autoAction);
  connect(autoAction, &QAction::triggered, this,
          [this]() { setPreferredCMakeTarget(QString()); });

  targetMenu->addSeparator();
  for (const CMakeTargetInfo &executable : executables) {
    QAction *action = targetMenu->addAction(executable.name);
    action->setCheckable(true);
    action->setChecked(executable.name == effectivePreferredCMakeTarget());
    group->addAction(action);
    connect(
        action, &QAction::triggered, this,
        [this, name = executable.name]() { setPreferredCMakeTarget(name); });
  }
}

void MainWindow::addRunTemplateActions(QMenu *menu, const QString &filePath) {
  if (!menu || filePath.isEmpty()) {
    return;
  }

  RunTemplateManager &manager = RunTemplateManager::instance();
  if (manager.getAllTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  QList<RunTemplate> candidates = manager.getTemplatesForFilePath(filePath);
  const QString languageId = effectiveLanguageIdForFile(filePath);
  for (const RunTemplate &languageTemplate :
       manager.getTemplatesForLanguageId(languageId)) {
    bool alreadyListed = false;
    for (const RunTemplate &candidate : candidates) {
      alreadyListed = alreadyListed || candidate.id == languageTemplate.id;
    }
    if (!alreadyListed) {
      candidates.append(languageTemplate);
    }
  }
  if (candidates.isEmpty()) {
    return;
  }

  const QString assigned =
      manager.getAssignmentForFile(filePath).templateId.trimmed();
  QMenu *templateMenu = menu->addMenu(tr("Run With"));
  auto *group = new QActionGroup(templateMenu);
  auto assign = [this, filePath](const QString &id) {
    RunTemplateManager &templates = RunTemplateManager::instance();
    FileTemplateAssignment assignment =
        templates.getAssignmentForFile(filePath);
    assignment.filePath = filePath;
    assignment.templateId = id;
    if (assignment.isEmpty()) {
      templates.removeAssignment(filePath);
    } else {
      templates.assignTemplateToFile(filePath, assignment);
    }
    setActiveLaunchTarget(LaunchTarget::currentFile());
  };

  const RunTemplate automatic = manager.getTemplateById(
      manager.automaticTemplateIdForFile(filePath, languageId));
  QAction *automaticAction = templateMenu->addAction(
      automatic.isValid() ? tr("Automatic (%1)").arg(automatic.name)
                          : tr("Automatic"));
  automaticAction->setCheckable(true);
  automaticAction->setChecked(assigned.isEmpty());
  group->addAction(automaticAction);
  connect(automaticAction, &QAction::triggered, this,
          [assign]() { assign(QString()); });
  templateMenu->addSeparator();

  for (const RunTemplate &candidate : candidates) {
    QAction *action = templateMenu->addAction(
        candidate.isDebuggable() ? candidate.name
                                 : tr("%1  (run only)").arg(candidate.name));
    action->setCheckable(true);
    action->setChecked(candidate.id == assigned);
    action->setToolTip(candidate.description);
    group->addAction(action);
    connect(action, &QAction::triggered, this,
            [assign, id = candidate.id]() { assign(id); });
  }
  templateMenu->addSeparator();
  QAction *configure =
      templateMenu->addAction(tr("Arguments, Environment and More…"));
  connect(configure, &QAction::triggered, this, [this, filePath]() {
    openLaunchConfigurations(filePath, LaunchTarget::currentFile());
  });
}

// ------------------------------------------------- configurations dialog --

void MainWindow::openLaunchConfigurations(const QString &filePath,
                                          const LaunchTarget &selection) {
  if (auto *existing = findChild<LaunchConfigurationsDialog *>()) {
    existing->raise();
    existing->activateWindow();
    return;
  }
  if (!filePath.isEmpty()) {
    ensureProjectRootForPath(filePath);
  }
  if (m_projectRootPath.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Run and Debug Configurations"),
        tr("Open a file or a project folder first; configurations are saved "
           "with the project."));
    return;
  }
  DebugConfigurationManager &configs = DebugConfigurationManager::instance();
  configs.setWorkspaceFolder(m_projectRootPath);
  configs.loadFromLightpadDir();
  RunTemplateManager::instance().setWorkspaceFolder(m_projectRootPath);

  LaunchConfigurationsDialog::Context context;
  context.workspaceFolder = m_projectRootPath;
  context.currentFilePath = filePath;
  context.languageId =
      filePath.isEmpty() ? QString() : effectiveLanguageIdForFile(filePath);
  context.preferredCMakeTarget = effectivePreferredCMakeTarget();
  context.selection = selection;

  auto *dialog = new LaunchConfigurationsDialog(context, this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->applyTheme(getTheme());
  connect(dialog, &QDialog::accepted, this, [this, dialog]() {
    const LaunchTarget target = dialog->selectedTarget();
    const std::optional<LaunchMode> mode = dialog->requestedLaunch();
    invalidateRunTargetCache();
    setActiveLaunchTarget(target);
    if (mode) {
      QTimer::singleShot(
          0, this, [this, target, mode]() { launchTarget(target, *mode); });
    }
  });
  connect(dialog, &QObject::destroyed, this, [this]() {
    invalidateRunTargetCache();
    refreshLaunchButtons();
  });
  dialog->show();
}

void MainWindow::addLaunchConfigurationFromTemplate(const QString &templateId) {
  const QString filePath = runSourceFilePath();
  openLaunchConfigurations(filePath, activeLaunchTarget());
  auto *dialog = findChild<LaunchConfigurationsDialog *>();
  if (!dialog) {
    return;
  }
  if (templateId.isEmpty()) {
    QMetaObject::invokeMethod(
        dialog, [dialog]() { dialog->openTemplateGallery(); },
        Qt::QueuedConnection);
  } else {
    dialog->addConfigurationFromTemplate(templateId);
  }
}

void MainWindow::openRunConfigurationForFile(const QString &filePath) {
  if (filePath.isEmpty()) {
    ThemedMessageBox::information(
        this, tr("Run and Debug Configurations"),
        tr("Open a file first to choose how it runs."));
    return;
  }
  openLaunchConfigurations(filePath, LaunchTarget::currentFile());
}

void MainWindow::openConfigurationDialog() {
  openLaunchConfigurations(runSourceFilePath(), activeLaunchTarget());
}

void MainWindow::openDebugConfigurationDialog() {
  openLaunchConfigurations(runSourceFilePath(), activeLaunchTarget());
}

// ----------------------------------------------------------- entry points --

void MainWindow::launchActiveTarget(LaunchMode mode) {
  launchTarget(activeLaunchTarget(), mode);
}

void MainWindow::launchTarget(const LaunchTarget &target, LaunchMode mode) {
  switch (target.kind) {
  case LaunchTarget::Kind::CurrentFile:
    if (mode == LaunchMode::Run) {
      runPath(runSourceFilePath());
    } else {
      startDebuggingForCurrentFile();
    }
    return;
  case LaunchTarget::Kind::Configuration:
    if (mode == LaunchMode::Run) {
      runConfiguration(target.name);
    } else {
      debugConfiguration(target.name);
    }
    return;
  case LaunchTarget::Kind::Compound:
    launchCompound(target.name, mode);
    return;
  }
}

void MainWindow::runCurrentScript() { launchActiveTarget(LaunchMode::Run); }

void MainWindow::on_runButton_clicked() { launchActiveTarget(LaunchMode::Run); }

void MainWindow::on_debugButton_clicked() {
  launchActiveTarget(LaunchMode::Debug);
}

void MainWindow::on_actionRun_file_name_triggered() {
  launchActiveTarget(LaunchMode::Run);
}

void MainWindow::on_actionDebug_file_name_triggered() {
  launchActiveTarget(LaunchMode::Debug);
}

void MainWindow::on_actionStart_Debug_Configuration_triggered() {
  // Pick a target from the same menu the buttons use.
  if (!m_debugTargetMenu) {
    return;
  }
  QWidget *anchor = ui->debugButton && ui->debugButton->isVisible()
                        ? static_cast<QWidget *>(ui->debugButton)
                        : static_cast<QWidget *>(this);
  m_debugTargetMenu->popup(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

void MainWindow::on_actionEdit_Configurations_triggered() {
  openLaunchConfigurations(runSourceFilePath(), activeLaunchTarget());
}

void MainWindow::on_actionEdit_Debug_Configurations_triggered() {
  openLaunchConfigurations(runSourceFilePath(), activeLaunchTarget());
}

void MainWindow::runFileByPath(const QString &filePath) {
  if (filePath.isEmpty()) {
    return;
  }
  openFileAndAddToNewTab(filePath);
  runPath(filePath);
}

void MainWindow::debugFileByPath(const QString &filePath) {
  if (filePath.isEmpty()) {
    return;
  }
  openFileAndAddToNewTab(filePath);
  startDebuggingForFile(filePath);
}

// ------------------------------------------------------------ preparation --

void MainWindow::saveModifiedFilesBeforeLaunch() {
  // Builds and interpreters read files from disk, so unsaved edits in any
  // tab would be ignored.
  for (LightpadTabWidget *tabWidget : allTabWidgets()) {
    for (int i = 0; tabWidget && i < tabWidget->count(); ++i) {
      LightpadPage *page = tabWidget->getPage(i);
      const QString filePath = tabWidget->getFilePath(i);
      if (page && page->getTextArea() && !filePath.isEmpty() &&
          page->getTextArea()->changesUnsaved()) {
        save(filePath);
      }
    }
  }
}

void MainWindow::prepareLaunchWorkspace(const QString &filePath) {
  saveModifiedFilesBeforeLaunch();
  if (!filePath.isEmpty()) {
    ensureProjectRootForPath(filePath);
  }
  if (m_projectRootPath.isEmpty()) {
    return;
  }
  DebugSettings::instance().initialize(m_projectRootPath);
  DebugConfigurationManager &configs = DebugConfigurationManager::instance();
  configs.setWorkspaceFolder(m_projectRootPath);
  configs.loadFromLightpadDir();
  RunTemplateManager::instance().setWorkspaceFolder(m_projectRootPath);
  BreakpointManager::instance().setWorkspaceFolder(m_projectRootPath);
  if (BreakpointManager::instance().allBreakpoints().isEmpty()) {
    BreakpointManager::instance().loadFromLightpadDir();
  }
  WatchManager::instance().setWorkspaceFolder(m_projectRootPath);
  if (WatchManager::instance().allWatches().isEmpty()) {
    WatchManager::instance().loadFromLightpadDir();
  }
}

void MainWindow::showLaunchProblem(const QString &title, const QString &text,
                                   const QString &details,
                                   const LaunchTarget &configure,
                                   const QString &configureFile) {
  ThemedMessageBox message(this);
  message.setIcon(ThemedMessageBox::Warning);
  message.setWindowTitle(title);
  message.setText(text);
  if (!details.isEmpty()) {
    message.setInformativeText(details);
  }
  const bool canConfigure =
      !m_projectRootPath.isEmpty() || !configureFile.isEmpty();
  if (canConfigure) {
    message.setStandardButtons(ThemedMessageBox::Ok | ThemedMessageBox::Cancel);
    message.setButtonText(ThemedMessageBox::Ok, tr("Edit Configuration…"));
    message.setButtonText(ThemedMessageBox::Cancel, tr("Close"));
    message.setDefaultButton(ThemedMessageBox::Cancel);
  }
  if (message.exec() == ThemedMessageBox::Ok && canConfigure) {
    openLaunchConfigurations(configureFile, configure);
  }
}

void MainWindow::noScriptAssignedWarning() {
  ThemedMessageBox msgBox(this);
  msgBox.setWindowTitle(tr("Nothing to Launch"));
  msgBox.setText(tr("No file is open."));
  msgBox.setInformativeText(
      tr("Open a file to run or debug it, or add a saved configuration from "
         "Run > Edit Configurations."));
  msgBox.setStandardButtons(ThemedMessageBox::Ok | ThemedMessageBox::Cancel);
  msgBox.setButtonText(ThemedMessageBox::Ok, tr("Open File…"));
  if (msgBox.exec() == ThemedMessageBox::Ok) {
    on_actionOpen_File_triggered();
  }
}

// -------------------------------------------------------------- terminals --

Terminal *MainWindow::launchTerminal(const QString &title,
                                     const QString &workingDirectory,
                                     bool dedicated) {
  TerminalTabWidget *tabs = ensureTerminalWidget();
  showTerminalPanel();
  if (dedicated) {
    QPointer<Terminal> &existing = m_launchTerminals[title];
    if (!existing) {
      existing = tabs->addNewTerminal(workingDirectory, title);
    }
    tabs->setCurrentTerminal(existing);
    return existing;
  }

  // Do not take over a terminal that runs a server or a saved configuration.
  Terminal *current = tabs->currentTerminal();
  bool busy = current && current->hasActiveRunProcess() &&
              current == m_backendTerminal.data();
  for (const QPointer<Terminal> &terminal : std::as_const(m_launchTerminals)) {
    busy = busy || (current && terminal.data() == current &&
                    current->hasActiveRunProcess());
  }
  if (!current || busy) {
    current = tabs->addNewTerminal(workingDirectory, title);
    tabs->setCurrentTerminal(current);
  }
  return current;
}

void MainWindow::startTerminalLaunch(Terminal *terminal,
                                     const TerminalLaunch &launch) {
  if (!terminal) {
    return;
  }
  // A terminal follows one launch at a time; a new one replaces the old.
  delete terminal->findChild<QObject *>(QLatin1String(kLaunchChainName),
                                        Qt::FindDirectChildrenOnly);
  auto *chain = new QObject(terminal);
  chain->setObjectName(QLatin1String(kLaunchChainName));

  enum class Stage { Before, Main, After };
  auto stage = std::make_shared<Stage>(launch.before.isEmpty() ? Stage::Main
                                                               : Stage::Before);
  QPointer<Terminal> guard(terminal);
  auto runStage = [guard, launch, stage]() {
    if (!guard) {
      return;
    }
    switch (*stage) {
    case Stage::Before: {
      const auto shell = shellInvocation(launch.before);
      guard->executeCommand(shell.first, shell.second, launch.workingDirectory,
                            launch.environment);
      return;
    }
    case Stage::Main:
      guard->executeCommand(launch.program, launch.arguments,
                            launch.workingDirectory, launch.environment);
      return;
    case Stage::After: {
      const auto shell = shellInvocation(launch.after);
      guard->executeCommand(shell.first, shell.second, launch.workingDirectory,
                            launch.environment);
      return;
    }
    }
  };

  connect(terminal, &Terminal::processFinished, chain,
          [this, guard, chain, stage, launch, runStage](int exitCode) {
            if (*stage != Stage::After) {
              if (exitCode != 0 && guard) {
                publishBuildDiagnostics(guard->runTranscript(),
                                        launch.workingDirectory);
              } else if (m_diagnosticsManager) {
                m_diagnosticsManager->clearAllForSource(
                    kBuildDiagnosticsSource);
              }
            }
            if (*stage == Stage::Before) {
              if (exitCode != 0) {
                if (guard) {
                  guard->appendNotice(
                      tr("\nThe before-launch command failed (exit code %1), "
                         "so %2 was not started.")
                          .arg(exitCode)
                          .arg(launch.title),
                      true);
                }
                chain->deleteLater();
                return;
              }
              *stage = Stage::Main;
              runStage();
              return;
            }
            if (*stage == Stage::Main && !launch.after.isEmpty()) {
              *stage = Stage::After;
              runStage();
              return;
            }
            chain->deleteLater();
          });
  connect(terminal, &Terminal::processError, chain,
          [chain](const QString &) { chain->deleteLater(); });

  statusBar()->showMessage(tr("Running %1").arg(launch.title), 8000);
  runStage();
}

void MainWindow::stopFollowingLaunches() {
  if (!terminalWidget) {
    return;
  }
  for (Terminal *terminal : terminalWidget->findChildren<Terminal *>()) {
    qDeleteAll(terminal->findChildren<QObject *>(
        QLatin1String(kLaunchChainName), Qt::FindDirectChildrenOnly));
  }
}

// ------------------------------------------------------- run current file --

void MainWindow::runPath(const QString &filePath) {
  prepareLaunchWorkspace(filePath);
  invalidateRunTargetCache();
  executeRunTarget(resolveRunTargetForFile(filePath));
}

void MainWindow::runStructuredTestTarget(const RunTarget &target) {
  ensureTestPanel();
  if (!testPanel || !testDock) {
    return;
  }

  if (!m_projectRootPath.isEmpty()) {
    testPanel->setWorkspaceFolder(m_projectRootPath);
  }

  const QString configId = target.kind == RunTarget::Kind::Pytest
                               ? QStringLiteral("pytest")
                           : target.templateId == QLatin1String("cpp_make_test")
                               ? QStringLiteral("gtest_make")
                               : QStringLiteral("gtest_cmake");
  const QString testFile = target.templateId == "python_pytest_workspace"
                               ? QString()
                               : target.filePath;
  bool launched = testPanel->runWithConfigurationId(configId, testFile);
  if (!launched && target.kind != RunTarget::Kind::Pytest) {
    launched = testPanel->runWithConfigurationId(QStringLiteral("gtest_cmake"),
                                                 target.filePath);
  }
  if (!launched) {
    testPanel->runCurrentFile(target.filePath);
  }

  testDock->show();
  testDock->raise();
  syncViewToggleActionStates();
}

void MainWindow::executeRunTarget(const RunTarget &target) {
  if (!target.isValid()) {
    if (target.filePath.isEmpty()) {
      noScriptAssignedWarning();
      return;
    }
    showLaunchProblem(tr("Run"), tr("Nothing to run."),
                      target.unavailableReason, LaunchTarget::currentFile(),
                      target.filePath);
    return;
  }

  if (target.kind == RunTarget::Kind::CTest ||
      target.kind == RunTarget::Kind::Pytest) {
    runStructuredTestTarget(target);
    return;
  }

  RunTarget resolved = target;
  if (resolved.requiresBuild) {
    QString programPath;
    QString chosenTarget;
    QString buildError;
    if (!buildCMakeExecutableForSource(resolved.filePath, resolved.program,
                                       resolved.cmakeBinaryDir, &programPath,
                                       &buildError, &chosenTarget)) {
      showBuildFailure(
          tr("Build Failed"),
          tr("Unable to build a runnable target for this file."),
          tr("The build has to succeed before the program can run."),
          buildError);
      return;
    }
    if (!programPath.isEmpty()) {
      const QString defaultDirectory =
          resolved.program.isEmpty()
              ? resolved.cmakeRoot
              : QFileInfo(resolved.program).absolutePath();
      if (resolved.workingDirectory.isEmpty() ||
          resolved.workingDirectory == defaultDirectory) {
        resolved.workingDirectory = QFileInfo(programPath).absolutePath();
      }
      resolved.program = programPath;
      if (!chosenTarget.isEmpty()) {
        resolved.cmakeTargetName = chosenTarget;
      }
    }
  }

  if (resolved.program.isEmpty()) {
    showTerminalPanel();
    terminalWidget->runFile(resolved.filePath, resolved.languageId);
    return;
  }

  if (resolved.kind == RunTarget::Kind::PythonBackend) {
    // Keep the server alive while lint, tests and other tasks use other tabs.
    if (m_backendTerminal && m_backendTerminal->hasActiveRunProcess() &&
        m_backendTerminalRoot == m_projectRootPath) {
      showTerminalPanel();
      terminalWidget->setCurrentTerminal(m_backendTerminal);
      statusBar()->showMessage(tr("The backend server is already running."),
                               5000);
      return;
    }
    showTerminalPanel();
    m_backendTerminal = terminalWidget->addNewTerminal(
        resolved.workingDirectory, resolved.displayName());
    m_backendTerminalRoot = m_projectRootPath;
    terminalWidget->setCurrentTerminal(m_backendTerminal);
    TerminalLaunch launch;
    launch.title = resolved.displayName();
    launch.program = resolved.program;
    launch.arguments = resolved.arguments;
    launch.workingDirectory = resolved.workingDirectory;
    launch.environment = resolved.environment;
    startTerminalLaunch(m_backendTerminal, launch);
    return;
  }

  const FileTemplateAssignment assignment =
      RunTemplateManager::instance().getAssignmentForFile(resolved.filePath);
  TerminalLaunch launch;
  launch.title = resolved.displayName();
  launch.program = resolved.program;
  launch.arguments = resolved.arguments;
  launch.workingDirectory = resolved.workingDirectory;
  launch.environment = resolved.environment;
  if (!assignment.preRunCommand.trimmed().isEmpty()) {
    launch.before = RunTemplateManager::substituteCommandLineVariables(
        assignment.preRunCommand.trimmed(), resolved.filePath);
  }
  if (!assignment.postRunCommand.trimmed().isEmpty()) {
    launch.after = RunTemplateManager::substituteCommandLineVariables(
        assignment.postRunCommand.trimmed(), resolved.filePath);
  }

  Terminal *terminal =
      launchTerminal(launch.title, launch.workingDirectory, false);
  if (resolved.languageId == QLatin1String("py")) {
    const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
        {assignment.pythonMode, assignment.pythonInterpreter,
         assignment.pythonVenvPath, assignment.pythonRequirementsFile},
        m_projectRootPath, resolved.filePath, resolved.workingDirectory);
    terminal->setPythonEnvironmentBanner(info);
  }
  startTerminalLaunch(terminal, launch);
}

// ------------------------------------------------- run saved configuration --

void MainWindow::runConfiguration(const QString &name) {
  const QString currentFile = activeEditorFilePath();
  prepareLaunchWorkspace(currentFile);
  DebugConfigurationManager &configs = DebugConfigurationManager::instance();
  if (!configs.hasConfiguration(name)) {
    showLaunchProblem(
        tr("Run"), tr("The configuration \"%1\" no longer exists.").arg(name),
        QString(), LaunchTarget::currentFile(), currentFile);
    setActiveLaunchTarget(LaunchTarget::currentFile());
    return;
  }
  const DebugConfiguration config = configs.configuration(name);
  if (currentFile.isEmpty() && usesCurrentFile(config)) {
    showLaunchProblem(
        tr("Run"), tr("\"%1\" needs an open file.").arg(name),
        tr("It refers to ${file} or a related variable. Open the file to run "
           "it with, then try again."),
        LaunchTarget::configuration(name), currentFile);
    return;
  }
  DebugConfiguration resolved = configs.resolveVariables(config, currentFile);

  TerminalLaunch launch;
  launch.title = config.name;
  QStringList before;
  if (!resolved.preLaunchTask.isEmpty()) {
    before << resolved.preLaunchTask;
  }
  // A native configuration whose program is the open source file runs the
  // binary the file's run template builds.
  if (resolved.type == QLatin1String("cppdbg") && !currentFile.isEmpty() &&
      QFileInfo(resolved.program) == QFileInfo(currentFile)) {
    const FileDebugPlan plan =
        LaunchPlanner::debugPlanForFile(resolveRunTargetForFile(currentFile));
    if (!plan.isValid() || plan.requiresCMakeBuild) {
      runPath(currentFile);
      return;
    }
    for (const LaunchBuildStep &step : plan.buildSteps) {
      before << shellJoin(step.command);
    }
    resolved.program = plan.configuration.program;
  }

  const RunCommand command = LaunchPlanner::runCommandFor(resolved);
  if (!command.isValid()) {
    showLaunchProblem(tr("Run"), tr("\"%1\" cannot run.").arg(name),
                      command.error, LaunchTarget::configuration(name),
                      currentFile);
    return;
  }
  launch.program = command.program;
  launch.arguments = command.arguments;
  launch.workingDirectory =
      resolved.cwd.isEmpty() ? m_projectRootPath : resolved.cwd;
  launch.environment = resolved.env;
  for (auto it = command.environment.constBegin();
       it != command.environment.constEnd(); ++it) {
    if (!launch.environment.contains(it.key())) {
      launch.environment.insert(it.key(), it.value());
    }
  }
  launch.before = before.join(QStringLiteral(" && "));
  launch.after = resolved.postDebugTask;
  startTerminalLaunch(
      launchTerminal(config.name, launch.workingDirectory, true), launch);
}

void MainWindow::launchCompound(const QString &name, LaunchMode mode) {
  const QString currentFile = activeEditorFilePath();
  prepareLaunchWorkspace(currentFile);
  DebugConfigurationManager &configs = DebugConfigurationManager::instance();
  const CompoundDebugConfiguration compound =
      configs.compoundConfiguration(name);
  if (compound.name.isEmpty()) {
    showLaunchProblem(tr("Compound"),
                      tr("The compound \"%1\" no longer exists.").arg(name),
                      QString(), LaunchTarget::currentFile(), currentFile);
    setActiveLaunchTarget(LaunchTarget::currentFile());
    return;
  }
  QStringList missing;
  for (const QString &member : compound.configurations) {
    if (!configs.hasConfiguration(member)) {
      missing << member;
    }
  }
  if (compound.configurations.isEmpty() || !missing.isEmpty()) {
    showLaunchProblem(tr("Compound"), tr("\"%1\" cannot start.").arg(name),
                      compound.configurations.isEmpty()
                          ? tr("It does not list any configurations.")
                          : tr("These configurations do not exist: %1")
                                .arg(missing.join(QStringLiteral(", "))),
                      LaunchTarget::compound(name), currentFile);
    return;
  }

  if (mode == LaunchMode::Run) {
    for (const QString &member : compound.configurations) {
      runConfiguration(member);
    }
    return;
  }

  if (m_debugStartInProgress) {
    return;
  }
  m_debugStartInProgress = true;
  const auto done = qScopeGuard([this]() { m_debugStartInProgress = false; });

  // Prepare every member first so a failing build starts nothing.
  QList<DebugConfiguration> prepared;
  for (const QString &member : compound.configurations) {
    DebugLaunchRequest request;
    if (!debugRequestForConfiguration(configs.configuration(member),
                                      currentFile, &request) ||
        !prepareDebugLaunch(&request)) {
      return;
    }
    prepared << request.configuration;
  }
  QStringList started;
  for (const DebugConfiguration &config : prepared) {
    const QString sessionId = startPreparedDebugSession(config);
    if (sessionId.isEmpty()) {
      for (const QString &id : started) {
        DebugSessionManager::instance().stopSession(id, compound.stopAll);
      }
      ThemedMessageBox::warning(
          this, tr("Debug %1").arg(name),
          DebugSessionManager::instance().lastError().isEmpty()
              ? tr("Unable to start \"%1\".").arg(config.name)
              : DebugSessionManager::instance().lastError());
      return;
    }
    started << sessionId;
  }
  if (!started.isEmpty()) {
    attachDebugSession(started.first());
  }
}

// ----------------------------------------------------------------- debug --

void MainWindow::startDebuggingForCurrentFile() {
  const RunTarget target = currentRunTarget();
  startDebuggingForFile(target.kind == RunTarget::Kind::PythonBackend
                            ? target.filePath
                            : runSourceFilePath());
}

void MainWindow::startDebuggingForFile(const QString &filePath) {
  if (m_debugStartInProgress) {
    return;
  }

  // F5 while a session runs brings it forward instead of starting another.
  if (!m_activeDebugSessionId.isEmpty()) {
    if (DebugSession *activeSession =
            DebugSessionManager::instance().session(m_activeDebugSessionId)) {
      if (activeSession->state() != DebugSession::State::Idle &&
          activeSession->state() != DebugSession::State::Terminated) {
        attachDebugSession(m_activeDebugSessionId);
        if (debugDock) {
          debugDock->show();
          debugDock->raise();
        }
        return;
      }
    }
  }

  if (filePath.isEmpty()) {
    noScriptAssignedWarning();
    return;
  }

  prepareLaunchWorkspace(filePath);
  m_debugStartInProgress = true;
  const auto done = qScopeGuard([this]() { m_debugStartInProgress = false; });

  invalidateRunTargetCache();
  const RunTarget target = resolveRunTargetForFile(filePath);
  const FileDebugPlan plan = LaunchPlanner::debugPlanForFile(target);
  if (!plan.isValid()) {
    showLaunchProblem(
        tr("Debug"),
        tr("%1 cannot be debugged.").arg(QFileInfo(filePath).fileName()),
        plan.error, LaunchTarget::currentFile(), filePath);
    return;
  }

  DebugLaunchRequest request;
  request.title = QFileInfo(filePath).fileName();
  request.configuration = plan.configuration;
  request.buildSteps = plan.buildSteps;
  request.cmakeBuild = plan.requiresCMakeBuild;
  request.cmakeSourceFile = filePath;
  request.cmakeBinaryDir = target.cmakeBinaryDir;
  const FileTemplateAssignment assignment =
      RunTemplateManager::instance().getAssignmentForFile(filePath);
  if (!assignment.preRunCommand.trimmed().isEmpty()) {
    request.configuration.preLaunchTask =
        RunTemplateManager::substituteCommandLineVariables(
            assignment.preRunCommand.trimmed(), filePath);
  }
  if (!assignment.postRunCommand.trimmed().isEmpty()) {
    request.configuration.postDebugTask =
        RunTemplateManager::substituteCommandLineVariables(
            assignment.postRunCommand.trimmed(), filePath);
  }
  if (!prepareDebugLaunch(&request)) {
    return;
  }
  const QString sessionId = startPreparedDebugSession(request.configuration);
  if (sessionId.isEmpty()) {
    showLaunchProblem(tr("Debug"),
                      tr("Unable to start a debug session for %1.")
                          .arg(QFileInfo(filePath).fileName()),
                      DebugSessionManager::instance().lastError(),
                      LaunchTarget::currentFile(), filePath);
    return;
  }
  attachDebugSession(sessionId);
}

bool MainWindow::debugConfiguration(const QString &name) {
  if (m_debugStartInProgress) {
    return false;
  }
  const QString currentFile = activeEditorFilePath();
  prepareLaunchWorkspace(currentFile);
  if (m_projectRootPath.isEmpty()) {
    noScriptAssignedWarning();
    return false;
  }
  DebugConfigurationManager &configs = DebugConfigurationManager::instance();
  if (!configs.hasConfiguration(name)) {
    showLaunchProblem(
        tr("Debug"), tr("The configuration \"%1\" no longer exists.").arg(name),
        QString(), LaunchTarget::currentFile(), currentFile);
    setActiveLaunchTarget(LaunchTarget::currentFile());
    return false;
  }
  m_debugStartInProgress = true;
  const auto done = qScopeGuard([this]() { m_debugStartInProgress = false; });

  DebugLaunchRequest request;
  if (!debugRequestForConfiguration(configs.configuration(name), currentFile,
                                    &request) ||
      !prepareDebugLaunch(&request)) {
    return false;
  }
  const QString sessionId = startPreparedDebugSession(request.configuration);
  if (sessionId.isEmpty()) {
    showLaunchProblem(tr("Debug"), tr("Unable to start \"%1\".").arg(name),
                      DebugSessionManager::instance().lastError(),
                      LaunchTarget::configuration(name), currentFile);
    return false;
  }
  attachDebugSession(sessionId);
  return true;
}

bool MainWindow::debugRequestForConfiguration(const DebugConfiguration &config,
                                              const QString &currentFile,
                                              DebugLaunchRequest *request) {
  request->title = config.name;
  if (config.type == QLatin1String("command")) {
    showLaunchProblem(
        tr("Debug"), tr("\"%1\" can only run.").arg(config.name),
        tr("It has no debugger. Choose one in its configuration, or use Run."),
        LaunchTarget::configuration(config.name), currentFile);
    return false;
  }
  if (currentFile.isEmpty() && usesCurrentFile(config)) {
    showLaunchProblem(
        tr("Debug"), tr("\"%1\" needs an open file.").arg(config.name),
        tr("It refers to ${file} or a related variable. Open the file to "
           "debug it with, then try again."),
        LaunchTarget::configuration(config.name), currentFile);
    return false;
  }
  request->configuration =
      DebugConfigurationManager::instance().resolveVariables(config,
                                                             currentFile);
  DebugConfiguration &resolved = request->configuration;
  const bool nativeLaunch = resolved.type == QLatin1String("cppdbg") &&
                            resolved.request != QLatin1String("attach");
  if (!nativeLaunch) {
    return true;
  }

  // "program": "${file}" on a C/C++ source debugs what the file builds to.
  if (!currentFile.isEmpty() &&
      QFileInfo(resolved.program) == QFileInfo(currentFile)) {
    const RunTarget fileTarget = resolveRunTargetForFile(currentFile);
    const FileDebugPlan plan = LaunchPlanner::debugPlanForFile(fileTarget);
    if (!plan.isValid()) {
      showLaunchProblem(tr("Debug"),
                        tr("\"%1\" cannot start.").arg(config.name), plan.error,
                        LaunchTarget::configuration(config.name), currentFile);
      return false;
    }
    resolved.program = plan.configuration.program;
    request->buildSteps = plan.buildSteps;
    request->cmakeBuild = plan.requiresCMakeBuild;
    request->cmakeSourceFile = currentFile;
    request->cmakeBinaryDir = fileTarget.cmakeBinaryDir;
    return true;
  }

  // In a CMake project, build the target the configuration starts (named by
  // its program, or chosen when the program is empty) before launching.
  const QString cmakeRoot = CMakeProject::findProjectRoot(m_projectRootPath);
  if (cmakeRoot.isEmpty()) {
    return true;
  }
  bool buildsTarget = resolved.program.isEmpty();
  if (!buildsTarget) {
    CMakeProject project;
    const QFileInfo program(resolved.program);
    for (const CMakeTargetInfo &target :
         project.parseExecutableTargets(cmakeRoot)) {
      buildsTarget = buildsTarget || target.name == program.fileName() ||
                     target.name == program.completeBaseName();
    }
  }
  if (buildsTarget) {
    request->cmakeBuild = true;
    request->cmakeSourceFile = currentFile;
    request->cmakeBinaryDir =
        resolved.adapterConfig.value(QStringLiteral("cmakeBinaryDir"))
            .toString();
  }
  return true;
}

bool MainWindow::prepareDebugLaunch(DebugLaunchRequest *request) {
  DebugConfiguration &config = request->configuration;

  if (config.request.compare("attach", Qt::CaseInsensitive) == 0 &&
      config.processId <= 0) {
    const bool pickInField =
        config.processIdExpression.trimmed() == kPickProcess;
    const bool pickInAdapter =
        config.adapterConfig.value(QStringLiteral("processId"))
            .toString()
            .trimmed() == kPickProcess;
    if (pickInField || pickInAdapter) {
      ProcessPickerDialog picker(this);
      picker.setWindowModality(Qt::ApplicationModal);
      if (picker.exec() != QDialog::Accepted || picker.selectedPid() <= 0) {
        return false;
      }
      config.processId = static_cast<int>(
          qBound<qint64>(1, picker.selectedPid(), qint64(4194304)));
      config.processIdExpression.clear();
      config.adapterConfig.remove(QStringLiteral("processId"));
    }
  }

  QString error;
  const QString workingDirectory =
      config.cwd.isEmpty() ? m_projectRootPath : config.cwd;
  if (!config.preLaunchTask.trimmed().isEmpty()) {
    const auto shell = shellInvocation(config.preLaunchTask.trimmed());
    if (!runBuildProcessWithProgress(
            tr("Before launch: %1").arg(config.preLaunchTask.trimmed()),
            QStringList{shell.first} + shell.second, workingDirectory, nullptr,
            &error)) {
      showBuildFailure(tr("Before-Launch Command Failed"),
                       tr("\"%1\" did not start.").arg(request->title),
                       tr("The before-launch command has to succeed first."),
                       error);
      return false;
    }
  }

  if (request->cmakeBuild) {
    QString programPath;
    QString chosenTarget;
    const QString previousProgram = config.program;
    if (!buildCMakeExecutableForSource(request->cmakeSourceFile, config.program,
                                       request->cmakeBinaryDir, &programPath,
                                       &error, &chosenTarget)) {
      showDebugBuildFailure(error);
      return false;
    }
    if (!programPath.isEmpty()) {
      if (config.cwd.isEmpty() ||
          (!previousProgram.isEmpty() &&
           config.cwd == QFileInfo(previousProgram).absolutePath())) {
        config.cwd = QFileInfo(programPath).absolutePath();
      }
      config.program = programPath;
    }
  }

  for (const LaunchBuildStep &step : request->buildSteps) {
    if (!runBuildProcessWithProgress(step.title, step.command,
                                     step.workingDirectory, nullptr, &error)) {
      showDebugBuildFailure(error);
      return false;
    }
  }

  if (config.request == QLatin1String("launch") &&
      config.type == QLatin1String("cppdbg")) {
    const QFileInfo executable(
        QDir(workingDirectory).absoluteFilePath(config.program));
    if (config.program.isEmpty() || !executable.isFile()) {
      showLaunchProblem(
          tr("Debug"),
          tr("The program for \"%1\" does not exist.").arg(request->title),
          tr("%1\n\nBuild it first, or set Program to the compiled "
             "executable. ${workspaceFolder} is %2.")
              .arg(config.program.isEmpty() ? tr("(no program set)")
                                            : config.program,
                   m_projectRootPath),
          activeLaunchTarget(), activeEditorFilePath());
      return false;
    }
    if (!executable.isExecutable()) {
      showLaunchProblem(tr("Debug"), tr("The program is not executable."),
                        executable.absoluteFilePath(), activeLaunchTarget(),
                        activeEditorFilePath());
      return false;
    }
    config.program = executable.absoluteFilePath();
  }
  return true;
}

QString
MainWindow::startPreparedDebugSession(const DebugConfiguration &config) {
  const QString sessionId =
      DebugSessionManager::instance().startSession(config);
  if (sessionId.isEmpty()) {
    return {};
  }
  const QString after = config.postDebugTask.trimmed();
  if (!after.isEmpty()) {
    const QString workingDirectory =
        config.cwd.isEmpty() ? m_projectRootPath : config.cwd;
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(
        &DebugSessionManager::instance(),
        &DebugSessionManager::sessionTerminated, this,
        [this, sessionId, after, workingDirectory, connection,
         name = config.name](const QString &terminated) {
          if (terminated != sessionId) {
            return;
          }
          disconnect(*connection);
          TerminalLaunch launch;
          launch.title = tr("After %1").arg(name);
          const auto shell = shellInvocation(after);
          launch.program = shell.first;
          launch.arguments = shell.second;
          launch.workingDirectory = workingDirectory;
          startTerminalLaunch(
              launchTerminal(launch.title, workingDirectory, true), launch);
        });
  }
  return sessionId;
}

// ----------------------------------------------------------------- builds --

bool MainWindow::runBuildProcessWithProgress(const QString &title,
                                             const QStringList &command,
                                             const QString &workingDirectory,
                                             QString *output,
                                             QString *errorMessage) {
  if (output) {
    output->clear();
  }
  if (command.isEmpty()) {
    if (errorMessage) {
      *errorMessage = tr("No build command to run.");
    }
    return false;
  }

  if (m_buildInProgress) {
    if (errorMessage) {
      *errorMessage = tr("A build is already running.");
    }
    return false;
  }

  m_lastBuildDirectory = workingDirectory;
  Terminal *terminal = launchTerminal(title, workingDirectory, false);
  if (!terminal) {
    if (errorMessage) {
      *errorMessage = tr("The terminal panel is unavailable.");
    }
    return false;
  }
  // The build replaces whatever launch the terminal was following.
  delete terminal->findChild<QObject *>(QLatin1String(kLaunchChainName),
                                        Qt::FindDirectChildrenOnly);

  statusBar()->showMessage(title);
  m_buildInProgress = true;

  QEventLoop loop;
  int exitCode = -1;
  bool startFailed = false;
  QString startError;

  const QMetaObject::Connection finishedConnection =
      connect(terminal, &Terminal::processFinished, &loop,
              [&loop, &exitCode](int code) {
                exitCode = code;
                loop.quit();
              });
  const QMetaObject::Connection errorConnection =
      connect(terminal, &Terminal::processError, &loop,
              [&loop, &startFailed, &startError](const QString &message) {
                startFailed = true;
                startError = message;
                loop.quit();
              });

  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);
  timeoutTimer.setInterval(600000);
  QPointer<Terminal> guard(terminal);
  connect(&timeoutTimer, &QTimer::timeout, &loop, [guard]() {
    if (guard) {
      guard->interruptActiveProcess();
    }
  });
  timeoutTimer.start();

  terminal->executeCommand(command.first(), command.mid(1), workingDirectory);
  loop.exec();

  timeoutTimer.stop();
  disconnect(finishedConnection);
  disconnect(errorConnection);
  m_buildInProgress = false;

  const QString transcript = guard ? guard->runTranscript() : QString();
  if (output) {
    *output = transcript;
  }

  if (startFailed) {
    if (errorMessage) {
      *errorMessage = tr("Could not run '%1': %2")
                          .arg(command.join(QLatin1Char(' ')), startError);
    }
    statusBar()->clearMessage();
    return false;
  }

  if (exitCode != 0) {
    if (errorMessage) {
      const QString details = transcript.trimmed();
      *errorMessage =
          tr("'%1' failed:\n%2")
              .arg(command.join(QLatin1Char(' ')),
                   details.isEmpty() ? tr("exited with code %1").arg(exitCode)
                                     : details.right(8000));
    }
    statusBar()->showMessage(tr("%1 failed").arg(title), 8000);
    return false;
  }

  if (m_diagnosticsManager) {
    m_diagnosticsManager->clearAllForSource(kBuildDiagnosticsSource);
  }
  statusBar()->clearMessage();
  return true;
}

QString MainWindow::cmakeRootForSource(const QString &filePath) const {
  return RunTargetResolver::cmakeRootFor(filePath, m_projectRootPath);
}

bool MainWindow::buildCMakeExecutableForSource(
    const QString &filePath, const QString &configuredProgram,
    const QString &configuredBinaryDir, QString *programPath,
    QString *errorMessage, QString *chosenTargetName) {
  if (programPath) {
    programPath->clear();
  }
  if (chosenTargetName) {
    chosenTargetName->clear();
  }

  const QString cmakeRoot = cmakeRootForSource(filePath);
  if (cmakeRoot.isEmpty()) {

    return true;
  }

  CMakeProject project;
  const QList<CMakeTargetInfo> executables =
      project.parseExecutableTargets(cmakeRoot);

  const QString binaryDir =
      CMakeProject::resolveBinaryDirectory(cmakeRoot, configuredBinaryDir);

  QString exePath;
  QString targetName;
  if (!executables.isEmpty()) {
    if (!configuredProgram.isEmpty()) {
      const QString programFile = QFileInfo(configuredProgram).fileName();
      const QString programStem =
          QFileInfo(configuredProgram).completeBaseName();
      for (const CMakeTargetInfo &target : executables) {
        if (target.name == programFile || target.name == programStem) {
          exePath = CMakeProject::executablePathFor(target, binaryDir);
          targetName = target.name;
          break;
        }
      }
    }

    if (exePath.isEmpty()) {
      const QString owner =
          CMakeProject::targetForSource(executables, cmakeRoot, filePath);
      if (!owner.isEmpty()) {
        for (const CMakeTargetInfo &target : executables) {
          if (target.name == owner) {
            exePath = CMakeProject::executablePathFor(target, binaryDir);
            targetName = target.name;
            break;
          }
        }
      }
    }

    const QString preferredTarget = effectivePreferredCMakeTarget();
    if (exePath.isEmpty() && !preferredTarget.isEmpty()) {
      for (const CMakeTargetInfo &target : executables) {
        if (target.name == preferredTarget) {
          exePath = CMakeProject::executablePathFor(target, binaryDir);
          targetName = target.name;
          break;
        }
      }
    }

    const bool sourceIsCMakeLists =
        QFileInfo(filePath).fileName().compare(QStringLiteral("CMakeLists.txt"),
                                               Qt::CaseInsensitive) == 0;
    if (exePath.isEmpty() && executables.size() == 1 &&
        (sourceIsCMakeLists ||
         CMakeProject::hasUnresolvedSources(executables))) {
      exePath = CMakeProject::executablePathFor(executables.first(), binaryDir);
      targetName = executables.first().name;
    }

    if (exePath.isEmpty()) {
      CMakeTargetPickerDialog picker(executables, this);
      picker.setWindowModality(Qt::ApplicationModal);
      if (picker.exec() != QDialog::Accepted ||
          picker.selectedTargetName().isEmpty()) {
        if (errorMessage) {
          *errorMessage = tr("A build target must be selected to run or debug "
                             "this CMake project.");
        }
        return false;
      }
      for (const CMakeTargetInfo &target : executables) {
        if (target.name == picker.selectedTargetName()) {
          exePath = CMakeProject::executablePathFor(target, binaryDir);
          targetName = target.name;
          break;
        }
      }
      setPreferredCMakeTarget(targetName);
    }
  } else if (configuredProgram.isEmpty()) {
    if (errorMessage) {
      *errorMessage =
          tr("No executable targets were found in %1. Add an "
             "add_executable() declaration or point 'program' at a built "
             "binary.")
              .arg(cmakeRoot);
    }
    return false;
  }

  QString configureOutput;
  if (CMakeProject::needsConfigure(binaryDir)) {
    if (!runBuildProcessWithProgress(
            tr("Configuring CMake project…"),
            CMakeProject::configureCommand(cmakeRoot, binaryDir), cmakeRoot,
            &configureOutput, errorMessage)) {
      return false;
    }
  }

  QString buildOutput;
  const int jobs = qMax(1, QThread::idealThreadCount());

  const QString buildTitle = targetName.isEmpty()
                                 ? tr("Building project…")
                                 : tr("Building %1…").arg(targetName);
  if (!runBuildProcessWithProgress(
          buildTitle, CMakeProject::buildCommand(binaryDir, jobs, targetName),
          binaryDir, &buildOutput, errorMessage)) {
    return false;
  }

  LOG_INFO(QString("CMake build output tail: %1")
               .arg(buildOutput.trimmed().section(QLatin1Char('\n'), -3, -1)));

  const QString resolved = exePath.isEmpty() ? configuredProgram : exePath;
  if (resolved.isEmpty() || !QFileInfo::exists(resolved)) {
    if (errorMessage) {
      *errorMessage = tr("The built program was not found: %1")
                          .arg(resolved.isEmpty() ? tr("(no executable target)")
                                                  : resolved);
    }
    return false;
  }

  if (programPath) {
    *programPath = resolved;
  }
  if (chosenTargetName) {
    *chosenTargetName = targetName;
  }
  LOG_INFO(QString("CMake target ready: %1").arg(resolved));
  return true;
}
