#include "launchconfigurationsdialog.h"

#include "../../core/pathvariables.h"
#include "../../dap/debugadapterregistry.h"
#include "../../language/languagecatalog.h"
#include "../../run_templates/launchtemplatecatalog.h"
#include "../../run_templates/runtargetresolver.h"
#include "../widgets/keyvaluetable.h"
#include "../widgets/pythonenvironmentwidget.h"
#include "launchtemplategallery.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QMenu>
#include <QProcess>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int KindRole = Qt::UserRole;
constexpr int IndexRole = Qt::UserRole + 1;
constexpr int TypeRole = Qt::UserRole;
constexpr int AdapterIdRole = Qt::UserRole + 1;
constexpr int TemplateIdRole = Qt::UserRole;

const QLatin1String kRunOnlyType("command");
const QLatin1String kPickProcess("${command:pickProcess}");

QString joinArguments(const QStringList &arguments) {
  return DebugConfigurationManager::joinCommandLineArguments(arguments);
}

QStringList splitArguments(const QString &text) {
  return DebugConfigurationManager::splitCommandLineArguments(text.trimmed());
}

QFormLayout *newForm(QWidget *parent) {
  auto *form = new QFormLayout(parent);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  form->setHorizontalSpacing(14);
  form->setVerticalSpacing(10);
  form->setContentsMargins(0, 0, 0, 0);
  return form;
}

// A line edit with a "Browse…" button that picks a file or a folder.
QWidget *withBrowse(QLineEdit *edit, bool directory, QWidget *dialog,
                    const QString &title, const QString &startDir) {
  auto *row = new QWidget(dialog);
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  layout->addWidget(edit, 1);
  auto *browse = new QPushButton(QObject::tr("Browse…"), row);
  browse->setAccessibleName(QObject::tr("Browse for %1").arg(title));
  layout->addWidget(browse);
  QObject::connect(
      browse, &QPushButton::clicked, dialog,
      [edit, directory, dialog, title, startDir]() {
        const QString start =
            edit->text().contains(QLatin1String("${"))
                ? startDir
                : (edit->text().isEmpty() ? startDir : edit->text());
        const QString chosen =
            directory ? QFileDialog::getExistingDirectory(dialog, title, start)
                      : QFileDialog::getOpenFileName(dialog, title, start);
        if (!chosen.isEmpty()) {
          edit->setText(chosen);
        }
      });
  return row;
}

QLabel *sectionTitle(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setObjectName(QStringLiteral("launchSectionTitle"));
  return label;
}

QString variablesTooltip() {
  QStringList rows;
  for (const auto &entry : PathVariables::documentation()) {
    rows << QStringLiteral("<tr><td><code>%1</code></td><td>%2</td></tr>")
                .arg(entry.first.toHtmlEscaped(), entry.second.toHtmlEscaped());
  }
  return QStringLiteral("<b>%1</b><table cellspacing='4'>%2</table>")
      .arg(QObject::tr("Variables"), rows.join(QString()));
}

QLabel *variablesHint(QWidget *parent) {
  auto *label = new QLabel(
      QObject::tr("Fields accept variables such as ${file}, "
                  "${workspaceFolder} and ${env:NAME}; hover for the list."),
      parent);
  label->setWordWrap(true);
  label->setToolTip(variablesTooltip());
  label->setObjectName(QStringLiteral("launchHint"));
  return label;
}

bool isPythonType(const QString &type) {
  return type == QLatin1String("debugpy") || type == QLatin1String("python");
}

bool isNodeType(const QString &type) {
  return type == QLatin1String("node") || type == QLatin1String("pwa-node");
}

bool samePreference(const PythonEnvironmentPreference &a,
                    const PythonEnvironmentPreference &b) {
  return a.mode == b.mode && a.customInterpreter == b.customInterpreter &&
         a.venvPath == b.venvPath && a.requirementsFile == b.requirementsFile;
}

QString adapterFamilyName(const QString &adapterName) {
  const int paren = adapterName.indexOf(QLatin1String(" ("));
  return paren > 0 ? adapterName.left(paren) : adapterName;
}
} // namespace

LaunchConfigurationsDialog::LaunchConfigurationsDialog(const Context &context,
                                                       QWidget *parent)
    : StyledDialog(parent), m_context(context) {
  DebugConfigurationManager &configs = DebugConfigurationManager::instance();
  m_configurations = configs.allConfigurations();
  m_compounds = configs.allCompoundConfigurations();
  if (!m_context.currentFilePath.isEmpty()) {
    m_fileAssignment = RunTemplateManager::instance().getAssignmentForFile(
        m_context.currentFilePath);
  }

  m_filePreviewTimer = new QTimer(this);
  m_filePreviewTimer->setSingleShot(true);
  m_filePreviewTimer->setInterval(200);
  connect(m_filePreviewTimer, &QTimer::timeout, this,
          &LaunchConfigurationsDialog::refreshFilePreview);
  m_configurationPreviewTimer = new QTimer(this);
  m_configurationPreviewTimer->setSingleShot(true);
  m_configurationPreviewTimer->setInterval(200);
  connect(m_configurationPreviewTimer, &QTimer::timeout, this,
          &LaunchConfigurationsDialog::refreshConfigurationPreview);
  m_statusTimer = new QTimer(this);
  m_statusTimer->setSingleShot(true);
  m_statusTimer->setInterval(400);
  connect(m_statusTimer, &QTimer::timeout, this,
          &LaunchConfigurationsDialog::refreshDebuggerStatus);

  setupUi();
  loadFileSettings();

  EntryKind kind = EntryKind::None;
  int index = -1;
  switch (m_context.selection.kind) {
  case LaunchTarget::Kind::Configuration:
    for (int i = 0; i < m_configurations.size(); ++i) {
      if (m_configurations.at(i).name == m_context.selection.name) {
        kind = EntryKind::Configuration;
        index = i;
      }
    }
    break;
  case LaunchTarget::Kind::Compound:
    for (int i = 0; i < m_compounds.size(); ++i) {
      if (m_compounds.at(i).name == m_context.selection.name) {
        kind = EntryKind::Compound;
        index = i;
      }
    }
    break;
  case LaunchTarget::Kind::CurrentFile:
    break;
  }
  if (kind == EntryKind::None) {
    if (!m_context.currentFilePath.isEmpty()) {
      kind = EntryKind::File;
    } else if (!m_configurations.isEmpty()) {
      kind = EntryKind::Configuration;
      index = 0;
    }
  }
  rebuildTree(kind, index);
}

// ---------------------------------------------------------------- layout --

void LaunchConfigurationsDialog::setupUi() {
  setWindowTitle(tr("Run and Debug Configurations"));
  setMinimumSize(880, 600);
  resize(1020, 720);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 14, 14, 12);
  layout->setSpacing(10);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setChildrenCollapsible(false);

  auto *left = new QWidget(splitter);
  auto *leftLayout = new QVBoxLayout(left);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);
  m_tree = new QTreeWidget(left);
  m_tree->setHeaderHidden(true);
  m_tree->setRootIsDecorated(false);
  m_tree->setIndentation(14);
  m_tree->setUniformRowHeights(true);
  m_tree->setMinimumWidth(240);
  m_tree->setAccessibleName(tr("Launch entries"));
  m_tree->setObjectName(QStringLiteral("launchEntries"));
  connect(m_tree, &QTreeWidget::currentItemChanged, this,
          &LaunchConfigurationsDialog::onCurrentItemChanged);
  leftLayout->addWidget(m_tree, 1);

  auto *listButtons = new QHBoxLayout();
  listButtons->setSpacing(6);
  m_addButton = new QPushButton(tr("Add"), left);
  m_addButton->setToolTip(tr("Add a configuration from a template, or a "
                             "compound that starts several together"));
  m_addMenu = new QMenu(m_addButton);
  connect(m_addMenu, &QMenu::aboutToShow, this,
          &LaunchConfigurationsDialog::populateAddMenu);
  m_addButton->setMenu(m_addMenu);
  m_duplicateButton = new QPushButton(tr("Duplicate"), left);
  m_removeButton = new QPushButton(tr("Remove"), left);
  connect(m_duplicateButton, &QPushButton::clicked, this,
          &LaunchConfigurationsDialog::duplicateCurrent);
  connect(m_removeButton, &QPushButton::clicked, this,
          &LaunchConfigurationsDialog::removeCurrent);
  listButtons->addWidget(m_addButton, 1);
  listButtons->addWidget(m_duplicateButton, 1);
  listButtons->addWidget(m_removeButton, 1);
  leftLayout->addLayout(listButtons);

  m_pages = new QStackedWidget(splitter);
  m_filePage = buildFilePage();
  m_configurationPage = buildConfigurationPage();
  m_compoundPage = buildCompoundPage();
  m_emptyPage = buildEmptyPage();
  for (QWidget *page :
       {m_filePage, m_configurationPage, m_compoundPage, m_emptyPage}) {
    auto *scroll = new QScrollArea(m_pages);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(page);
    m_pages->addWidget(scroll);
  }

  splitter->addWidget(left);
  splitter->addWidget(m_pages);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);
  splitter->setSizes({260, 740});
  layout->addWidget(splitter, 1);
  layout->addWidget(buildFooter());
}

