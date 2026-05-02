#include "testconfigurationdialog.h"

#include "themedmessagebox.h"

#include <QGridLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QListView>
#include <QMenu>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>
#include <QUuid>

namespace {
constexpr int ConfigIdRole = Qt::UserRole;
constexpr int IsUserConfigRole = Qt::UserRole + 1;

void configureFormLayout(QFormLayout *layout) {
  if (!layout)
    return;

  layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  layout->setFormAlignment(Qt::AlignTop);
  layout->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
  layout->setHorizontalSpacing(16);
  layout->setVerticalSpacing(12);
}
}

TestConfigurationDialog::TestConfigurationDialog(QWidget *parent,
                                                 const QString &initialConfigId)
    : StyledDialog(parent), m_initialConfigId(initialConfigId) {
  setupUi();
  populateTemplateCombo();
  reloadConfigurationList(m_initialConfigId);
}

void TestConfigurationDialog::setupUi() {
  setWindowTitle(tr("Test Configurations"));
  setMinimumSize(860, 720);
  resize(960, 760);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(14, 14, 14, 10);
  mainLayout->setSpacing(12);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setChildrenCollapsible(false);

  auto *leftPanel = new QWidget(splitter);
  auto *leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(10);
  leftPanel->setMinimumWidth(285);

  auto *listLabel = new QLabel(tr("<b>Configurations</b>"), leftPanel);
  leftLayout->addWidget(listLabel);

  m_configList = new QListWidget(leftPanel);
  m_configList->setMinimumWidth(285);
  m_configList->setAlternatingRowColors(false);
  connect(m_configList, &QListWidget::currentItemChanged, this,
          &TestConfigurationDialog::onConfigSelected);
  leftLayout->addWidget(m_configList, 1);

  auto *buttonLayout = new QGridLayout();
  buttonLayout->setContentsMargins(0, 6, 0, 0);
  buttonLayout->setHorizontalSpacing(8);
  buttonLayout->setVerticalSpacing(8);
  m_addConfigBtn = new QPushButton(tr("Add"), leftPanel);
  m_addTemplateBtn = new QPushButton(tr("Template"), leftPanel);
  m_duplicateConfigBtn = new QPushButton(tr("Duplicate"), leftPanel);
  m_removeConfigBtn = new QPushButton(tr("Remove"), leftPanel);
  m_addConfigBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_addTemplateBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_duplicateConfigBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_removeConfigBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  connect(m_addConfigBtn, &QPushButton::clicked, this,
          &TestConfigurationDialog::onAddConfiguration);
  connect(m_duplicateConfigBtn, &QPushButton::clicked, this,
          &TestConfigurationDialog::onDuplicateConfiguration);
  connect(m_removeConfigBtn, &QPushButton::clicked, this,
          &TestConfigurationDialog::onRemoveConfiguration);

  auto *templateMenu = new QMenu(m_addTemplateBtn);
  for (const TestConfiguration &cfg :
       TestConfigurationManager::instance().allTemplates()) {
    QAction *action = templateMenu->addAction(cfg.name);
    connect(action, &QAction::triggered, this,
             [this, templateId = cfg.id]() { onAddFromTemplate(templateId); });
  }
  m_addTemplateBtn->setMenu(templateMenu);

  buttonLayout->addWidget(m_addConfigBtn, 0, 0);
  buttonLayout->addWidget(m_addTemplateBtn, 0, 1);
  buttonLayout->addWidget(m_duplicateConfigBtn, 1, 0);
  buttonLayout->addWidget(m_removeConfigBtn, 1, 1);
  leftLayout->addLayout(buttonLayout);

  splitter->addWidget(leftPanel);

  auto *scrollArea = new QScrollArea(splitter);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);

  auto *rightPanel = new QWidget(scrollArea);
  auto *rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(4, 0, 4, 0);
  rightLayout->setSpacing(12);

  m_originLabel = new QLabel(rightPanel);
  m_originLabel->setWordWrap(true);
  rightLayout->addWidget(m_originLabel);

  auto *basicGroup = new QGroupBox(tr("Basic Settings"), rightPanel);
  auto *basicLayout = new QFormLayout(basicGroup);
  configureFormLayout(basicLayout);

  m_nameEdit = new QLineEdit(basicGroup);
  m_nameEdit->setPlaceholderText(tr("Configuration name"));
  basicLayout->addRow(tr("Name:"), m_nameEdit);

  m_templateCombo = new QComboBox(basicGroup);
  m_templateCombo->setView(new QListView(m_templateCombo));
  m_templateCombo->setMaxVisibleItems(12);
  basicLayout->addRow(tr("Base template:"), m_templateCombo);

  m_defaultCheck = new QCheckBox(tr("Use as default test configuration"),
                                 basicGroup);
  basicLayout->addRow(QString(), m_defaultCheck);

  m_languageEdit = new QLineEdit(basicGroup);
  m_languageEdit->setPlaceholderText(tr("Language hint (for example Python)"));
  basicLayout->addRow(tr("Language:"), m_languageEdit);

  m_extensionsEdit = new QLineEdit(basicGroup);
  m_extensionsEdit->setPlaceholderText(tr("Comma-separated extensions"));
  basicLayout->addRow(tr("Extensions:"), m_extensionsEdit);

  m_outputFormatCombo = new QComboBox(basicGroup);
  m_outputFormatCombo->setEditable(true);
  m_outputFormatCombo->setInsertPolicy(QComboBox::NoInsert);
  m_outputFormatCombo->setView(new QListView(m_outputFormatCombo));
  m_outputFormatCombo->setMaxVisibleItems(8);
  m_outputFormatCombo->setMinimumContentsLength(14);
  m_outputFormatCombo->addItems(
      {"generic", "pytest", "ctest", "go_json", "cargo_json", "jest_json"});
  basicLayout->addRow(tr("Output format:"), m_outputFormatCombo);

  m_testFilePatternEdit = new QLineEdit(basicGroup);
  m_testFilePatternEdit->setPlaceholderText(tr("Optional discovery hint"));
  basicLayout->addRow(tr("Test file pattern:"), m_testFilePatternEdit);

  rightLayout->addWidget(basicGroup);

  auto *executionGroup = new QGroupBox(tr("Execution"), rightPanel);
  auto *executionLayout = new QFormLayout(executionGroup);
  configureFormLayout(executionLayout);

  m_commandEdit = new QLineEdit(executionGroup);
  m_commandEdit->setPlaceholderText(tr("Executable or command to run"));
  executionLayout->addRow(tr("Command:"), m_commandEdit);

  m_argsEdit = new QPlainTextEdit(executionGroup);
  m_argsEdit->setMinimumHeight(96);
  m_argsEdit->setMaximumHeight(120);
  m_argsEdit->setPlaceholderText(
      tr("One argument per line.\nVariables like ${workspaceFolder} are "
         "supported."));
  executionLayout->addRow(tr("Arguments:"), m_argsEdit);

  m_workingDirectoryEdit = new QLineEdit(executionGroup);
  m_workingDirectoryEdit->setPlaceholderText(tr("${workspaceFolder}"));
  executionLayout->addRow(tr("Working directory:"), m_workingDirectoryEdit);

  m_discoveryDirectoryEdit = new QLineEdit(executionGroup);
  m_discoveryDirectoryEdit->setPlaceholderText(tr("Optional discovery root"));
  executionLayout->addRow(tr("Discovery directory:"),
                          m_discoveryDirectoryEdit);

  rightLayout->addWidget(executionGroup);

  auto *overrideGroup = new QGroupBox(tr("Run Overrides"), rightPanel);
  auto *overrideLayout = new QFormLayout(overrideGroup);
  configureFormLayout(overrideLayout);

  m_runFileArgsEdit = new QPlainTextEdit(overrideGroup);
  m_runFileArgsEdit->setMinimumHeight(70);
  m_runFileArgsEdit->setMaximumHeight(92);
  m_runFileArgsEdit->setPlaceholderText(tr("Override args for Run File"));
  overrideLayout->addRow(tr("Run file args:"), m_runFileArgsEdit);

  m_runSingleTestArgsEdit = new QPlainTextEdit(overrideGroup);
  m_runSingleTestArgsEdit->setMinimumHeight(70);
  m_runSingleTestArgsEdit->setMaximumHeight(92);
  m_runSingleTestArgsEdit->setPlaceholderText(
      tr("Override args for Run Test"));
  overrideLayout->addRow(tr("Run test args:"), m_runSingleTestArgsEdit);

  m_runFailedArgsEdit = new QPlainTextEdit(overrideGroup);
  m_runFailedArgsEdit->setMinimumHeight(70);
  m_runFailedArgsEdit->setMaximumHeight(92);
  m_runFailedArgsEdit->setPlaceholderText(
      tr("Override args for Run Failed"));
  overrideLayout->addRow(tr("Run failed args:"), m_runFailedArgsEdit);

  m_runSuiteArgsEdit = new QPlainTextEdit(overrideGroup);
  m_runSuiteArgsEdit->setMinimumHeight(70);
  m_runSuiteArgsEdit->setMaximumHeight(92);
  m_runSuiteArgsEdit->setPlaceholderText(tr("Override args for Run Suite"));
  overrideLayout->addRow(tr("Run suite args:"), m_runSuiteArgsEdit);

  rightLayout->addWidget(overrideGroup);

  auto *envGroup = new QGroupBox(tr("Environment Variables"), rightPanel);
  auto *envLayout = new QVBoxLayout(envGroup);

  m_envTable = new QTableWidget(0, 2, envGroup);
  m_envTable->setHorizontalHeaderLabels({tr("Variable"), tr("Value")});
  m_envTable->horizontalHeader()->setStretchLastSection(true);
  m_envTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_envTable->verticalHeader()->setVisible(false);
  m_envTable->setMaximumHeight(160);
  envLayout->addWidget(m_envTable);

  auto *envButtonLayout = new QHBoxLayout();
  m_addEnvBtn = new QPushButton(tr("Add"), envGroup);
  m_removeEnvBtn = new QPushButton(tr("Remove"), envGroup);
  connect(m_addEnvBtn, &QPushButton::clicked, this,
          &TestConfigurationDialog::onAddEnvVar);
  connect(m_removeEnvBtn, &QPushButton::clicked, this,
          &TestConfigurationDialog::onRemoveEnvVar);
  envButtonLayout->addWidget(m_addEnvBtn);
  envButtonLayout->addWidget(m_removeEnvBtn);
  envButtonLayout->addStretch();
  envLayout->addLayout(envButtonLayout);

  rightLayout->addWidget(envGroup);

  auto *tasksGroup = new QGroupBox(tr("Tasks"), rightPanel);
  auto *tasksLayout = new QFormLayout(tasksGroup);
  configureFormLayout(tasksLayout);

  m_preLaunchTaskEdit = new QLineEdit(tasksGroup);
  m_preLaunchTaskEdit->setPlaceholderText(
      tr("Optional task to run before the test command"));
  tasksLayout->addRow(tr("Pre-launch task:"), m_preLaunchTaskEdit);

  m_postRunTaskEdit = new QLineEdit(tasksGroup);
  m_postRunTaskEdit->setPlaceholderText(
      tr("Optional task to run after the test command"));
  tasksLayout->addRow(tr("Post-run task:"), m_postRunTaskEdit);

  rightLayout->addWidget(tasksGroup);
  rightLayout->addStretch();

  scrollArea->setWidget(rightPanel);
  splitter->addWidget(scrollArea);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);
  splitter->setSizes({290, 670});

  mainLayout->addWidget(splitter, 1);

  auto *footerLayout = new QHBoxLayout();
  footerLayout->addStretch();
  m_saveButton = new QPushButton(tr("Save"), this);
  m_saveButton->setDefault(true);
  connect(m_saveButton, &QPushButton::clicked, this,
          &TestConfigurationDialog::onSave);
  footerLayout->addWidget(m_saveButton);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  footerLayout->addWidget(m_cancelButton);

  mainLayout->addLayout(footerLayout);
}

