#include "pythonenvironmentdialog.h"
#include "../../run_templates/runtemplatemanager.h"
#include "../uistylehelper.h"
#include "../widgets/pythonenvironmentwidget.h"

#include "themedmessagebox.h"
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QString displayValue(const QString &value,
                     const QString &fallback = QString("(not set)")) {
  return value.trimmed().isEmpty() ? fallback : value;
}

QString joinIndentedList(const QStringList &values) {
  if (values.isEmpty()) {
    return "  (none)";
  }

  QStringList lines;
  for (const QString &value : values) {
    lines.append(QString("  - %1").arg(value));
  }
  return lines.join('\n');
}
} // namespace

PythonEnvironmentDialog::PythonEnvironmentDialog(const QString &workspaceFolder,
                                                 const QString &filePath,
                                                 const Theme &theme,
                                                 QWidget *parent)
    : StyledDialog(parent), m_workspaceFolder(workspaceFolder),
      m_filePath(filePath), m_theme(theme), m_contextLabel(nullptr),
      m_environmentWidget(nullptr), m_detailsEdit(nullptr),
      m_saveButton(nullptr), m_closeButton(nullptr) {
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(tr("Python Environment"));
  resize(760, 680);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  auto *titleLabel = new QLabel(tr("Python Environment"), this);
  layout->addWidget(titleLabel);

  m_contextLabel = new QLabel(this);
  m_contextLabel->setWordWrap(true);
  m_contextLabel->setText(
      tr("Editing Python setup for %1")
          .arg(displayValue(QDir::toNativeSeparators(filePath))));
  layout->addWidget(m_contextLabel);

  m_environmentWidget = new PythonEnvironmentWidget(this);
  m_environmentWidget->setDebugToolsVisible(true);
  m_environmentWidget->setContext(workspaceFolder, filePath,
                                  QFileInfo(filePath).absolutePath());

  RunTemplateManager &manager = RunTemplateManager::instance();
  FileTemplateAssignment assignment = manager.getAssignmentForFile(filePath);
  PythonEnvironmentPreference preference;
  preference.mode = assignment.pythonMode;
  preference.customInterpreter = assignment.pythonInterpreter;
  preference.venvPath = assignment.pythonVenvPath;
  preference.requirementsFile = assignment.pythonRequirementsFile;
  m_environmentWidget->setPreference(preference);
  layout->addWidget(m_environmentWidget);

  auto *detailsTitle = new QLabel(tr("Resolution Details"), this);
  layout->addWidget(detailsTitle);

  m_detailsEdit = new QPlainTextEdit(this);
  m_detailsEdit->setReadOnly(true);
  layout->addWidget(m_detailsEdit, 1);

  auto *buttonRow = new QHBoxLayout();
  buttonRow->addStretch();
  m_saveButton = new QPushButton(tr("Save"), this);
  m_closeButton = new QPushButton(tr("Close"), this);
  buttonRow->addWidget(m_saveButton);
  buttonRow->addWidget(m_closeButton);
  layout->addLayout(buttonRow);

  connect(m_environmentWidget, &PythonEnvironmentWidget::preferenceChanged,
          this, &PythonEnvironmentDialog::refreshDiagnostics);
  connect(m_environmentWidget, &PythonEnvironmentWidget::environmentChanged,
          this, &PythonEnvironmentDialog::refreshDiagnostics);
  connect(m_saveButton, &QPushButton::clicked, this,
          &PythonEnvironmentDialog::saveConfiguration);
  connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);

  applyTheme(theme);
  styleTitleLabel(titleLabel);
  styleTitleLabel(detailsTitle);
  styleSubduedLabel(m_contextLabel);
  stylePrimaryButton(m_saveButton);
  refreshDiagnostics();
}

void PythonEnvironmentDialog::refreshDiagnostics() {
  const PythonEnvironmentPreference preference =
      m_environmentWidget->preference();
  const PythonEnvironmentInfo info = PythonProjectEnvironment::resolve(
      preference, m_workspaceFolder, m_filePath,
      QFileInfo(m_filePath).absolutePath());
  const PythonEnvironmentDiagnostics diagnostics =
      PythonProjectEnvironment::diagnostics(
          preference, m_workspaceFolder, m_filePath,
          QFileInfo(m_filePath).absolutePath());

  QStringList lines;
  lines << QString("File: %1")
               .arg(displayValue(QDir::toNativeSeparators(m_filePath)))
        << QString("Workspace root: %1")
               .arg(displayValue(
                   QDir::toNativeSeparators(diagnostics.workspaceRoot)))
        << QString("Working directory: %1")
               .arg(displayValue(QDir::toNativeSeparators(
                   QFileInfo(m_filePath).absolutePath())))
        << QString("Mode: %1").arg(displayValue(preference.mode, "auto"))
        << QString("Resolved interpreter: %1")
               .arg(displayValue(QDir::toNativeSeparators(info.interpreter),
                                 "(not resolved)"))
        << QString("Resolved virtualenv: %1")
               .arg(displayValue(QDir::toNativeSeparators(info.venvPath),
                                 "(not resolved)"))
        << QString("Resolved requirements file: %1")
               .arg(displayValue(QDir::toNativeSeparators(
                                     diagnostics.resolvedRequirementsFile),
                                 "(not found)"))
        << QString("Status: %1")
               .arg(displayValue(info.statusMessage, "(no status message)"))
        << QString("Configured custom interpreter: %1")
               .arg(displayValue(QDir::toNativeSeparators(
                   diagnostics.normalizedCustomInterpreter)))
        << QString("Configured virtualenv path: %1")
               .arg(displayValue(QDir::toNativeSeparators(
                   diagnostics.normalizedConfiguredVenvPath)))
        << QString("Active shell interpreter: %1")
               .arg(displayValue(QDir::toNativeSeparators(
                                     diagnostics.activeEnvironmentInterpreter),
                                 "(none)"))
        << QString("Global fallback interpreter: %1")
               .arg(displayValue(
                   QDir::toNativeSeparators(diagnostics.globalInterpreter),
                   "(not found)"))
        << "" << "Searched virtualenv locations:"
        << joinIndentedList(diagnostics.searchedVenvPaths) << ""
        << "Requirements search roots:"
        << joinIndentedList(diagnostics.searchedRequirementsRoots) << ""
        << "Saved in:"
        << "  .lightpad/run_config.json (per-file Python settings)";

  m_detailsEdit->setPlainText(lines.join('\n'));
}

void PythonEnvironmentDialog::saveConfiguration() {
  if (m_filePath.trimmed().isEmpty()) {
    ThemedMessageBox::warning(
        this, tr("Python Environment"),
        tr("Open a Python file before editing its environment."));
    return;
  }

  RunTemplateManager &manager = RunTemplateManager::instance();
  FileTemplateAssignment assignment = manager.getAssignmentForFile(m_filePath);
  const PythonEnvironmentPreference preference =
      m_environmentWidget->preference();

  assignment.pythonMode = preference.mode;
  assignment.pythonInterpreter = preference.customInterpreter;
  assignment.pythonVenvPath = preference.venvPath;
  assignment.pythonRequirementsFile = preference.requirementsFile;

  if (!manager.assignTemplateToFile(m_filePath, assignment)) {
    ThemedMessageBox::warning(
        this, tr("Python Environment"),
        tr("Failed to save Python environment settings."));
    return;
  }

  emit configurationSaved();
  accept();
}