QWidget *LaunchConfigurationsDialog::buildFooter() {
  auto *footer = new QWidget(this);
  auto *layout = new QHBoxLayout(footer);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  m_validationLabel = new QLabel(footer);
  m_validationLabel->setObjectName(QStringLiteral("validationLabel"));
  m_validationLabel->setWordWrap(true);
  m_validationLabel->setVisible(false);
  layout->addWidget(m_validationLabel, 1);
  layout->addStretch();

  m_runButton = new QPushButton(tr("Run"), footer);
  m_runButton->setToolTip(tr("Save, close, and run the selected entry"));
  m_debugButton = new QPushButton(tr("Debug"), footer);
  m_debugButton->setToolTip(tr("Save, close, and debug the selected entry"));
  m_cancelButton = new QPushButton(tr("Cancel"), footer);
  m_saveButton = new QPushButton(tr("Save"), footer);
  m_saveButton->setObjectName(QStringLiteral("launchSaveButton"));
  m_runButton->setObjectName(QStringLiteral("launchRunButton"));
  m_debugButton->setObjectName(QStringLiteral("launchDebugButton"));
  for (QPushButton *button :
       {m_runButton, m_debugButton, m_cancelButton, m_saveButton}) {
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setMinimumWidth(96);
  }
  layout->addWidget(m_runButton);
  layout->addWidget(m_debugButton);
  layout->addSpacing(16);
  layout->addWidget(m_cancelButton);
  layout->addWidget(m_saveButton);

  connect(m_runButton, &QPushButton::clicked, this,
          [this]() { launchSelected(LaunchMode::Run); });
  connect(m_debugButton, &QPushButton::clicked, this,
          [this]() { launchSelected(LaunchMode::Debug); });
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_saveButton, &QPushButton::clicked, this, [this]() {
    if (save()) {
      accept();
    }
  });
  setKeyboardDefault(m_saveButton);
  return footer;
}

QWidget *LaunchConfigurationsDialog::buildFilePage() {
  auto *page = new QWidget(this);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(16, 4, 12, 8);
  layout->setSpacing(12);

  m_fileTitle = new QLabel(page);
  m_fileTitle->setObjectName(QStringLiteral("launchPageTitle"));
  m_filePath = new QLabel(page);
  m_filePath->setWordWrap(true);
  m_filePath->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_filePath->setObjectName(QStringLiteral("launchPageSubtitle"));
  layout->addWidget(m_fileTitle);
  layout->addWidget(m_filePath);

  // What Run and Debug will do, kept up to date while editing below.
  m_filePreview = new QLabel(page);
  m_filePreview->setObjectName(QStringLiteral("launchPreview"));
  m_filePreview->setWordWrap(true);
  m_filePreview->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_filePreview->setTextFormat(Qt::RichText);
  m_filePreview->setAccessibleName(tr("What Run and Debug start"));
  layout->addWidget(m_filePreview);

  auto *formHost = new QWidget(page);
  QFormLayout *form = newForm(formHost);
  m_fileForm = form;
  m_fileTemplateCombo = new QComboBox(formHost);
  m_fileTemplateCombo->setAccessibleName(tr("Run with"));
  m_fileTemplateCombo->setSizeAdjustPolicy(
      QComboBox::AdjustToMinimumContentsLengthWithIcon);
  form->addRow(tr("Run with"), m_fileTemplateCombo);
  m_fileTemplateDescription = new QLabel(formHost);
  m_fileTemplateDescription->setWordWrap(true);
  m_fileTemplateDescription->setObjectName(QStringLiteral("launchHint"));
  form->addRow(QString(), m_fileTemplateDescription);

  m_fileArgsEdit = new QLineEdit(formHost);
  m_fileArgsEdit->setObjectName(QStringLiteral("fileArguments"));
  m_fileArgsEdit->setPlaceholderText(tr("Program arguments, e.g. --verbose "
                                        "\"two words\""));
  form->addRow(tr("Arguments"), m_fileArgsEdit);
  m_fileCwdEdit = new QLineEdit(formHost);
  m_fileCwdEdit->setPlaceholderText(tr("Template default"));
  form->addRow(tr("Working directory"),
               withBrowse(m_fileCwdEdit, true, this, tr("Working Directory"),
                          m_context.workspaceFolder));
  m_fileEnvTable = new KeyValueTable(formHost);
  form->addRow(tr("Environment"), m_fileEnvTable);
  m_filePreEdit = new QLineEdit(formHost);
  m_filePreEdit->setPlaceholderText(
      tr("Shell command run first, e.g. make generate"));
  form->addRow(tr("Before launch"), m_filePreEdit);
  m_filePostEdit = new QLineEdit(formHost);
  m_filePostEdit->setPlaceholderText(tr("Shell command run after it stops"));
  form->addRow(tr("After stop"), m_filePostEdit);

  m_fileFlagsEdit = new QLineEdit(formHost);
  m_fileFlagsEdit->setPlaceholderText(tr("e.g. -Wall -O2 -lpthread"));
  form->addRow(tr("Compiler flags"), m_fileFlagsEdit);
  m_fileSourcesEdit = new QLineEdit(formHost);
  m_fileSourcesEdit->setPlaceholderText(
      tr("Other sources to compile with it, e.g. ${fileDir}/util.c"));
  auto *sourcesRow = new QWidget(formHost);
  auto *sourcesLayout = new QHBoxLayout(sourcesRow);
  sourcesLayout->setContentsMargins(0, 0, 0, 0);
  sourcesLayout->setSpacing(6);
  sourcesLayout->addWidget(m_fileSourcesEdit, 1);
  auto *addSources = new QPushButton(tr("Add Files…"), sourcesRow);
  sourcesLayout->addWidget(addSources);
  connect(addSources, &QPushButton::clicked, this, [this]() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Add Source Files"),
        QFileInfo(m_context.currentFilePath).absolutePath());
    QStringList sources = splitArguments(m_fileSourcesEdit->text());
    sources << files;
    m_fileSourcesEdit->setText(joinArguments(sources));
  });
  form->addRow(tr("Extra sources"), sourcesRow);
  layout->addWidget(formHost);

  m_filePythonWidget = new PythonEnvironmentWidget(page);
  m_filePythonWidget->setDebugToolsVisible(true);
  layout->addWidget(m_filePythonWidget);
  layout->addWidget(variablesHint(page));

  auto *resetRow = new QHBoxLayout();
  m_fileResetButton = new QPushButton(tr("Reset to Defaults"), page);
  m_fileResetButton->setToolTip(
      tr("Forget the settings above and run the file the automatic way"));
  connect(m_fileResetButton, &QPushButton::clicked, this,
          &LaunchConfigurationsDialog::resetFileSettings);
  resetRow->addWidget(m_fileResetButton);
  resetRow->addStretch();
  layout->addLayout(resetRow);
  layout->addStretch();

  for (QLineEdit *edit : {m_fileArgsEdit, m_fileCwdEdit, m_filePreEdit,
                          m_filePostEdit, m_fileFlagsEdit, m_fileSourcesEdit}) {
    connect(edit, &QLineEdit::textChanged, this,
            &LaunchConfigurationsDialog::scheduleFilePreview);
  }
  connect(m_fileEnvTable, &KeyValueTable::changed, this,
          &LaunchConfigurationsDialog::scheduleFilePreview);
  connect(m_filePythonWidget, &PythonEnvironmentWidget::preferenceChanged, this,
          &LaunchConfigurationsDialog::scheduleFilePreview);
  connect(
      m_fileTemplateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
      this, [this]() {
        const RunTemplate runTemplate =
            RunTemplateManager::instance().getTemplateById(
                m_fileTemplateCombo->currentData(TemplateIdRole).toString());
        m_fileTemplateDescription->setText(
            runTemplate.isValid()
                ? runTemplate.description
                : tr("Lightpad picks how to run the file: CMake targets, "
                     "test files, web backends and project tools first, "
                     "then the language's default template."));
        scheduleFilePreview();
      });
  connect(m_fileCwdEdit, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            m_filePythonWidget->setContext(m_context.workspaceFolder,
                                           m_context.currentFilePath,
                                           text.trimmed());
          });
  return page;
}