void TestConfigurationDialog::populateTemplateCombo() {
  m_templateCombo->clear();
  m_templateCombo->addItem(tr("Custom"), QString());
  for (const TestConfiguration &cfg :
       TestConfigurationManager::instance().allTemplates()) {
    m_templateCombo->addItem(cfg.name, cfg.id);
  }
}

void TestConfigurationDialog::reloadConfigurationList(const QString &selectedId) {
  QSignalBlocker blocker(m_configList);
  m_configList->clear();

  const TestConfigurationManager &manager = TestConfigurationManager::instance();
  const QList<TestConfiguration> configs = manager.allConfigurations();
  int selectedRow = -1;
  for (int i = 0; i < configs.size(); ++i) {
    const TestConfiguration &cfg = configs[i];
    const bool isUserConfig = manager.hasUserConfiguration(cfg.id);
    const QString suffix = isUserConfig ? QString() : tr(" (Built-in)");
    auto *item = new QListWidgetItem(cfg.name + suffix, m_configList);
    item->setData(ConfigIdRole, cfg.id);
    item->setData(IsUserConfigRole, isUserConfig);
    if (!selectedId.isEmpty() && cfg.id == selectedId)
      selectedRow = i;
  }

  if (selectedRow < 0 && m_configList->count() > 0)
    selectedRow = 0;

  if (selectedRow >= 0) {
    m_configList->setCurrentRow(selectedRow);
    onConfigSelected(m_configList->currentItem(), nullptr);
  } else {
    clearForm();
  }
}