QWidget *LaunchConfigurationsDialog::buildConfigurationPage() {
  auto *page = new QWidget(this);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(16, 4, 12, 8);
  layout->setSpacing(12);

  // What Run and Debug will do, kept up to date while editing below.
  m_configurationPreview = new QLabel(page);
  m_configurationPreview->setObjectName(QStringLiteral("launchPreview"));
  m_configurationPreview->setWordWrap(true);
  m_configurationPreview->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_configurationPreview->setTextFormat(Qt::RichText);
  m_configurationPreview->setAccessibleName(tr("What Run and Debug start"));
  layout->addWidget(m_configurationPreview);
  m_debuggerStatus = new QLabel(page);
  m_debuggerStatus->setWordWrap(true);
  m_debuggerStatus->setObjectName(QStringLiteral("launchHint"));
  layout->addWidget(m_debuggerStatus);

  auto *formHost = new QWidget(page);
  m_launchForm = newForm(formHost);
  m_nameEdit = new QLineEdit(formHost);
  m_nameEdit->setPlaceholderText(tr("Shown in the Run and Debug menus"));
  m_launchForm->addRow(tr("Name"), m_nameEdit);
  m_debuggerCombo = new QComboBox(formHost);
  m_debuggerCombo->setAccessibleName(tr("Debugger"));
  m_launchForm->addRow(tr("Debugger"), m_debuggerCombo);
  m_requestCombo = new QComboBox(formHost);
  m_requestCombo->addItem(tr("Launch a program"), QStringLiteral("launch"));
  m_requestCombo->addItem(tr("Attach to a running program"),
                          QStringLiteral("attach"));
  m_launchForm->addRow(tr("Start by"), m_requestCombo);

  m_programEdit = new QLineEdit(formHost);
  m_programEdit->setObjectName(QStringLiteral("launchProgram"));
  m_programEdit->setPlaceholderText(
      tr("Executable or script, e.g. ${workspaceFolder}/build/app"));
  m_launchForm->addRow(tr("Program"),
                       withBrowse(m_programEdit, false, this, tr("Program"),
                                  m_context.workspaceFolder));
  m_moduleEdit = new QLineEdit(formHost);
  m_moduleEdit->setPlaceholderText(
      tr("Run a module instead of a script (python -m), e.g. app.main"));
  m_launchForm->addRow(tr("Module"), m_moduleEdit);
  m_runtimeArgsEdit = new QLineEdit(formHost);
  m_runtimeArgsEdit->setPlaceholderText(
      tr("Arguments for the runtime, e.g. run-script start"));
  m_launchForm->addRow(tr("Runtime arguments"), m_runtimeArgsEdit);
  m_argsEdit = new QLineEdit(formHost);
  m_argsEdit->setObjectName(QStringLiteral("launchArguments"));
  m_argsEdit->setPlaceholderText(
      tr("Program arguments, e.g. --port 8000 \"two words\""));
  m_launchForm->addRow(tr("Arguments"), m_argsEdit);
  m_cwdEdit = new QLineEdit(formHost);
  m_cwdEdit->setPlaceholderText(QStringLiteral("${workspaceFolder}"));
  m_launchForm->addRow(tr("Working directory"),
                       withBrowse(m_cwdEdit, true, this,
                                  tr("Working Directory"),
                                  m_context.workspaceFolder));
  m_envTable = new KeyValueTable(formHost);
  m_launchForm->addRow(tr("Environment"), m_envTable);

  auto *processRow = new QWidget(formHost);
  auto *processLayout = new QHBoxLayout(processRow);
  processLayout->setContentsMargins(0, 0, 0, 0);
  processLayout->setSpacing(6);
  m_processEdit = new QLineEdit(processRow);
  m_processEdit->setPlaceholderText(
      tr("Process ID; leave empty to connect to host and port"));
  auto *pick = new QPushButton(tr("Ask When Starting"), processRow);
  pick->setToolTip(tr("Show a process list each time the session starts"));
  connect(pick, &QPushButton::clicked, this,
          [this]() { m_processEdit->setText(kPickProcess); });
  processLayout->addWidget(m_processEdit, 1);
  processLayout->addWidget(pick);
  m_launchForm->addRow(tr("Process"), processRow);
  m_hostEdit = new QLineEdit(formHost);
  m_hostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
  m_launchForm->addRow(tr("Host"), m_hostEdit);
  m_portSpin = new QSpinBox(formHost);
  m_portSpin->setRange(0, 65535);
  m_portSpin->setSpecialValueText(tr("Not set"));
  m_launchForm->addRow(tr("Port"), m_portSpin);

  m_preLaunchEdit = new QLineEdit(formHost);
  m_preLaunchEdit->setPlaceholderText(
      tr("Shell command that must succeed first, e.g. cmake --build build"));
  m_launchForm->addRow(tr("Before launch"), m_preLaunchEdit);
  m_postLaunchEdit = new QLineEdit(formHost);
  m_postLaunchEdit->setPlaceholderText(tr("Shell command run after it stops"));
  m_launchForm->addRow(tr("After stop"), m_postLaunchEdit);
  layout->addWidget(formHost);

  m_pythonWidget = new PythonEnvironmentWidget(page);
  m_pythonWidget->setDebugToolsVisible(true);
  layout->addWidget(m_pythonWidget);

  m_debuggerGroup = new QWidget(page);
  auto *debuggerLayout = new QVBoxLayout(m_debuggerGroup);
  debuggerLayout->setContentsMargins(0, 4, 0, 0);
  debuggerLayout->setSpacing(8);
  debuggerLayout->addWidget(
      sectionTitle(tr("Debugger options"), m_debuggerGroup));
  m_stopOnEntryCheck =
      new QCheckBox(tr("Pause at the program's entry point"), m_debuggerGroup);
  debuggerLayout->addWidget(m_stopOnEntryCheck);
  auto *optionsHost = new QWidget(m_debuggerGroup);
  m_adapterOptionsLayout = newForm(optionsHost);
  debuggerLayout->addWidget(optionsHost);
  layout->addWidget(m_debuggerGroup);

  m_advancedToggle = new QToolButton(page);
  m_advancedToggle->setText(tr("Advanced (launch.json properties)"));
  m_advancedToggle->setCheckable(true);
  m_advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  m_advancedToggle->setArrowType(Qt::RightArrow);
  m_advancedToggle->setAutoRaise(true);
  layout->addWidget(m_advancedToggle);
  m_advancedEdit = new QPlainTextEdit(page);
  m_advancedEdit->setPlaceholderText(
      tr("Other properties sent to the debugger, as a JSON object, e.g.\n"
         "{ \"justMyCode\": false }"));
  m_advancedEdit->setTabStopDistance(20);
  m_advancedEdit->setMinimumHeight(110);
  m_advancedEdit->setVisible(false);
  layout->addWidget(m_advancedEdit);
  connect(m_advancedToggle, &QToolButton::toggled, this, [this](bool open) {
    m_advancedToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    m_advancedEdit->setVisible(open);
  });

  layout->addWidget(variablesHint(page));
  layout->addStretch();

  for (QLineEdit *edit :
       {m_nameEdit, m_programEdit, m_moduleEdit, m_runtimeArgsEdit, m_argsEdit,
        m_cwdEdit, m_processEdit, m_hostEdit, m_preLaunchEdit,
        m_postLaunchEdit}) {
    connect(edit, &QLineEdit::textChanged, this,
            &LaunchConfigurationsDialog::scheduleConfigurationPreview);
  }
  connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &LaunchConfigurationsDialog::scheduleConfigurationPreview);
  connect(m_envTable, &KeyValueTable::changed, this,
          &LaunchConfigurationsDialog::scheduleConfigurationPreview);
  connect(m_advancedEdit, &QPlainTextEdit::textChanged, this,
          &LaunchConfigurationsDialog::scheduleConfigurationPreview);
  connect(m_pythonWidget, &PythonEnvironmentWidget::preferenceChanged, this,
          &LaunchConfigurationsDialog::scheduleConfigurationPreview);
  connect(m_debuggerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this]() {
            if (m_loadingForm) {
              return;
            }
            updateConfigurationFieldVisibility();
            rebuildAdapterOptions();
            scheduleConfigurationPreview();
          });
  connect(m_requestCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this]() {
            if (m_loadingForm) {
              return;
            }
            updateConfigurationFieldVisibility();
            scheduleConfigurationPreview();
          });
  for (QLineEdit *edit : {m_programEdit, m_cwdEdit}) {
    connect(edit, &QLineEdit::textChanged, this, [this]() {
      m_pythonWidget->setContext(m_context.workspaceFolder,
                                 m_programEdit->text().trimmed(),
                                 m_cwdEdit->text().trimmed());
    });
  }
  return page;
}

QWidget *LaunchConfigurationsDialog::buildCompoundPage() {
  auto *page = new QWidget(this);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(16, 4, 12, 8);
  layout->setSpacing(12);
  auto *intro = new QLabel(
      tr("A compound starts several configurations at once, for example a "
         "server and the client that talks to it. Each runs in its own "
         "terminal or debug session."),
      page);
  intro->setWordWrap(true);
  intro->setObjectName(QStringLiteral("launchHint"));
  layout->addWidget(intro);

  auto *formHost = new QWidget(page);
  QFormLayout *form = newForm(formHost);
  m_compoundNameEdit = new QLineEdit(formHost);
  form->addRow(tr("Name"), m_compoundNameEdit);

  auto *listHost = new QWidget(formHost);
  auto *listLayout = new QHBoxLayout(listHost);
  listLayout->setContentsMargins(0, 0, 0, 0);
  listLayout->setSpacing(6);
  m_compoundList = new QListWidget(listHost);
  m_compoundList->setMinimumHeight(180);
  m_compoundList->setAccessibleName(tr("Configurations to start"));
  listLayout->addWidget(m_compoundList, 1);
  auto *order = new QVBoxLayout();
  auto *up = new QPushButton(tr("Move Up"), listHost);
  auto *down = new QPushButton(tr("Move Down"), listHost);
  order->addWidget(up);
  order->addWidget(down);
  order->addStretch();
  listLayout->addLayout(order);
  connect(up, &QPushButton::clicked, this, [this]() { moveCompoundEntry(-1); });
  connect(down, &QPushButton::clicked, this,
          [this]() { moveCompoundEntry(1); });
  form->addRow(tr("Start, in order"), listHost);

  m_compoundStopAllCheck =
      new QCheckBox(tr("Stop all of them when one is stopped"), formHost);
  form->addRow(QString(), m_compoundStopAllCheck);
  layout->addWidget(formHost);
  layout->addStretch();
  return page;
}

QWidget *LaunchConfigurationsDialog::buildEmptyPage() {
  auto *page = new QWidget(this);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->addStretch();
  auto *title = new QLabel(tr("No saved configurations yet"), page);
  title->setObjectName(QStringLiteral("launchPageTitle"));
  title->setAlignment(Qt::AlignCenter);
  layout->addWidget(title);
  auto *text = new QLabel(
      tr("Saved configurations start the same program every time, whichever "
         "file is open: a web server, a CLI with test arguments, an attach to "
         "a running process, or several of them together. Start from a "
         "template for Python, Node.js, C/C++, Rust, Go, Java and more."),
      page);
  text->setWordWrap(true);
  text->setAlignment(Qt::AlignCenter);
  text->setObjectName(QStringLiteral("launchHint"));
  layout->addWidget(text);
  auto *row = new QHBoxLayout();
  row->addStretch();
  auto *add = new QPushButton(tr("Add from Template…"), page);
  add->setObjectName(QStringLiteral("emptyAddButton"));
  connect(add, &QPushButton::clicked, this,
          &LaunchConfigurationsDialog::openTemplateGallery);
  row->addWidget(add);
  row->addStretch();
  layout->addLayout(row);
  layout->addStretch();
  return page;
}

// ------------------------------------------------------------ navigation --

void LaunchConfigurationsDialog::rebuildTree(EntryKind kind, int index) {
  {
    const QSignalBlocker blocker(m_tree);
    m_tree->clear();

    auto addSection = [this](const QString &title) {
      auto *section = new QTreeWidgetItem(m_tree, {title});
      section->setFlags(Qt::ItemIsEnabled);
      section->setData(0, KindRole, int(EntryKind::Section));
      QFont font = section->font(0);
      font.setBold(true);
      font.setPointSizeF(font.pointSizeF() * 0.92);
      section->setFont(0, font);
      section->setExpanded(true);
      return section;
    };
    auto addEntry = [](QTreeWidgetItem *section, const QString &text,
                       EntryKind entryKind, int entryIndex,
                       const QString &toolTip) {
      auto *item = new QTreeWidgetItem(section, {text});
      item->setData(0, KindRole, int(entryKind));
      item->setData(0, IndexRole, entryIndex);
      item->setToolTip(0, toolTip);
      return item;
    };

    if (!m_context.currentFilePath.isEmpty()) {
      QTreeWidgetItem *section = addSection(tr("CURRENT FILE"));
      addEntry(section, QFileInfo(m_context.currentFilePath).fileName(),
               EntryKind::File, -1, m_context.currentFilePath);
    }
    QTreeWidgetItem *configurations = addSection(tr("CONFIGURATIONS"));
    for (int i = 0; i < m_configurations.size(); ++i) {
      const DebugConfiguration &config = m_configurations.at(i);
      const bool runOnly = config.type == kRunOnlyType;
      addEntry(configurations,
               runOnly ? tr("%1  (run only)").arg(config.name) : config.name,
               EntryKind::Configuration, i,
               runOnly ? tr("Runs without a debugger")
                       : tr("Can be run or debugged"));
    }
    if (m_configurations.isEmpty()) {
      QTreeWidgetItem *placeholder =
          addEntry(configurations, tr("None yet. Click Add."), EntryKind::None,
                   -1, QString());
      QFont font = placeholder->font(0);
      font.setItalic(true);
      placeholder->setFont(0, font);
    }
    if (!m_compounds.isEmpty()) {
      QTreeWidgetItem *compounds = addSection(tr("COMPOUNDS"));
      for (int i = 0; i < m_compounds.size(); ++i) {
        addEntry(compounds, m_compounds.at(i).name, EntryKind::Compound, i,
                 m_compounds.at(i).configurations.join(QStringLiteral(", ")));
      }
    }
    m_tree->expandAll();

    QTreeWidgetItem *selected = nullptr;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
      if (kindOf(*it) == kind &&
          (kind == EntryKind::File || kind == EntryKind::None ||
           indexOf(*it) == index)) {
        selected = *it;
        break;
      }
    }
    if (!selected) {
      for (QTreeWidgetItem *section : {configurations}) {
        if (section->childCount() > 0) {
          selected = section->child(0);
        }
      }
    }
    m_tree->setCurrentItem(selected);
  }
  QTreeWidgetItem *current = m_tree->currentItem();
  showEntry(kindOf(current), indexOf(current));
}

LaunchConfigurationsDialog::EntryKind
LaunchConfigurationsDialog::kindOf(const QTreeWidgetItem *item) const {
  return item ? EntryKind(item->data(0, KindRole).toInt()) : EntryKind::None;
}

int LaunchConfigurationsDialog::indexOf(const QTreeWidgetItem *item) const {
  return item ? item->data(0, IndexRole).toInt() : -1;
}

void LaunchConfigurationsDialog::onCurrentItemChanged(
    QTreeWidgetItem *current, QTreeWidgetItem *previous) {
  if (!commitCurrentEntry()) {
    const QSignalBlocker blocker(m_tree);
    m_tree->setCurrentItem(previous);
    return;
  }
  showEntry(kindOf(current), indexOf(current));
}

void LaunchConfigurationsDialog::showEntry(EntryKind kind, int index) {
  m_currentKind = kind;
  m_currentIndex = index;
  switch (kind) {
  case EntryKind::File:
    m_pages->setCurrentIndex(0);
    scheduleFilePreview();
    break;
  case EntryKind::Configuration:
    if (index < 0 || index >= m_configurations.size()) {
      m_currentKind = EntryKind::None;
      m_pages->setCurrentIndex(3);
      break;
    }
    loadConfigurationIntoForm(m_configurations.at(index));
    m_pages->setCurrentIndex(1);
    break;
  case EntryKind::Compound:
    if (index < 0 || index >= m_compounds.size()) {
      m_currentKind = EntryKind::None;
      m_pages->setCurrentIndex(3);
      break;
    }
    loadCompoundIntoForm(m_compounds.at(index));
    m_pages->setCurrentIndex(2);
    break;
  case EntryKind::None:
  case EntryKind::Section:
    m_currentKind = EntryKind::None;
    m_pages->setCurrentIndex(3);
    break;
  }
  updateButtons();
}

bool LaunchConfigurationsDialog::commitCurrentEntry() {
  switch (m_currentKind) {
  case EntryKind::File:
    m_fileAssignment = fileAssignmentFromForm();
    break;
  case EntryKind::Configuration: {
    DebugConfiguration config;
    QString error;
    if (!configurationFromForm(&config, &error)) {
      showValidationError(error);
      return false;
    }
    if (nameInUse(config.name, EntryKind::Configuration, m_currentIndex)) {
      showValidationError(
          tr("Another entry is already named \"%1\".").arg(config.name));
      m_nameEdit->setFocus();
      return false;
    }
    const QString oldName = m_configurations.at(m_currentIndex).name;
    if (oldName != config.name) {
      // Keep compounds pointing at the renamed configuration.
      for (CompoundDebugConfiguration &compound : m_compounds) {
        for (QString &member : compound.configurations) {
          if (member == oldName) {
            member = config.name;
          }
        }
      }
    }
    m_configurations[m_currentIndex] = config;
    if (QTreeWidgetItem *item = m_tree->currentItem();
        item && kindOf(item) == EntryKind::Configuration) {
      item->setText(0, config.type == kRunOnlyType
                           ? tr("%1  (run only)").arg(config.name)
                           : config.name);
    }
    break;
  }
  case EntryKind::Compound: {
    const CompoundDebugConfiguration compound = compoundFromForm();
    if (compound.name.isEmpty()) {
      showValidationError(tr("Give the compound a name."));
      m_compoundNameEdit->setFocus();
      return false;
    }
    if (nameInUse(compound.name, EntryKind::Compound, m_currentIndex)) {
      showValidationError(
          tr("Another entry is already named \"%1\".").arg(compound.name));
      m_compoundNameEdit->setFocus();
      return false;
    }
    m_compounds[m_currentIndex] = compound;
    if (QTreeWidgetItem *item = m_tree->currentItem();
        item && kindOf(item) == EntryKind::Compound) {
      item->setText(0, compound.name);
    }
    break;
  }
  case EntryKind::None:
  case EntryKind::Section:
    break;
  }
  showValidationError(QString());
  return true;
}

void LaunchConfigurationsDialog::updateButtons() {
  const bool entry = m_currentKind == EntryKind::Configuration ||
                     m_currentKind == EntryKind::Compound;
  m_duplicateButton->setEnabled(entry);
  m_removeButton->setEnabled(entry);
  const bool launchable = m_currentKind != EntryKind::None;
  bool runnable = launchable;
  bool debuggable = launchable;
  if (m_currentKind == EntryKind::Configuration) {
    debuggable = selectedType() != kRunOnlyType;
    runnable = !debuggable || m_requestCombo->currentData().toString() !=
                                  QLatin1String("attach");
  }
  m_runButton->setEnabled(runnable);
  m_debugButton->setEnabled(debuggable);
}

void LaunchConfigurationsDialog::showValidationError(const QString &message) {
  m_validationLabel->setText(message);
  m_validationLabel->setVisible(!message.isEmpty());
}

LaunchTarget LaunchConfigurationsDialog::selectedTarget() const {
  switch (m_currentKind) {
  case EntryKind::Configuration:
    if (m_currentIndex >= 0 && m_currentIndex < m_configurations.size()) {
      return LaunchTarget::configuration(
          m_configurations.at(m_currentIndex).name);
    }
    break;
  case EntryKind::Compound:
    if (m_currentIndex >= 0 && m_currentIndex < m_compounds.size()) {
      return LaunchTarget::compound(m_compounds.at(m_currentIndex).name);
    }
    break;
  case EntryKind::File:
  case EntryKind::None:
  case EntryKind::Section:
    break;
  }
  return LaunchTarget::currentFile();
}

void LaunchConfigurationsDialog::launchSelected(LaunchMode mode) {
  if (!save()) {
    return;
  }
  m_requestedLaunch = mode;
  accept();
}

// ---------------------------------------------------------- current file --

void LaunchConfigurationsDialog::loadFileSettings() {
  if (m_context.currentFilePath.isEmpty()) {
    return;
  }
  m_loadingForm = true;
  const QFileInfo info(m_context.currentFilePath);
  m_fileTitle->setText(info.fileName());
  m_filePath->setText(QDir::toNativeSeparators(info.absoluteFilePath()));
  populateFileTemplates();
  const FileTemplateAssignment &a = m_fileAssignment;
  const int templateIndex =
      m_fileTemplateCombo->findData(a.templateId, TemplateIdRole);
  m_fileTemplateCombo->setCurrentIndex(templateIndex >= 0 ? templateIndex : 0);
  m_fileArgsEdit->setText(joinArguments(a.customArgs));
  m_fileCwdEdit->setText(a.workingDirectory);
  m_fileEnvTable->setValues(a.customEnv);
  m_fileFlagsEdit->setText(joinArguments(a.compilerFlags));
  m_fileSourcesEdit->setText(joinArguments(a.sourceFiles));
  m_filePreEdit->setText(a.preRunCommand);
  m_filePostEdit->setText(a.postRunCommand);
  m_filePythonWidget->setContext(m_context.workspaceFolder,
                                 m_context.currentFilePath, a.workingDirectory);
  m_filePythonWidget->setPreference({a.pythonMode, a.pythonInterpreter,
                                     a.pythonVenvPath,
                                     a.pythonRequirementsFile});
  m_filePythonShown = m_filePythonWidget->preference();
  const QString language = LanguageCatalog::normalize(m_context.languageId);
  m_filePythonWidget->setVisible(language == QLatin1String("py"));
  const bool compiled = language == QLatin1String("cpp") ||
                        language == QLatin1String("rust") ||
                        !a.compilerFlags.isEmpty() || !a.sourceFiles.isEmpty();
  m_fileForm->setRowVisible(m_fileFlagsEdit, compiled);
  m_fileForm->setRowVisible(m_fileSourcesEdit->parentWidget(), compiled);
  m_loadingForm = false;
  // Reflect the selected template's description.
  emit m_fileTemplateCombo->currentIndexChanged(
      m_fileTemplateCombo->currentIndex());
}