void TestConfigurationDialog::onConfigSelected(QListWidgetItem *current,
                                               QListWidgetItem *previous) {
  Q_UNUSED(previous)
  if (!current) {
    clearForm();
    return;
  }

  const QString id = current->data(ConfigIdRole).toString();
  const bool isUserConfig = current->data(IsUserConfigRole).toBool();
  const TestConfiguration cfg =
      TestConfigurationManager::instance().configurationById(id);
  loadConfigurationIntoForm(cfg, isUserConfig, false);
}

void TestConfigurationDialog::loadConfigurationIntoForm(
    const TestConfiguration &cfg, bool isUserConfig, bool isDraftConfig) {
  m_currentConfigId = cfg.id;
  m_currentIsUserConfig = isUserConfig;
  m_currentIsDraftConfig = isDraftConfig;
  m_loadedConfig = cfg;

  m_nameEdit->setText(cfg.name);
  const QString baseTemplateId = cfg.templateId;
  int templateIndex = m_templateCombo->findData(baseTemplateId);
  m_templateCombo->setCurrentIndex(templateIndex >= 0 ? templateIndex : 0);

  {
    QSignalBlocker blocker(m_defaultCheck);
    m_defaultCheck->setChecked(
        TestConfigurationManager::instance().defaultConfigurationId() == cfg.id);
  }

  m_languageEdit->setText(cfg.language);
  m_extensionsEdit->setText(cfg.extensions.join(", "));
  m_outputFormatCombo->setCurrentText(cfg.outputFormat);
  m_testFilePatternEdit->setText(cfg.testFilePattern);
  m_commandEdit->setText(cfg.command);
  setArgs(m_argsEdit, cfg.args);
  m_workingDirectoryEdit->setText(cfg.workingDirectory);
  m_discoveryDirectoryEdit->setText(cfg.discoveryDirectory);
  setArgs(m_runFileArgsEdit, cfg.runFile.args);
  setArgs(m_runSingleTestArgsEdit, cfg.runSingleTest.args);
  setArgs(m_runFailedArgsEdit, cfg.runFailed.args);
  setArgs(m_runSuiteArgsEdit, cfg.runSuite.args);

  m_envTable->setRowCount(0);
  int row = 0;
  for (auto it = cfg.env.begin(); it != cfg.env.end(); ++it) {
    m_envTable->insertRow(row);
    m_envTable->setItem(row, 0, new QTableWidgetItem(it.key()));
    m_envTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    ++row;
  }

  m_preLaunchTaskEdit->setText(cfg.preLaunchTask);
  m_postRunTaskEdit->setText(cfg.postRunTask);

  setOriginText();
}

void TestConfigurationDialog::clearForm() {
  m_currentConfigId.clear();
  m_currentIsUserConfig = false;
  m_currentIsDraftConfig = false;
  m_loadedConfig = TestConfiguration();
  m_nameEdit->clear();
  m_templateCombo->setCurrentIndex(0);
  {
    QSignalBlocker blocker(m_defaultCheck);
    m_defaultCheck->setChecked(false);
  }
  m_languageEdit->clear();
  m_extensionsEdit->clear();
  m_outputFormatCombo->setCurrentText("generic");
  m_testFilePatternEdit->clear();
  m_commandEdit->clear();
  m_argsEdit->clear();
  m_workingDirectoryEdit->clear();
  m_discoveryDirectoryEdit->clear();
  m_runFileArgsEdit->clear();
  m_runSingleTestArgsEdit->clear();
  m_runFailedArgsEdit->clear();
  m_runSuiteArgsEdit->clear();
  m_envTable->setRowCount(0);
  m_preLaunchTaskEdit->clear();
  m_postRunTaskEdit->clear();
  setOriginText();
}

TestConfiguration TestConfigurationDialog::formConfiguration() const {
  TestConfiguration cfg;
  cfg.id = m_currentConfigId.isEmpty()
               ? QUuid::createUuid().toString(QUuid::WithoutBraces)
               : m_currentConfigId;
  cfg.name = m_nameEdit->text().trimmed();
  cfg.templateId = m_templateCombo->currentData().toString().trimmed();
  cfg.language = m_languageEdit->text().trimmed();
  cfg.extensions = parseExtensions(m_extensionsEdit->text());
  cfg.outputFormat = m_outputFormatCombo->currentText().trimmed();
  if (cfg.outputFormat.isEmpty())
    cfg.outputFormat = "generic";
  cfg.testFilePattern = m_testFilePatternEdit->text().trimmed();
  cfg.command = m_commandEdit->text().trimmed();
  cfg.args = parseArgs(m_argsEdit);
  cfg.workingDirectory = m_workingDirectoryEdit->text().trimmed();
  cfg.discoveryDirectory = m_discoveryDirectoryEdit->text().trimmed();
  cfg.runFile.args = parseArgs(m_runFileArgsEdit);
  cfg.runSingleTest.args = parseArgs(m_runSingleTestArgsEdit);
  cfg.runFailed.args = parseArgs(m_runFailedArgsEdit);
  cfg.runSuite.args = parseArgs(m_runSuiteArgsEdit);
  cfg.preLaunchTask = m_preLaunchTaskEdit->text().trimmed();
  cfg.postRunTask = m_postRunTaskEdit->text().trimmed();

  for (int row = 0; row < m_envTable->rowCount(); ++row) {
    const auto *keyItem = m_envTable->item(row, 0);
    const auto *valueItem = m_envTable->item(row, 1);
    if (!keyItem || keyItem->text().trimmed().isEmpty())
      continue;
    cfg.env[keyItem->text().trimmed()] =
        valueItem ? valueItem->text().trimmed() : QString();
  }

  return cfg;
}