void LaunchConfigurationsDialog::populateFileTemplates() {
  const QSignalBlocker blocker(m_fileTemplateCombo);
  m_fileTemplateCombo->clear();
  RunTemplateManager &templates = RunTemplateManager::instance();
  if (templates.getAllTemplates().isEmpty()) {
    templates.loadTemplates();
  }
  const QString file = m_context.currentFilePath;

  // What Lightpad would pick without an explicit template.
  QString automatic;
  {
    FileTemplateAssignment probe = m_fileAssignment;
    probe.templateId.clear();
    RunTemplateManager::AssignmentPreview preview(file, probe);
    const RunTarget target =
        RunTargetResolver::resolve({file,
                                    m_context.languageId,
                                    m_context.workspaceFolder,
                                    {},
                                    m_context.preferredCMakeTarget});
    switch (target.kind) {
    case RunTarget::Kind::CMakeExecutable:
      automatic = target.needsTargetChoice
                      ? tr("CMake target, chosen when run")
                      : tr("CMake target %1").arg(target.cmakeTargetName);
      break;
    case RunTarget::Kind::PythonBackend:
      automatic = tr("%1 web server").arg(target.templateName);
      break;
    case RunTarget::Kind::None:
      automatic = tr("nothing found");
      break;
    case RunTarget::Kind::Template:
    case RunTarget::Kind::Pytest:
    case RunTarget::Kind::CTest:
      automatic = target.templateName.isEmpty() ? target.templateId
                                                : target.templateName;
      break;
    }
  }
  m_fileTemplateCombo->addItem(tr("Automatic: %1").arg(automatic), QString());

  QList<RunTemplate> matching = templates.getTemplatesForFilePath(file);
  for (const RunTemplate &entry :
       templates.getTemplatesForLanguageId(m_context.languageId)) {
    bool listed = false;
    for (const RunTemplate &existing : matching) {
      listed = listed || existing.id == entry.id;
    }
    if (!listed) {
      matching << entry;
    }
  }
  QSet<QString> shown;
  for (const RunTemplate &entry : matching) {
    m_fileTemplateCombo->addItem(entry.name, entry.id);
    m_fileTemplateCombo->setItemData(m_fileTemplateCombo->count() - 1,
                                     entry.description, Qt::ToolTipRole);
    shown.insert(entry.id);
  }
  QList<RunTemplate> others;
  for (const RunTemplate &entry : templates.getAllTemplates()) {
    if (!shown.contains(entry.id)) {
      others << entry;
    }
  }
  std::stable_sort(others.begin(), others.end(),
                   [](const RunTemplate &a, const RunTemplate &b) {
                     return a.language.compare(b.language,
                                               Qt::CaseInsensitive) < 0;
                   });
  if (!others.isEmpty()) {
    m_fileTemplateCombo->insertSeparator(m_fileTemplateCombo->count());
  }
  for (const RunTemplate &entry : others) {
    m_fileTemplateCombo->addItem(
        QStringLiteral("%1 — %2").arg(entry.language, entry.name), entry.id);
    m_fileTemplateCombo->setItemData(m_fileTemplateCombo->count() - 1,
                                     entry.description, Qt::ToolTipRole);
  }
  // A template that no longer exists keeps its id so saving does not drop it.
  if (!m_fileAssignment.templateId.isEmpty() &&
      m_fileTemplateCombo->findData(m_fileAssignment.templateId,
                                    TemplateIdRole) < 0) {
    m_fileTemplateCombo->addItem(
        tr("%1 (missing template)").arg(m_fileAssignment.templateId),
        m_fileAssignment.templateId);
  }
}

FileTemplateAssignment
LaunchConfigurationsDialog::fileAssignmentFromForm() const {
  FileTemplateAssignment a;
  a.filePath = m_context.currentFilePath;
  a.templateId = m_fileTemplateCombo->currentData(TemplateIdRole).toString();
  a.customArgs = splitArguments(m_fileArgsEdit->text());
  a.workingDirectory = m_fileCwdEdit->text().trimmed();
  a.customEnv = m_fileEnvTable->values();
  a.compilerFlags = splitArguments(m_fileFlagsEdit->text());
  a.sourceFiles = splitArguments(m_fileSourcesEdit->text());
  a.preRunCommand = m_filePreEdit->text().trimmed();
  a.postRunCommand = m_filePostEdit->text().trimmed();
  a.pythonMode = m_fileAssignment.pythonMode;
  a.pythonInterpreter = m_fileAssignment.pythonInterpreter;
  a.pythonVenvPath = m_fileAssignment.pythonVenvPath;
  a.pythonRequirementsFile = m_fileAssignment.pythonRequirementsFile;
  const PythonEnvironmentPreference shown = m_filePythonWidget->preference();
  if (m_filePythonWidget->isVisibleTo(m_filePage) &&
      !samePreference(shown, m_filePythonShown)) {
    a.pythonMode = shown.mode;
    a.pythonInterpreter = shown.customInterpreter;
    a.pythonVenvPath = shown.venvPath;
    a.pythonRequirementsFile = shown.requirementsFile;
  }
  return a;
}

void LaunchConfigurationsDialog::scheduleFilePreview() {
  if (!m_loadingForm && m_currentKind == EntryKind::File) {
    m_filePreviewTimer->start();
  }
}

void LaunchConfigurationsDialog::refreshFilePreview() {
  const QString file = m_context.currentFilePath;
  if (file.isEmpty()) {
    return;
  }
  const FileTemplateAssignment assignment = fileAssignmentFromForm();
  m_fileResetButton->setEnabled(!assignment.isEmpty());
  RunTemplateManager::AssignmentPreview preview(file, assignment);
  const RunTarget target =
      RunTargetResolver::resolve({file,
                                  m_context.languageId,
                                  m_context.workspaceFolder,
                                  {},
                                  m_context.preferredCMakeTarget});

  QStringList rows;
  auto row = [&rows](const QString &label, const QString &html) {
    rows << QStringLiteral("<tr><td style='padding-right:12px'><b>%1</b></td>"
                           "<td>%2</td></tr>")
                .arg(label.toHtmlEscaped(), html);
  };
  auto code = [](const QString &text) {
    return QStringLiteral("<code>%1</code>").arg(text.toHtmlEscaped());
  };

  if (!target.isValid()) {
    row(tr("Run"), target.unavailableReason.toHtmlEscaped());
  } else if (target.needsTargetChoice) {
    row(tr("Run"), tr("asks which CMake target to build").toHtmlEscaped());
  } else {
    const QString line =
        LaunchPlanner::commandLine(target.program, target.arguments);
    row(tr("Run"), line.isEmpty() ? tr("opens the file in a terminal")
                                  : code(QStringLiteral("$ ") + line));
    if (!target.workingDirectory.isEmpty()) {
      row(tr("in"), code(QDir::toNativeSeparators(target.workingDirectory)));
    }
    if (target.kind != RunTarget::Kind::Template) {
      row(QString(), target.rationale().toHtmlEscaped());
    }
  }

  const FileDebugPlan plan = LaunchPlanner::debugPlanForFile(target);
  if (!plan.isValid()) {
    row(tr("Debug"), plan.error.toHtmlEscaped());
  } else {
    const QString debugger = LaunchPlanner::debuggerNameFor(plan.configuration);
    QString what = plan.configuration.adapterConfig.value("module").toString();
    what = what.isEmpty()
               ? LaunchPlanner::commandLine(plan.configuration.program,
                                            plan.configuration.args)
               : QStringLiteral("-m ") +
                     LaunchPlanner::commandLine(what, plan.configuration.args);
    row(tr("Debug"),
        (debugger.isEmpty() ? QString()
                            : debugger.toHtmlEscaped() + QStringLiteral(": ")) +
            (what.isEmpty() ? tr("the program above").toHtmlEscaped()
                            : code(what)));
    for (const LaunchBuildStep &step : plan.buildSteps) {
      row(tr("after"), code(QStringLiteral("$ ") +
                            LaunchPlanner::commandLine(step.command.value(0),
                                                       step.command.mid(1))));
    }
    if (plan.requiresCMakeBuild) {
      row(QString(), tr("builds the CMake target first").toHtmlEscaped());
    }
  }
  m_filePreview->setText(
      QStringLiteral("<table>%1</table>").arg(rows.join(QString())));
}

void LaunchConfigurationsDialog::resetFileSettings() {
  m_fileAssignment = FileTemplateAssignment();
  m_fileAssignment.filePath = m_context.currentFilePath;
  loadFileSettings();
  scheduleFilePreview();
}

// -------------------------------------------------- saved configurations --