QStringList TestConfigurationDialog::parseArgs(QPlainTextEdit *edit) const {
  QStringList args;
  const QStringList lines =
      edit->toPlainText().split(QRegularExpression("[\r\n]+"),
                                Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty())
      args.append(trimmed);
  }
  return args;
}

void TestConfigurationDialog::setArgs(QPlainTextEdit *edit,
                                      const QStringList &args) {
  edit->setPlainText(args.join("\n"));
}

QStringList TestConfigurationDialog::parseExtensions(const QString &text) const {
  QStringList result;
  const QStringList parts =
      text.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
  for (QString ext : parts) {
    ext = ext.trimmed();
    while (ext.startsWith('.'))
      ext.remove(0, 1);
    if (!ext.isEmpty() && !result.contains(ext, Qt::CaseInsensitive))
      result.append(ext);
  }
  return result;
}

QString TestConfigurationDialog::uniqueName(const QString &baseName,
                                            const QString &excludeId) const {
  QString candidate = baseName.trimmed().isEmpty() ? tr("New Test Configuration")
                                                   : baseName.trimmed();
  QStringList existingNames;
  for (const TestConfiguration &cfg :
       TestConfigurationManager::instance().allConfigurations()) {
    if (cfg.id != excludeId)
      existingNames.append(cfg.name);
  }

  if (!existingNames.contains(candidate))
    return candidate;

  const QString base = candidate;
  int suffix = 2;
  while (existingNames.contains(candidate))
    candidate = tr("%1 (%2)").arg(base).arg(suffix++);
  return candidate;
}

void TestConfigurationDialog::setOriginText() {
  if (m_currentIsDraftConfig) {
    m_originLabel->setText(
        tr("Draft workspace configuration. Save to persist it to "
           ".lightpad/test/config.json."));
  } else if (m_currentIsUserConfig) {
    if (!m_currentConfigId.isEmpty() &&
        TestConfigurationManager::instance().templateById(m_currentConfigId)
            .isValid()) {
      m_originLabel->setText(tr("Workspace override for a built-in template."));
    } else {
      m_originLabel->setText(
          tr("Workspace configuration stored in .lightpad/test/config.json."));
    }
  } else if (!m_currentConfigId.isEmpty()) {
    m_originLabel->setText(
        tr("Built-in template. Save to create a workspace override, or "
           "duplicate it to keep a separate variant."));
  } else {
    m_originLabel->clear();
  }

  m_removeConfigBtn->setEnabled(m_currentIsUserConfig && !m_currentIsDraftConfig);
}

bool TestConfigurationDialog::currentSelectionIsUserConfiguration() const {
  return m_currentIsUserConfig && !m_currentIsDraftConfig;
}

void TestConfigurationDialog::onAddConfiguration() {
  QSignalBlocker blocker(m_configList);
  m_configList->clearSelection();
  m_configList->setCurrentItem(nullptr);

  TestConfiguration cfg;
  cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  cfg.name = uniqueName(tr("New Test Configuration"));
  cfg.outputFormat = "generic";
  cfg.workingDirectory = "${workspaceFolder}";
  cfg.discoveryDirectory = "${workspaceFolder}";
  loadConfigurationIntoForm(cfg, true, true);
}

void TestConfigurationDialog::onAddFromTemplate(const QString &templateId) {
  TestConfiguration cfg =
      TestConfigurationManager::instance().templateById(templateId);
  if (!cfg.isValid())
    return;

  QSignalBlocker blocker(m_configList);
  m_configList->clearSelection();
  m_configList->setCurrentItem(nullptr);

  cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  cfg.name = uniqueName(cfg.name);
  cfg.templateId = templateId;
  loadConfigurationIntoForm(cfg, true, true);
}