void LaunchConfigurationsDialog::populateDebuggerCombo(
    const DebugConfiguration &config) {
  const QSignalBlocker blocker(m_debuggerCombo);
  m_debuggerCombo->clear();

  // One entry per debugger type, plus one per adapter when a type has
  // several (C/C++: GDB or LLDB).
  QMap<QString, QList<std::shared_ptr<IDebugAdapter>>> byType;
  QStringList typeOrder;
  for (const auto &adapter : DebugAdapterRegistry::instance().allAdapters()) {
    if (!adapter) {
      continue;
    }
    const QString type = adapter->config().type;
    if (!byType.contains(type)) {
      typeOrder << type;
    }
    byType[type] << adapter;
  }
  auto addItem = [this](const QString &label, const QString &type,
                        const QString &adapterId) {
    m_debuggerCombo->addItem(label);
    const int row = m_debuggerCombo->count() - 1;
    m_debuggerCombo->setItemData(row, type, TypeRole);
    m_debuggerCombo->setItemData(row, adapterId, AdapterIdRole);
  };
  for (const QString &type : typeOrder) {
    const auto &adapters = byType.value(type);
    if (adapters.size() == 1) {
      addItem(adapters.first()->config().name, type, QString());
      continue;
    }
    QStringList variants;
    for (const auto &adapter : adapters) {
      QString name = adapter->config().name;
      const int open = name.indexOf('(');
      const int close = name.lastIndexOf(')');
      variants << (open >= 0 && close > open
                       ? name.mid(open + 1, close - open - 1)
                       : name);
    }
    addItem(
        tr("%1 (%2)").arg(adapterFamilyName(adapters.first()->config().name),
                          variants.join(tr(" or "))),
        type, QString());
    for (const auto &adapter : adapters) {
      addItem(tr("%1 only").arg(adapter->config().name), type,
              adapter->config().id);
    }
  }
  addItem(tr("None (run only)"), kRunOnlyType, QString());

  int match = -1;
  for (int i = 0; i < m_debuggerCombo->count(); ++i) {
    const QString type = m_debuggerCombo->itemData(i, TypeRole).toString();
    const QString adapterId =
        m_debuggerCombo->itemData(i, AdapterIdRole).toString();
    if (!config.adapterId.isEmpty()
            ? adapterId == config.adapterId
            : (adapterId.isEmpty() && type == config.type)) {
      match = i;
      break;
    }
  }
  if (match < 0) {
    // Keep unknown types and explicit adapter ids exactly as written.
    const auto adapter =
        DebugAdapterRegistry::instance().adapter(config.adapterId);
    addItem(adapter ? tr("%1 only").arg(adapter->config().name)
                    : tr("Custom: %1")
                          .arg(config.type.isEmpty() ? config.adapterId
                                                     : config.type),
            config.type, config.adapterId);
    match = m_debuggerCombo->count() - 1;
  }
  m_debuggerCombo->setCurrentIndex(match);
}

QString LaunchConfigurationsDialog::selectedType() const {
  return m_debuggerCombo->currentData(TypeRole).toString();
}

QString LaunchConfigurationsDialog::selectedAdapterId() const {
  return m_debuggerCombo->currentData(AdapterIdRole).toString();
}

std::shared_ptr<IDebugAdapter>
LaunchConfigurationsDialog::selectedAdapter() const {
  DebugConfiguration probe;
  probe.type = selectedType();
  probe.adapterId = selectedAdapterId();
  const auto adapters =
      DebugAdapterRegistry::instance().adaptersForConfiguration(probe);
  return adapters.isEmpty() ? nullptr : adapters.first();
}

void LaunchConfigurationsDialog::loadConfigurationIntoForm(
    const DebugConfiguration &config) {
  m_loadingForm = true;
  m_nameEdit->setText(config.name);
  populateDebuggerCombo(config);
  m_requestCombo->setCurrentIndex(
      config.request.compare("attach", Qt::CaseInsensitive) == 0 ? 1 : 0);
  m_programEdit->setText(config.program);
  m_moduleEdit->setText(config.adapterConfig.value("module").toString());
  QStringList runtimeArgs;
  for (const QJsonValue &arg :
       config.adapterConfig.value("runtimeArgs").toArray()) {
    runtimeArgs << arg.toString();
  }
  m_runtimeArgsEdit->setText(joinArguments(runtimeArgs));
  m_argsEdit->setText(joinArguments(config.args));
  m_cwdEdit->setText(config.cwd);
  m_envTable->setValues(config.env);
  m_processEdit->setText(config.processId > 0
                             ? QString::number(config.processId)
                             : config.processIdExpression);
  m_hostEdit->setText(config.host);
  m_portSpin->setValue(config.port);
  m_preLaunchEdit->setText(config.preLaunchTask);
  m_postLaunchEdit->setText(config.postDebugTask);
  m_stopOnEntryCheck->setChecked(config.stopOnEntry);
  m_pythonWidget->setContext(m_context.workspaceFolder, config.program,
                             config.cwd);
  m_pythonWidget->setPreference({config.pythonMode, config.pythonInterpreter,
                                 config.pythonVenvPath,
                                 config.pythonRequirementsFile});
  m_pythonShown = m_pythonWidget->preference();

  updateConfigurationFieldVisibility();
  m_adapterOptionsSignature.clear();
  rebuildAdapterOptions();

  // Properties with their own field are edited there; the rest stay JSON.
  QJsonObject extra = config.adapterConfig;
  const QString type = selectedType();
  if (isPythonType(type) && extra.value("module").isString()) {
    extra.remove("module");
  }
  if (isNodeType(type) && extra.value("runtimeArgs").isArray()) {
    extra.remove("runtimeArgs");
  }
  for (auto it = m_adapterOptionEdits.constBegin();
       it != m_adapterOptionEdits.constEnd(); ++it) {
    const QJsonValue value = config.adapterConfig.value(it.key());
    it.value()->setText(value.toString());
    if (value.isString()) {
      extra.remove(it.key());
    }
  }
  m_advancedEdit->setPlainText(
      extra.isEmpty() ? QString()
                      : QString::fromUtf8(QJsonDocument(extra).toJson(
                                              QJsonDocument::Indented))
                            .trimmed());
  m_advancedToggle->setChecked(!extra.isEmpty());
  m_loadingForm = false;
  refreshConfigurationPreview();
}

void LaunchConfigurationsDialog::updateConfigurationFieldVisibility() {
  const QString type = selectedType();
  const bool runOnly = type == kRunOnlyType;
  const bool attach = !runOnly && m_requestCombo->currentData().toString() ==
                                      QLatin1String("attach");
  const bool python = isPythonType(type);
  const bool node = isNodeType(type);
  const bool native = type == QLatin1String("cppdbg") ||
                      type == QLatin1String("lldb") ||
                      type == QLatin1String("go");

  m_requestCombo->setEnabled(!runOnly);
  if (runOnly && m_requestCombo->currentIndex() != 0) {
    const QSignalBlocker blocker(m_requestCombo);
    m_requestCombo->setCurrentIndex(0);
  }
  m_launchForm->setRowVisible(m_programEdit->parentWidget(), !attach || native);
  m_launchForm->setRowVisible(m_moduleEdit, python && !attach);
  m_launchForm->setRowVisible(m_runtimeArgsEdit, node && !attach);
  m_launchForm->setRowVisible(m_argsEdit, !attach);
  m_launchForm->setRowVisible(m_cwdEdit->parentWidget(), !attach);
  m_launchForm->setRowVisible(m_envTable, !attach);
  m_launchForm->setRowVisible(m_processEdit->parentWidget(), attach);
  m_launchForm->setRowVisible(m_hostEdit, attach);
  m_launchForm->setRowVisible(m_portSpin, attach);
  m_pythonWidget->setVisible(python && !attach);
  m_debuggerGroup->setVisible(!runOnly);
  m_stopOnEntryCheck->setVisible(!attach);
  m_programEdit->setPlaceholderText(
      attach   ? tr("Program with the debug symbols (optional)")
      : python ? tr("Script, e.g. ${workspaceFolder}/main.py (or set Module)")
               : tr("Executable or script, e.g. ${workspaceFolder}/build/app"));
  updateButtons();
}

void LaunchConfigurationsDialog::rebuildAdapterOptions() {
  const auto adapter = selectedAdapter();
  const QList<DebugAdapterOption> options =
      adapter ? adapter->configurationOptions() : QList<DebugAdapterOption>{};
  QStringList signature;
  for (const DebugAdapterOption &option : options) {
    signature << option.key;
  }
  if (signature.join('|') == m_adapterOptionsSignature) {
    return;
  }
  m_adapterOptionsSignature = signature.join('|');

  QMap<QString, QString> previous;
  for (auto it = m_adapterOptionEdits.constBegin();
       it != m_adapterOptionEdits.constEnd(); ++it) {
    previous.insert(it.key(), it.value()->text());
  }
  m_adapterOptionEdits.clear();
  while (m_adapterOptionsLayout->rowCount() > 0) {
    m_adapterOptionsLayout->removeRow(0);
  }
  for (const DebugAdapterOption &option : options) {
    auto *edit = new QLineEdit();
    edit->setPlaceholderText(option.placeholder);
    edit->setText(previous.value(option.key));
    connect(edit, &QLineEdit::textChanged, this,
            &LaunchConfigurationsDialog::scheduleConfigurationPreview);
    QWidget *field = option.browseable
                         ? withBrowse(edit, false, this, option.label, {})
                         : static_cast<QWidget *>(edit);
    m_adapterOptionsLayout->addRow(option.label, field);
    m_adapterOptionEdits.insert(option.key, edit);
  }
  if (m_hasSemanticStyles) {
    for (QLineEdit *edit : std::as_const(m_adapterOptionEdits)) {
      edit->setStyleSheet(m_semanticStyles.lineEdit);
    }
  }
}