void TestConfigurationDialog::onDuplicateConfiguration() {
  TestConfiguration cfg = formConfiguration();
  if (cfg.name.isEmpty())
    cfg.name = m_loadedConfig.name;
  if (cfg.command.isEmpty())
    cfg.command = m_loadedConfig.command;

  if (cfg.name.isEmpty())
    return;

  if (cfg.templateId.isEmpty() &&
      TestConfigurationManager::instance().templateById(cfg.id).isValid()) {
    cfg.templateId = cfg.id;
  }

  QSignalBlocker blocker(m_configList);
  m_configList->clearSelection();
  m_configList->setCurrentItem(nullptr);

  cfg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  cfg.name = uniqueName(tr("%1 Copy").arg(cfg.name));
  loadConfigurationIntoForm(cfg, true, true);
}

void TestConfigurationDialog::onRemoveConfiguration() {
  if (!currentSelectionIsUserConfiguration())
    return;

  TestConfigurationManager &manager = TestConfigurationManager::instance();
  manager.removeConfiguration(m_currentConfigId);
  if (!manager.saveUserConfigurations(manager.workspaceFolder())) {
    ThemedMessageBox::warning(this, tr("Test Configurations"),
                              tr("Failed to save test configurations."));
  }
  reloadConfigurationList(m_currentConfigId);
}

void TestConfigurationDialog::onAddEnvVar() {
  const int row = m_envTable->rowCount();
  m_envTable->insertRow(row);
  m_envTable->setItem(row, 0, new QTableWidgetItem());
  m_envTable->setItem(row, 1, new QTableWidgetItem());
  m_envTable->setCurrentCell(row, 0);
}

void TestConfigurationDialog::onRemoveEnvVar() {
  const QModelIndexList rows = m_envTable->selectionModel()
                                   ? m_envTable->selectionModel()->selectedRows()
                                   : QModelIndexList();
  if (rows.isEmpty()) {
    if (m_envTable->currentRow() >= 0)
      m_envTable->removeRow(m_envTable->currentRow());
    return;
  }

  for (int i = rows.size() - 1; i >= 0; --i)
    m_envTable->removeRow(rows[i].row());
}

void TestConfigurationDialog::onSave() {
  TestConfiguration cfg = formConfiguration();
  if (cfg.name.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Test Configurations"),
                              tr("Configuration name is required."));
    return;
  }
  if (cfg.command.isEmpty()) {
    ThemedMessageBox::warning(this, tr("Test Configurations"),
                              tr("Command is required."));
    return;
  }

  TestConfigurationManager &manager = TestConfigurationManager::instance();
  const bool shouldPersistConfig =
      m_currentIsDraftConfig || m_currentIsUserConfig ||
      cfg.toJson() != m_loadedConfig.toJson();
  if (shouldPersistConfig)
    manager.addConfiguration(cfg);

  if (m_defaultCheck->isChecked()) {
    manager.setDefaultConfigurationId(cfg.id);
  } else if (manager.defaultConfigurationId() == cfg.id) {
    manager.setDefaultConfigurationId(QString());
  }

  if (!manager.saveUserConfigurations(manager.workspaceFolder())) {
    ThemedMessageBox::warning(this, tr("Test Configurations"),
                              tr("Failed to save test configurations."));
    return;
  }

  accept();
}

void TestConfigurationDialog::applyTheme(const Theme &theme) {
  StyledDialog::applyTheme(theme);

  stylePrimaryButton(m_saveButton);
  stylePrimaryButton(m_addConfigBtn);
  styleSecondaryButton(m_addTemplateBtn);
  styleSecondaryButton(m_duplicateConfigBtn);
  styleSecondaryButton(m_cancelButton);
  styleSecondaryButton(m_addEnvBtn);
  styleSecondaryButton(m_removeEnvBtn);
  styleDangerButton(m_removeConfigBtn);
  styleSubduedLabel(m_originLabel);
}