bool LaunchConfigurationsDialog::configurationFromForm(DebugConfiguration *out,
                                                       QString *error) const {
  if (m_currentIndex < 0 || m_currentIndex >= m_configurations.size()) {
    return false;
  }
  DebugConfiguration config = m_configurations.at(m_currentIndex);
  const QString name = m_nameEdit->text().trimmed();
  if (!name.isEmpty()) {
    config.name = name;
  }
  config.type = selectedType();
  config.adapterId = selectedAdapterId();
  config.request = config.type == kRunOnlyType
                       ? QStringLiteral("launch")
                       : m_requestCombo->currentData().toString();
  config.program = m_programEdit->text().trimmed();
  config.args = splitArguments(m_argsEdit->text());
  config.cwd = m_cwdEdit->text().trimmed();
  config.env = m_envTable->values();
  const QString process = m_processEdit->text().trimmed();
  bool numeric = false;
  const int pid = process.toInt(&numeric);
  config.processId = numeric && pid > 0 ? pid : 0;
  config.processIdExpression = numeric ? QString() : process;
  config.host = m_hostEdit->text().trimmed();
  config.port = m_portSpin->value();
  config.preLaunchTask = m_preLaunchEdit->text().trimmed();
  config.postDebugTask = m_postLaunchEdit->text().trimmed();
  config.stopOnEntry = m_stopOnEntryCheck->isChecked();
  const PythonEnvironmentPreference p = m_pythonWidget->preference();
  if (isPythonType(config.type) && !samePreference(p, m_pythonShown)) {
    config.pythonMode = p.mode;
    config.pythonInterpreter = p.customInterpreter;
    config.pythonVenvPath = p.venvPath;
    config.pythonRequirementsFile = p.requirementsFile;
  }

  QJsonObject adapterConfig;
  const QString advanced = m_advancedEdit->toPlainText().trimmed();
  if (!advanced.isEmpty()) {
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(advanced.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
      if (error) {
        *error = tr("Advanced properties are not valid JSON: %1")
                     .arg(parseError.errorString());
      }
      return false;
    }
    if (!document.isObject()) {
      if (error) {
        *error = tr("Advanced properties must be a JSON object { … }.");
      }
      return false;
    }
    adapterConfig = document.object();
  }
  const QString module = m_moduleEdit->text().trimmed();
  if (isPythonType(config.type) && !module.isEmpty()) {
    adapterConfig["module"] = module;
  }
  const QStringList runtimeArgs = splitArguments(m_runtimeArgsEdit->text());
  if (isNodeType(config.type) && !runtimeArgs.isEmpty()) {
    adapterConfig["runtimeArgs"] = QJsonArray::fromStringList(runtimeArgs);
  }
  for (auto it = m_adapterOptionEdits.constBegin();
       it != m_adapterOptionEdits.constEnd(); ++it) {
    const QString value = it.value()->text().trimmed();
    if (!value.isEmpty()) {
      adapterConfig[it.key()] = value;
    }
  }
  config.adapterConfig = adapterConfig;
  *out = config;
  return true;
}

void LaunchConfigurationsDialog::scheduleConfigurationPreview() {
  if (!m_loadingForm && m_currentKind == EntryKind::Configuration) {
    m_configurationPreviewTimer->start();
  }
}

void LaunchConfigurationsDialog::refreshConfigurationPreview() {
  DebugConfiguration config;
  QString error;
  if (!configurationFromForm(&config, &error)) {
    m_configurationPreview->setText(error.toHtmlEscaped());
    return;
  }
  updateButtons();
  const DebugConfiguration resolved =
      DebugConfigurationManager::instance().resolveVariables(
          config, m_context.currentFilePath);

  QStringList rows;
  auto row = [&rows](const QString &label, const QString &html) {
    rows << QStringLiteral("<tr><td style='padding-right:12px'><b>%1</b></td>"
                           "<td>%2</td></tr>")
                .arg(label.toHtmlEscaped(), html);
  };
  auto code = [](const QString &text) {
    return QStringLiteral("<code>%1</code>").arg(text.toHtmlEscaped());
  };

  if (!resolved.preLaunchTask.isEmpty()) {
    row(tr("First"), code(QStringLiteral("$ ") + resolved.preLaunchTask));
  }
  const RunCommand command = LaunchPlanner::runCommandFor(resolved);
  if (command.isValid()) {
    row(tr("Run"),
        code(QStringLiteral("$ ") +
             LaunchPlanner::commandLine(command.program, command.arguments)));
    if (!resolved.cwd.isEmpty()) {
      row(tr("in"), code(QDir::toNativeSeparators(resolved.cwd)));
    }
  } else {
    row(tr("Run"), command.error.toHtmlEscaped());
  }

  if (config.type == kRunOnlyType) {
    row(tr("Debug"),
        tr("not available: no debugger is selected").toHtmlEscaped());
    m_statusTimer->stop();
    m_pendingStatusAdapter.reset();
    m_debuggerStatus->clear();
  } else {
    const auto adapter = selectedAdapter();
    if (!adapter) {
      row(tr("Debug"), tr("no registered debugger handles type \"%1\"")
                           .arg(config.type)
                           .toHtmlEscaped());
      m_statusTimer->stop();
      m_pendingStatusAdapter.reset();
      m_debuggerStatus->clear();
    } else {
      QString what;
      if (resolved.request == QLatin1String("attach")) {
        what = resolved.processId > 0
                   ? tr("attaches to process %1").arg(resolved.processId)
               : resolved.processIdExpression == kPickProcess
                   ? tr("attaches to a process you pick")
               : resolved.port > 0 ? tr("connects to %1:%2")
                                         .arg(resolved.host.isEmpty()
                                                  ? QStringLiteral("localhost")
                                                  : resolved.host)
                                         .arg(resolved.port)
                                   : tr("attaches");
      } else {
        what = tr("launches the program above");
      }
      row(tr("Debug"),
          QStringLiteral("%1 %2").arg(adapter->config().name.toHtmlEscaped(),
                                      what.toHtmlEscaped()));

      // Probing a debugger starts a process, so it is debounced and cached.
      const QString key =
          adapter->config().id + QLatin1Char('|') +
          QString::fromUtf8(
              QJsonDocument(resolved.toJson()).toJson(QJsonDocument::Compact));
      const auto cached = m_statusCache.constFind(key);
      if (cached != m_statusCache.constEnd()) {
        m_statusTimer->stop();
        m_pendingStatusAdapter.reset();
        m_debuggerStatus->setText(tr("Debugger: %1").arg(cached.value()));
      } else {
        m_pendingStatusAdapter = adapter;
        m_pendingStatusConfiguration = resolved;
        m_pendingStatusKey = key;
        m_debuggerStatus->setText(tr("Debugger: checking…"));
        m_statusTimer->start();
      }
    }
  }
  m_configurationPreview->setText(
      QStringLiteral("<table>%1</table>").arg(rows.join(QString())));
}

void LaunchConfigurationsDialog::refreshDebuggerStatus() {
  if (!m_pendingStatusAdapter) {
    return;
  }
  const auto adapter = m_pendingStatusAdapter;
  m_pendingStatusAdapter.reset();
  const QString status =
      adapter->statusMessageForConfiguration(m_pendingStatusConfiguration);
  m_statusCache.insert(m_pendingStatusKey, status);
  m_debuggerStatus->setText(tr("Debugger: %1").arg(status));
}

void LaunchConfigurationsDialog::addConfiguration(
    const DebugConfiguration &config) {
  if (!commitCurrentEntry()) {
    return;
  }
  DebugConfiguration added = config;
  added.name = uniqueName(config.name);
  m_configurations << added;
  rebuildTree(EntryKind::Configuration, m_configurations.size() - 1);
  m_nameEdit->setFocus();
  m_nameEdit->selectAll();
}

QString
LaunchConfigurationsDialog::addConfigurationFromTemplate(const QString &id) {
  const LaunchTemplate entry =
      LaunchTemplateCatalog::instance().templateById(id);
  if (!entry.isValid()) {
    return {};
  }
  const QString name = uniqueName(entry.name);
  addConfiguration(entry.instantiate(name));
  return m_configurations.isEmpty() ? QString() : m_configurations.last().name;
}

void LaunchConfigurationsDialog::openTemplateGallery() {
  LaunchTemplateGallery gallery(m_context.languageId, m_context.workspaceFolder,
                                this);
  gallery.applyTheme(m_theme);
  if (gallery.exec() == QDialog::Accepted) {
    addConfigurationFromTemplate(gallery.selectedTemplateId());
  }
}

void LaunchConfigurationsDialog::populateAddMenu() {
  m_addMenu->clear();
  const QList<LaunchTemplate> suggestions =
      LaunchTemplateCatalog::instance().suggestionsFor(
          m_context.languageId, m_context.workspaceFolder);
  if (!suggestions.isEmpty()) {
    QAction *heading = m_addMenu->addAction(tr("Suggested"));
    heading->setEnabled(false);
    for (int i = 0; i < suggestions.size() && i < 5; ++i) {
      const LaunchTemplate &entry = suggestions.at(i);
      QAction *action = m_addMenu->addAction(entry.name);
      action->setToolTip(entry.description);
      connect(action, &QAction::triggered, this,
              [this, id = entry.id]() { addConfigurationFromTemplate(id); });
    }
    m_addMenu->addSeparator();
  }
  QAction *all = m_addMenu->addAction(tr("From Template…"));
  connect(all, &QAction::triggered, this,
          &LaunchConfigurationsDialog::openTemplateGallery);
  if (!m_context.currentFilePath.isEmpty()) {
    QAction *fromFile = m_addMenu->addAction(
        tr("From How %1 Runs")
            .arg(QFileInfo(m_context.currentFilePath).fileName()));
    fromFile->setToolTip(tr("Save the current file's settings as a "
                            "configuration that always starts this file"));
    connect(fromFile, &QAction::triggered, this, [this]() {
      if (!commitCurrentEntry()) {
        return;
      }
      const QString file = m_context.currentFilePath;
      RunTemplateManager::AssignmentPreview preview(file, m_fileAssignment);
      const RunTarget target =
          RunTargetResolver::resolve({file,
                                      m_context.languageId,
                                      m_context.workspaceFolder,
                                      {},
                                      m_context.preferredCMakeTarget});
      const FileDebugPlan plan = LaunchPlanner::debugPlanForFile(target);
      DebugConfiguration config;
      if (plan.isValid() && plan.buildSteps.isEmpty() &&
          !plan.requiresCMakeBuild) {
        config = plan.configuration;
      } else {
        config.type = kRunOnlyType;
        config.request = QStringLiteral("launch");
        config.program = target.program;
        config.args = target.arguments;
        config.cwd = target.workingDirectory;
        config.env = target.environment;
      }
      config.preLaunchTask = m_fileAssignment.preRunCommand;
      config.postDebugTask = m_fileAssignment.postRunCommand;
      config.name = QFileInfo(file).fileName();
      addConfiguration(config);
    });
  }
  m_addMenu->addSeparator();
  QAction *compound = m_addMenu->addAction(tr("Compound…"));
  compound->setToolTip(tr("Start several configurations together"));
  compound->setEnabled(!m_configurations.isEmpty());
  connect(compound, &QAction::triggered, this,
          &LaunchConfigurationsDialog::addCompound);
}

void LaunchConfigurationsDialog::duplicateCurrent() {
  if (!commitCurrentEntry()) {
    return;
  }
  if (m_currentKind == EntryKind::Configuration) {
    DebugConfiguration copy = m_configurations.at(m_currentIndex);
    copy.name = tr("%1 (copy)").arg(copy.name);
    addConfiguration(copy);
  } else if (m_currentKind == EntryKind::Compound) {
    CompoundDebugConfiguration copy = m_compounds.at(m_currentIndex);
    copy.name = uniqueName(tr("%1 (copy)").arg(copy.name));
    m_compounds << copy;
    rebuildTree(EntryKind::Compound, m_compounds.size() - 1);
  }
}

void LaunchConfigurationsDialog::removeCurrent() {
  if (m_currentKind == EntryKind::Configuration) {
    const QString name = m_configurations.at(m_currentIndex).name;
    m_configurations.removeAt(m_currentIndex);
    for (CompoundDebugConfiguration &compound : m_compounds) {
      compound.configurations.removeAll(name);
    }
    const int next = qMin(m_currentIndex, m_configurations.size() - 1);
    m_currentKind = EntryKind::None;
    rebuildTree(EntryKind::Configuration, next);
  } else if (m_currentKind == EntryKind::Compound) {
    m_compounds.removeAt(m_currentIndex);
    const int next = qMin(m_currentIndex, m_compounds.size() - 1);
    m_currentKind = EntryKind::None;
    rebuildTree(next >= 0 ? EntryKind::Compound : EntryKind::Configuration,
                next >= 0 ? next : 0);
  }
}

// ------------------------------------------------------------- compounds --

void LaunchConfigurationsDialog::addCompound() {
  if (!commitCurrentEntry()) {
    return;
  }
  CompoundDebugConfiguration compound;
  compound.name = uniqueName(tr("All Together"));
  for (const DebugConfiguration &config : m_configurations) {
    compound.configurations << config.name;
  }
  m_compounds << compound;
  rebuildTree(EntryKind::Compound, m_compounds.size() - 1);
  m_compoundNameEdit->setFocus();
  m_compoundNameEdit->selectAll();
}

void LaunchConfigurationsDialog::loadCompoundIntoForm(
    const CompoundDebugConfiguration &compound) {
  m_compoundNameEdit->setText(compound.name);
  m_compoundStopAllCheck->setChecked(compound.stopAll);
  m_compoundList->clear();
  QStringList order = compound.configurations;
  for (const DebugConfiguration &config : m_configurations) {
    if (!order.contains(config.name)) {
      order << config.name;
    }
  }
  for (const QString &name : order) {
    auto *item = new QListWidgetItem(name, m_compoundList);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(compound.configurations.contains(name) ? Qt::Checked
                                                               : Qt::Unchecked);
  }
}

CompoundDebugConfiguration
LaunchConfigurationsDialog::compoundFromForm() const {
  CompoundDebugConfiguration compound;
  compound.name = m_compoundNameEdit->text().trimmed();
  compound.stopAll = m_compoundStopAllCheck->isChecked();
  for (int i = 0; i < m_compoundList->count(); ++i) {
    const QListWidgetItem *item = m_compoundList->item(i);
    if (item->checkState() == Qt::Checked) {
      compound.configurations << item->text();
    }
  }
  return compound;
}

void LaunchConfigurationsDialog::moveCompoundEntry(int delta) {
  const int row = m_compoundList->currentRow();
  const int target = row + delta;
  if (row < 0 || target < 0 || target >= m_compoundList->count()) {
    return;
  }
  QListWidgetItem *item = m_compoundList->takeItem(row);
  m_compoundList->insertItem(target, item);
  m_compoundList->setCurrentRow(target);
}

// ------------------------------------------------------------------ save --

bool LaunchConfigurationsDialog::nameInUse(const QString &name,
                                           EntryKind except,
                                           int exceptIndex) const {
  for (int i = 0; i < m_configurations.size(); ++i) {
    if (!(except == EntryKind::Configuration && i == exceptIndex) &&
        m_configurations.at(i).name == name) {
      return true;
    }
  }
  for (int i = 0; i < m_compounds.size(); ++i) {
    if (!(except == EntryKind::Compound && i == exceptIndex) &&
        m_compounds.at(i).name == name) {
      return true;
    }
  }
  return false;
}

QString LaunchConfigurationsDialog::uniqueName(const QString &baseName) const {
  const QString base =
      baseName.trimmed().isEmpty() ? tr("New Configuration") : baseName;
  QString candidate = base;
  int suffix = 2;
  while (nameInUse(candidate, EntryKind::None, -1)) {
    candidate = QStringLiteral("%1 (%2)").arg(base).arg(suffix++);
  }
  return candidate;
}

bool LaunchConfigurationsDialog::save() {
  if (!commitCurrentEntry()) {
    return false;
  }
  for (const CompoundDebugConfiguration &compound : m_compounds) {
    if (compound.configurations.isEmpty()) {
      showValidationError(
          tr("Compound \"%1\" does not start anything; tick at least one "
             "configuration.")
              .arg(compound.name));
      return false;
    }
  }

  DebugConfigurationManager &configs = DebugConfigurationManager::instance();
  auto sameConfigurations = [&configs, this]() {
    const QList<DebugConfiguration> saved = configs.allConfigurations();
    const QList<CompoundDebugConfiguration> compounds =
        configs.allCompoundConfigurations();
    if (saved.size() != m_configurations.size() ||
        compounds.size() != m_compounds.size()) {
      return false;
    }
    for (int i = 0; i < saved.size(); ++i) {
      if (saved.at(i).toJson() != m_configurations.at(i).toJson()) {
        return false;
      }
    }
    for (int i = 0; i < compounds.size(); ++i) {
      if (compounds.at(i).toJson() != m_compounds.at(i).toJson()) {
        return false;
      }
    }
    return true;
  };
  if (!sameConfigurations()) {
    const QList<DebugConfiguration> previous = configs.allConfigurations();
    const QList<CompoundDebugConfiguration> previousCompounds =
        configs.allCompoundConfigurations();
    configs.replaceConfigurations(m_configurations);
    configs.replaceCompoundConfigurations(m_compounds);
    if (!configs.saveToLightpadDir()) {
      configs.replaceConfigurations(previous);
      configs.replaceCompoundConfigurations(previousCompounds);
      showValidationError(
          tr("Could not write %1.").arg(configs.lightpadLaunchConfigPath()));
      return false;
    }
  }

  const QString file = m_context.currentFilePath;
  if (!file.isEmpty()) {
    RunTemplateManager &templates = RunTemplateManager::instance();
    const FileTemplateAssignment saved = templates.getAssignmentForFile(file);
    FileTemplateAssignment edited = m_fileAssignment;
    edited.filePath = file;
    const bool unchanged =
        saved.templateId == edited.templateId &&
        saved.customArgs == edited.customArgs &&
        saved.customEnv == edited.customEnv &&
        saved.sourceFiles == edited.sourceFiles &&
        saved.workingDirectory == edited.workingDirectory &&
        saved.compilerFlags == edited.compilerFlags &&
        saved.preRunCommand == edited.preRunCommand &&
        saved.postRunCommand == edited.postRunCommand &&
        saved.pythonMode == edited.pythonMode &&
        saved.pythonInterpreter == edited.pythonInterpreter &&
        saved.pythonVenvPath == edited.pythonVenvPath &&
        saved.pythonRequirementsFile == edited.pythonRequirementsFile;
    if (!unchanged) {
      const bool ok = edited.isEmpty()
                          ? templates.removeAssignment(file)
                          : templates.assignTemplateToFile(file, edited);
      if (!ok) {
        showValidationError(tr("Could not save the settings for %1.")
                                .arg(QFileInfo(file).fileName()));
        return false;
      }
    }
  }
  showValidationError(QString());
  return true;
}

// ----------------------------------------------------------------- theme --

void LaunchConfigurationsDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);
  stylePrimaryButton(m_saveButton);
  styleDangerButton(m_removeButton);
  m_validationLabel->setStyleSheet(UIStyleHelper::errorInfoLabelStyle(theme));
  for (QLabel *label : findChildren<QLabel *>(QStringLiteral("launchHint"))) {
    styleSubduedLabel(label);
  }
  for (QLabel *label :
       findChildren<QLabel *>(QStringLiteral("launchPageSubtitle"))) {
    styleSubduedLabel(label);
  }
  for (QLabel *label :
       findChildren<QLabel *>(QStringLiteral("launchPageTitle"))) {
    styleTitleLabel(label);
  }
  for (QLabel *label :
       findChildren<QLabel *>(QStringLiteral("launchSectionTitle"))) {
    styleSectionLabel(label);
  }
  if (auto *add = findChild<QPushButton *>(QStringLiteral("emptyAddButton"))) {
    stylePrimaryButton(add);
  }
  const QColor fill = theme.surfaceAltColor.isValid() ? theme.surfaceAltColor
                                                      : theme.surfaceColor;
  const QString previewStyle =
      QStringLiteral("QLabel { padding: 10px; border-radius: 6px; "
                     "background: %1; color: %2; border: 1px solid %3; }")
          .arg(fill.name(),
               UIStyleHelper::readableText(theme, fill, theme.foregroundColor)
                   .name(),
               theme.borderColor.name());
  m_filePreview->setStyleSheet(previewStyle);
  m_configurationPreview->setStyleSheet(previewStyle);
  m_filePythonWidget->applyTheme(theme);
  m_pythonWidget->applyTheme(theme);
  m_advancedToggle->setStyleSheet(
      QStringLiteral("QToolButton { border: none; color: %1; padding: 2px; }")
          .arg(theme.foregroundColor.name()));
}
