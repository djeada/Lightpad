#include "debugconfigurationdialog.h"
#include "../../dap/debugadapterregistry.h"
#include "../../dap/debugconfiguration.h"
#include "../uistylehelper.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>

DebugConfigurationDialog::DebugConfigurationDialog(QWidget *parent)
    : QDialog(parent) {
  setupUi();
  loadConfigurations();
}

DebugConfigurationDialog::~DebugConfigurationDialog() {}

void DebugConfigurationDialog::setupUi() {
  setWindowTitle("Debug Configurations");
  setMinimumSize(820, 640);
  resize(900, 720);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  QSplitter *splitter = new QSplitter(Qt::Horizontal);

  QWidget *leftPanel = new QWidget();
  QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);

  QLabel *listLabel = new QLabel("<b>Configurations</b>");
  leftLayout->addWidget(listLabel);

  m_configList = new QListWidget();
  connect(m_configList, &QListWidget::currentItemChanged, this,
          &DebugConfigurationDialog::onConfigSelected);
  leftLayout->addWidget(m_configList);

  QHBoxLayout *listBtnLayout = new QHBoxLayout();
  m_addConfigBtn = new QPushButton("Add");
  m_addTemplateBtn = new QPushButton("Templates");
  m_removeConfigBtn = new QPushButton("Remove");
  m_duplicateConfigBtn = new QPushButton("Duplicate");
  connect(m_addConfigBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onAddConfig);
  connect(m_removeConfigBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onRemoveConfig);
  connect(m_duplicateConfigBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onDuplicateConfig);
  QMenu *templateMenu = new QMenu(m_addTemplateBtn);
  const QList<QPair<QString, QString>> templates = {
      {"python-current", tr("Python: Current File")},
      {"python-module", tr("Python: Module")},
      {"python-pytest", tr("Python: Pytest Current File")},
      {"python-attach", tr("Python: Attach by Host/Port")},
      {"gdb-launch", tr("C/C++: GDB Launch")},
      {"gdb-attach", tr("C/C++: Attach to Process")},
      {"gdb-remote", tr("C/C++: Remote gdbserver")},
      {"gdb-core", tr("C/C++: Core Dump Analysis")},
  };
  for (const auto &entry : templates) {
    QAction *action = templateMenu->addAction(entry.second);
    connect(action, &QAction::triggered, this,
            [this, templateId = entry.first]() {
              addConfigurationFromTemplate(templateId);
            });
  }
  m_addTemplateBtn->setMenu(templateMenu);
  listBtnLayout->addWidget(m_addConfigBtn);
  listBtnLayout->addWidget(m_addTemplateBtn);
  listBtnLayout->addWidget(m_removeConfigBtn);
  listBtnLayout->addWidget(m_duplicateConfigBtn);
  leftLayout->addLayout(listBtnLayout);

  splitter->addWidget(leftPanel);

  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);

  QWidget *rightPanel = new QWidget();
  QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

  QGroupBox *basicGroup = new QGroupBox("Basic Settings");
  QVBoxLayout *basicLayout = new QVBoxLayout(basicGroup);

  QHBoxLayout *nameLayout = new QHBoxLayout();
  nameLayout->addWidget(new QLabel("Name:"));
  m_nameEdit = new QLineEdit();
  m_nameEdit->setPlaceholderText("Configuration name");
  nameLayout->addWidget(m_nameEdit);
  basicLayout->addLayout(nameLayout);

  QHBoxLayout *adapterRowLayout = new QHBoxLayout();
  adapterRowLayout->addWidget(new QLabel("Adapter:"));
  m_adapterCombo = new QComboBox();
  m_adapterCombo->addItem("Auto (match by type)", "");
  for (const auto &adapter : DebugAdapterRegistry::instance().allAdapters()) {
    if (!adapter) {
      continue;
    }
    const DebugAdapterConfig cfg = adapter->config();
    m_adapterCombo->addItem(QString("%1 (%2)").arg(cfg.name, cfg.id), cfg.id);
  }
  connect(m_adapterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &DebugConfigurationDialog::updateAdapterUi);
  adapterRowLayout->addWidget(m_adapterCombo);
  basicLayout->addLayout(adapterRowLayout);

  QHBoxLayout *typeLayout = new QHBoxLayout();
  typeLayout->addWidget(new QLabel("Type:"));
  m_typeCombo = new QComboBox();
  m_typeCombo->setEditable(true);
  QStringList knownTypes = {"cppdbg", "debugpy",  "node",     "lldb",
                            "go",     "coreclr",  "java",     "php",
                            "ruby",   "rust-gdb", "rust-lldb"};
  for (const auto &adapter : DebugAdapterRegistry::instance().allAdapters()) {
    const QString type = adapter ? adapter->config().type.trimmed() : QString();
    if (!type.isEmpty() && !knownTypes.contains(type)) {
      knownTypes.append(type);
    }
  }
  knownTypes.removeDuplicates();
  m_typeCombo->addItems(knownTypes);
  connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &DebugConfigurationDialog::updateAdapterUi);
  connect(m_typeCombo, &QComboBox::editTextChanged, this,
          &DebugConfigurationDialog::updateAdapterUi);
  typeLayout->addWidget(m_typeCombo);
  basicLayout->addLayout(typeLayout);

  QHBoxLayout *requestLayout = new QHBoxLayout();
  requestLayout->addWidget(new QLabel("Request:"));
  m_requestCombo = new QComboBox();
  m_requestCombo->addItems({"launch", "attach"});
  requestLayout->addWidget(m_requestCombo);
  basicLayout->addLayout(requestLayout);

  m_adapterStatusLabel = new QLabel();
  m_adapterStatusLabel->setWordWrap(true);
  m_adapterStatusLabel->setStyleSheet("font-size: 11px; color: #8b949e;");
  basicLayout->addWidget(m_adapterStatusLabel);

  rightLayout->addWidget(basicGroup);

  QGroupBox *execGroup = new QGroupBox("Execution");
  QVBoxLayout *execLayout = new QVBoxLayout(execGroup);

  QHBoxLayout *progLayout = new QHBoxLayout();
  progLayout->addWidget(new QLabel("Program:"));
  m_programEdit = new QLineEdit();
  m_programEdit->setPlaceholderText(
      "Path to executable (e.g., ${workspaceFolder}/build/myapp)");
  connect(m_programEdit, &QLineEdit::textChanged, this,
          &DebugConfigurationDialog::updateAdapterUi);
  m_browseProgramBtn = new QPushButton("Browse...");
  connect(m_browseProgramBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onBrowseProgram);
  progLayout->addWidget(m_programEdit);
  progLayout->addWidget(m_browseProgramBtn);
  execLayout->addLayout(progLayout);

  QHBoxLayout *argsLayout = new QHBoxLayout();
  argsLayout->addWidget(new QLabel("Arguments:"));
  m_argsEdit = new QLineEdit();
  m_argsEdit->setPlaceholderText("Program arguments (space-separated)");
  argsLayout->addWidget(m_argsEdit);
  execLayout->addLayout(argsLayout);

  QHBoxLayout *cwdLayout = new QHBoxLayout();
  cwdLayout->addWidget(new QLabel("Working Dir:"));
  m_cwdEdit = new QLineEdit();
  m_cwdEdit->setPlaceholderText("Working directory (e.g., ${workspaceFolder})");
  connect(m_cwdEdit, &QLineEdit::textChanged, this,
          &DebugConfigurationDialog::updateAdapterUi);
  m_browseCwdBtn = new QPushButton("Browse...");
  connect(m_browseCwdBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onBrowseCwd);
  cwdLayout->addWidget(m_cwdEdit);
  cwdLayout->addWidget(m_browseCwdBtn);
  execLayout->addLayout(cwdLayout);

  m_stopOnEntryCheck = new QCheckBox("Stop on entry");
  execLayout->addWidget(m_stopOnEntryCheck);

  rightLayout->addWidget(execGroup);

  m_adapterOptionsGroup = new QGroupBox("Adapter Options");
  m_adapterOptionsLayout = new QFormLayout(m_adapterOptionsGroup);
  m_adapterOptionsLayout->setContentsMargins(12, 12, 12, 12);
  m_adapterOptionsLayout->setFieldGrowthPolicy(
      QFormLayout::ExpandingFieldsGrow);
  rightLayout->addWidget(m_adapterOptionsGroup);

  QGroupBox *envGroup = new QGroupBox("Environment Variables");
  QVBoxLayout *envLayout = new QVBoxLayout(envGroup);

  m_envTable = new QTableWidget(0, 2);
  m_envTable->setHorizontalHeaderLabels({"Variable", "Value"});
  m_envTable->horizontalHeader()->setStretchLastSection(true);
  m_envTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_envTable->setMaximumHeight(120);
  m_envTable->verticalHeader()->setVisible(false);
  envLayout->addWidget(m_envTable);

  QHBoxLayout *envBtnLayout = new QHBoxLayout();
  m_addEnvBtn = new QPushButton("Add");
  m_removeEnvBtn = new QPushButton("Remove");
  connect(m_addEnvBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onAddEnvVar);
  connect(m_removeEnvBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onRemoveEnvVar);
  envBtnLayout->addWidget(m_addEnvBtn);
  envBtnLayout->addWidget(m_removeEnvBtn);
  envBtnLayout->addStretch();
  envLayout->addLayout(envBtnLayout);

  rightLayout->addWidget(envGroup);

  QGroupBox *attachGroup = new QGroupBox("Attach / Remote");
  QVBoxLayout *attachLayout = new QVBoxLayout(attachGroup);

  QHBoxLayout *pidLayout = new QHBoxLayout();
  pidLayout->addWidget(new QLabel("Process ID:"));
  m_processIdSpin = new QSpinBox();
  m_processIdSpin->setRange(0, 999999);
  m_processIdSpin->setSpecialValueText("(not set)");
  pidLayout->addWidget(m_processIdSpin);
  pidLayout->addStretch();
  attachLayout->addLayout(pidLayout);

  QHBoxLayout *hostLayout = new QHBoxLayout();
  hostLayout->addWidget(new QLabel("Host:"));
  m_hostEdit = new QLineEdit();
  m_hostEdit->setPlaceholderText("Remote host (e.g., 127.0.0.1)");
  hostLayout->addWidget(m_hostEdit);
  attachLayout->addLayout(hostLayout);

  QHBoxLayout *portLayout = new QHBoxLayout();
  portLayout->addWidget(new QLabel("Port:"));
  m_portSpin = new QSpinBox();
  m_portSpin->setRange(0, 65535);
  m_portSpin->setSpecialValueText("(not set)");
  portLayout->addWidget(m_portSpin);
  portLayout->addStretch();
  attachLayout->addLayout(portLayout);

  rightLayout->addWidget(attachGroup);

  QGroupBox *tasksGroup = new QGroupBox("Tasks");
  QVBoxLayout *tasksLayout = new QVBoxLayout(tasksGroup);

  QHBoxLayout *preLaunchLayout = new QHBoxLayout();
  preLaunchLayout->addWidget(new QLabel("Pre-launch task:"));
  m_preLaunchTaskEdit = new QLineEdit();
  m_preLaunchTaskEdit->setPlaceholderText(
      "Task to run before debugging (e.g., build)");
  preLaunchLayout->addWidget(m_preLaunchTaskEdit);
  tasksLayout->addLayout(preLaunchLayout);

  QHBoxLayout *postDebugLayout = new QHBoxLayout();
  postDebugLayout->addWidget(new QLabel("Post-debug task:"));
  m_postDebugTaskEdit = new QLineEdit();
  m_postDebugTaskEdit->setPlaceholderText("Task to run after debugging ends");
  postDebugLayout->addWidget(m_postDebugTaskEdit);
  tasksLayout->addLayout(postDebugLayout);

  rightLayout->addWidget(tasksGroup);

  QGroupBox *adapterGroup = new QGroupBox("Additional Adapter Configuration");
  QVBoxLayout *adapterLayout = new QVBoxLayout(adapterGroup);
  QLabel *adapterHint = new QLabel(
      "Extra JSON properties passed to the debug adapter. One JSON object.");
  adapterHint->setWordWrap(true);
  adapterHint->setStyleSheet("font-size: 11px; color: #8b949e;");
  adapterLayout->addWidget(adapterHint);

  m_adapterConfigEdit = new QPlainTextEdit();
  m_adapterConfigEdit->setMaximumHeight(120);
  m_adapterConfigEdit->setPlaceholderText("{\n  \"MIMode\": \"gdb\"\n}");
  m_adapterConfigEdit->setTabStopDistance(20);
  connect(m_adapterConfigEdit, &QPlainTextEdit::textChanged, this,
          &DebugConfigurationDialog::updateAdapterUi);
  adapterLayout->addWidget(m_adapterConfigEdit);

  rightLayout->addWidget(adapterGroup);

  rightLayout->addStretch();

  scrollArea->setWidget(rightPanel);
  splitter->addWidget(scrollArea);

  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);

  mainLayout->addWidget(splitter, 1);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  m_saveButton = new QPushButton("Save");
  m_saveButton->setDefault(true);
  connect(m_saveButton, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onSave);
  buttonLayout->addWidget(m_saveButton);

  m_cancelButton = new QPushButton("Cancel");
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  buttonLayout->addWidget(m_cancelButton);

  mainLayout->addLayout(buttonLayout);
  setLayout(mainLayout);
  updateAdapterUi();
}

void DebugConfigurationDialog::loadConfigurations() {
  m_configList->clear();

  auto configs = DebugConfigurationManager::instance().allConfigurations();
  for (const auto &cfg : configs) {
    QListWidgetItem *item = new QListWidgetItem(cfg.name);
    item->setData(Qt::UserRole, cfg.name);
    m_configList->addItem(item);
  }

  if (m_configList->count() > 0) {
    m_configList->setCurrentRow(0);
  } else {
    clearForm();
  }
}

void DebugConfigurationDialog::onConfigSelected(QListWidgetItem *current,
                                                QListWidgetItem *previous) {
  if (previous && !m_currentConfigName.isEmpty()) {
    saveCurrentToModel();
  }

  if (!current) {
    clearForm();
    return;
  }

  QString name = current->data(Qt::UserRole).toString();
  DebugConfiguration cfg =
      DebugConfigurationManager::instance().configuration(name);
  m_currentConfigName = name;
  loadConfigIntoForm(cfg);
}

void DebugConfigurationDialog::loadConfigIntoForm(
    const DebugConfiguration &cfg) {
  m_nameEdit->setText(cfg.name);

  int adapterIdx = m_adapterCombo->findData(cfg.adapterId);
  if (adapterIdx >= 0) {
    m_adapterCombo->setCurrentIndex(adapterIdx);
  } else {
    m_adapterCombo->setCurrentIndex(0);
  }

  int typeIdx = m_typeCombo->findText(cfg.type);
  if (typeIdx >= 0) {
    m_typeCombo->setCurrentIndex(typeIdx);
  } else {
    m_typeCombo->setCurrentText(cfg.type);
  }

  int reqIdx = m_requestCombo->findText(cfg.request);
  if (reqIdx >= 0) {
    m_requestCombo->setCurrentIndex(reqIdx);
  }

  m_programEdit->setText(cfg.program);
  m_argsEdit->setText(cfg.args.join(" "));
  m_cwdEdit->setText(cfg.cwd);
  m_stopOnEntryCheck->setChecked(cfg.stopOnEntry);

  m_envTable->setRowCount(0);
  int row = 0;
  for (auto it = cfg.env.begin(); it != cfg.env.end(); ++it) {
    m_envTable->insertRow(row);
    m_envTable->setItem(row, 0, new QTableWidgetItem(it.key()));
    m_envTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    ++row;
  }

  m_processIdSpin->setValue(cfg.processId);
  m_hostEdit->setText(cfg.host);
  m_portSpin->setValue(cfg.port);
  if (cfg.processId == 0 && !cfg.processIdExpression.isEmpty()) {
    m_processIdSpin->setSpecialValueText(cfg.processIdExpression);
  } else {
    m_processIdSpin->setSpecialValueText("(not set)");
  }

  m_preLaunchTaskEdit->setText(cfg.preLaunchTask);
  m_postDebugTaskEdit->setText(cfg.postDebugTask);

  if (!cfg.adapterConfig.isEmpty()) {
    QJsonDocument doc(cfg.adapterConfig);
    m_adapterConfigEdit->setPlainText(
        QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
  } else {
    m_adapterConfigEdit->clear();
  }

  updateAdapterUi();
  populateAdapterOptions(cfg);
}

void DebugConfigurationDialog::clearForm() {
  m_currentConfigName.clear();
  m_nameEdit->clear();
  m_adapterCombo->setCurrentIndex(0);
  m_typeCombo->setCurrentIndex(0);
  m_requestCombo->setCurrentIndex(0);
  m_programEdit->clear();
  m_argsEdit->clear();
  m_cwdEdit->clear();
  m_stopOnEntryCheck->setChecked(false);
  m_envTable->setRowCount(0);
  m_processIdSpin->setValue(0);
  m_processIdSpin->setSpecialValueText("(not set)");
  m_hostEdit->clear();
  m_portSpin->setValue(0);
  m_preLaunchTaskEdit->clear();
  m_postDebugTaskEdit->clear();
  m_adapterConfigEdit->clear();
  for (QLineEdit *edit : std::as_const(m_adapterOptionEdits)) {
    if (edit) {
      edit->clear();
    }
  }
  updateAdapterUi();
}

void DebugConfigurationDialog::saveCurrentToModel() {
  if (m_currentConfigName.isEmpty()) {
    return;
  }

  DebugConfiguration cfg;
  cfg.name = m_nameEdit->text().trimmed();
  cfg.adapterId = m_adapterCombo->currentData().toString().trimmed();
  cfg.type = m_typeCombo->currentText().trimmed();
  cfg.request = m_requestCombo->currentText();

  cfg.program = m_programEdit->text().trimmed();
  QString argsText = m_argsEdit->text().trimmed();
  if (!argsText.isEmpty()) {
    cfg.args = argsText.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  }
  cfg.cwd = m_cwdEdit->text().trimmed();
  cfg.stopOnEntry = m_stopOnEntryCheck->isChecked();

  for (int row = 0; row < m_envTable->rowCount(); ++row) {
    QTableWidgetItem *keyItem = m_envTable->item(row, 0);
    QTableWidgetItem *valItem = m_envTable->item(row, 1);
    if (keyItem && !keyItem->text().trimmed().isEmpty()) {
      cfg.env[keyItem->text().trimmed()] =
          valItem ? valItem->text() : QString();
    }
  }

  cfg.processId = m_processIdSpin->value();
  if (cfg.processId == 0) {
    const QString specialValue = m_processIdSpin->specialValueText().trimmed();
    if (!specialValue.isEmpty() && specialValue != "(not set)") {
      cfg.processIdExpression = specialValue;
    }
  }
  cfg.host = m_hostEdit->text().trimmed();
  cfg.port = m_portSpin->value();

  cfg.preLaunchTask = m_preLaunchTaskEdit->text().trimmed();
  cfg.postDebugTask = m_postDebugTaskEdit->text().trimmed();

  QString adapterText = m_adapterConfigEdit->toPlainText().trimmed();
  if (!adapterText.isEmpty()) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(adapterText.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      cfg.adapterConfig = doc.object();
    }
  }
  applyAdapterOptionsToConfig(cfg);

  if (cfg.name.isEmpty()) {
    cfg.name = m_currentConfigName;
  }

  DebugConfigurationManager::instance().updateConfiguration(m_currentConfigName,
                                                            cfg);

  if (cfg.name != m_currentConfigName) {
    for (int i = 0; i < m_configList->count(); ++i) {
      if (m_configList->item(i)->data(Qt::UserRole).toString() ==
          m_currentConfigName) {
        m_configList->item(i)->setText(cfg.name);
        m_configList->item(i)->setData(Qt::UserRole, cfg.name);
        break;
      }
    }
    m_currentConfigName = cfg.name;
  }
}

void DebugConfigurationDialog::onAddConfig() {
  DebugConfiguration cfg;
  cfg.name = "New Configuration";
  cfg.adapterId = "cppdbg-gdb";
  cfg.type = "cppdbg";
  cfg.request = "launch";
  cfg.cwd = "${workspaceFolder}";

  int suffix = 1;
  QString baseName = cfg.name;
  while (!DebugConfigurationManager::instance()
              .configuration(cfg.name)
              .name.isEmpty()) {
    cfg.name = QString("%1 (%2)").arg(baseName).arg(suffix++);
  }

  DebugConfigurationManager::instance().addConfiguration(cfg);

  QListWidgetItem *item = new QListWidgetItem(cfg.name);
  item->setData(Qt::UserRole, cfg.name);
  m_configList->addItem(item);
  m_configList->setCurrentItem(item);
}

DebugConfiguration DebugConfigurationDialog::createTemplateConfiguration(
    const QString &templateId) const {
  DebugConfiguration cfg;
  cfg.cwd = "${workspaceFolder}";

  if (templateId == "python-current") {
    cfg.name = tr("Python: Current File");
    cfg.adapterId = "python-debugpy";
    cfg.type = "debugpy";
    cfg.request = "launch";
    cfg.program = "${file}";
    cfg.adapterConfig["console"] = "integratedTerminal";
    cfg.adapterConfig["justMyCode"] = true;
  } else if (templateId == "python-module") {
    cfg.name = tr("Python: Module");
    cfg.adapterId = "python-debugpy";
    cfg.type = "debugpy";
    cfg.request = "launch";
    cfg.adapterConfig["module"] = "your_package.module";
    cfg.adapterConfig["console"] = "integratedTerminal";
    cfg.adapterConfig["justMyCode"] = true;
  } else if (templateId == "python-pytest") {
    cfg.name = tr("Python: Pytest Current File");
    cfg.adapterId = "python-debugpy";
    cfg.type = "debugpy";
    cfg.request = "launch";
    cfg.args = {"${file}", "-q"};
    cfg.adapterConfig["module"] = "pytest";
    cfg.adapterConfig["console"] = "integratedTerminal";
    cfg.adapterConfig["justMyCode"] = false;
  } else if (templateId == "python-attach") {
    cfg.name = tr("Python: Attach by Host/Port");
    cfg.adapterId = "python-debugpy";
    cfg.type = "debugpy";
    cfg.request = "attach";
    cfg.host = "127.0.0.1";
    cfg.port = 5678;
  } else if (templateId == "gdb-launch") {
    cfg.name = tr("C++: GDB Launch");
    cfg.adapterId = "cppdbg-gdb";
    cfg.type = "cppdbg";
    cfg.request = "launch";
    cfg.program = "${workspaceFolder}/a.out";
    cfg.adapterConfig["MIMode"] = "gdb";
    if (auto gdbAdapter =
            DebugAdapterRegistry::instance().adapter("cppdbg-gdb")) {
      cfg.adapterConfig["miDebuggerPath"] = gdbAdapter->createLaunchConfig(
          "${workspaceFolder}/a.out", "${workspaceFolder}")["miDebuggerPath"];
    } else {
      cfg.adapterConfig["miDebuggerPath"] = "gdb";
    }
    cfg.adapterConfig["externalConsole"] = false;
    cfg.adapterConfig["stopAtEntry"] = false;
  } else if (templateId == "gdb-attach") {
    cfg.name = tr("C++: Attach to Process");
    cfg.adapterId = "cppdbg-gdb";
    cfg.type = "cppdbg";
    cfg.request = "attach";
    cfg.program = "${workspaceFolder}/a.out";
    cfg.adapterConfig["MIMode"] = "gdb";
    cfg.adapterConfig["processId"] = "${command:pickProcess}";
  } else if (templateId == "gdb-remote") {
    cfg.name = tr("C++: Remote gdbserver");
    cfg.adapterId = "cppdbg-gdb";
    cfg.type = "cppdbg";
    cfg.request = "attach";
    cfg.program = "${workspaceFolder}/a.out";
    cfg.host = "127.0.0.1";
    cfg.port = 1234;
    cfg.adapterConfig["MIMode"] = "gdb";
    cfg.adapterConfig["miDebuggerServerAddress"] = "127.0.0.1:1234";
  } else if (templateId == "gdb-core") {
    cfg.name = tr("C++: Core Dump Analysis");
    cfg.adapterId = "cppdbg-gdb";
    cfg.type = "cppdbg";
    cfg.request = "launch";
    cfg.program = "${workspaceFolder}/a.out";
    cfg.adapterConfig["MIMode"] = "gdb";
    cfg.adapterConfig["coreDumpPath"] = "${workspaceFolder}/core";
  } else {
    cfg.name = tr("New Configuration");
    cfg.adapterId = "cppdbg-gdb";
    cfg.type = "cppdbg";
    cfg.request = "launch";
  }

  return cfg;
}

void DebugConfigurationDialog::addConfigurationFromTemplate(
    const QString &templateId) {
  DebugConfiguration cfg = createTemplateConfiguration(templateId);
  int suffix = 1;
  const QString baseName = cfg.name;
  while (!DebugConfigurationManager::instance()
              .configuration(cfg.name)
              .name.isEmpty()) {
    cfg.name = QString("%1 (%2)").arg(baseName).arg(suffix++);
  }

  DebugConfigurationManager::instance().addConfiguration(cfg);

  QListWidgetItem *item = new QListWidgetItem(cfg.name);
  item->setData(Qt::UserRole, cfg.name);
  m_configList->addItem(item);
  m_configList->setCurrentItem(item);
}

std::shared_ptr<IDebugAdapter>
DebugConfigurationDialog::selectedAdapter() const {
  const QString adapterId = m_adapterCombo->currentData().toString().trimmed();
  if (!adapterId.isEmpty()) {
    return DebugAdapterRegistry::instance().adapter(adapterId);
  }

  DebugConfiguration probe;
  probe.type = m_typeCombo->currentText().trimmed();
  const auto adapters =
      DebugAdapterRegistry::instance().adaptersForConfiguration(probe);
  return adapters.isEmpty() ? nullptr : adapters.first();
}

void DebugConfigurationDialog::applyAdapterOptionsToConfig(
    DebugConfiguration &cfg) const {
  QStringList optionKeys;
  for (const auto &adapter : DebugAdapterRegistry::instance().allAdapters()) {
    if (!adapter) {
      continue;
    }
    for (const DebugAdapterOption &option : adapter->configurationOptions()) {
      if (!option.key.isEmpty() && !optionKeys.contains(option.key)) {
        optionKeys.append(option.key);
      }
    }
  }
  for (const QString &key : optionKeys) {
    cfg.adapterConfig.remove(key);
  }

  const auto adapter = selectedAdapter();
  if (!adapter) {
    return;
  }

  for (const DebugAdapterOption &option : adapter->configurationOptions()) {
    const auto it = m_adapterOptionEdits.constFind(option.key);
    if (it == m_adapterOptionEdits.constEnd() || !it.value()) {
      continue;
    }

    const QString value = it.value()->text().trimmed();
    if (!value.isEmpty()) {
      cfg.adapterConfig[option.key] = value;
    }
  }
}

void DebugConfigurationDialog::onRemoveConfig() {
  QListWidgetItem *current = m_configList->currentItem();
  if (!current) {
    return;
  }

  QString name = current->data(Qt::UserRole).toString();
  DebugConfigurationManager::instance().removeConfiguration(name);
  m_currentConfigName.clear();
  delete current;

  if (m_configList->count() > 0) {
    m_configList->setCurrentRow(0);
  } else {
    clearForm();
  }
}

void DebugConfigurationDialog::updateAdapterUi() {
  const auto adapter = selectedAdapter();
  if (!adapter) {
    rebuildAdapterOptionsUi(nullptr);
    m_adapterStatusLabel->setText("No registered adapter matches this type.");
    return;
  }

  rebuildAdapterOptionsUi(adapter);

  DebugConfiguration preview;
  preview.adapterId = m_adapterCombo->currentData().toString().trimmed();
  preview.type = m_typeCombo->currentText().trimmed();
  preview.request = m_requestCombo->currentText();
  preview.program = m_programEdit->text().trimmed();
  preview.cwd = m_cwdEdit->text().trimmed();

  const QString adapterText = m_adapterConfigEdit->toPlainText().trimmed();
  if (!adapterText.isEmpty()) {
    QJsonParseError err;
    const QJsonDocument doc =
        QJsonDocument::fromJson(adapterText.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
      preview.adapterConfig = doc.object();
    }
  }

  for (const DebugAdapterOption &option : adapter->configurationOptions()) {
    const auto it = m_adapterOptionEdits.constFind(option.key);
    if (it == m_adapterOptionEdits.constEnd() || !it.value()) {
      continue;
    }

    const QString value = it.value()->text().trimmed();
    if (!value.isEmpty()) {
      preview.adapterConfig[option.key] = value;
    } else {
      preview.adapterConfig.remove(option.key);
    }
  }

  m_adapterStatusLabel->setText(
      QString("Status: %1")
          .arg(adapter->statusMessageForConfiguration(preview)));
}

void DebugConfigurationDialog::rebuildAdapterOptionsUi(
    const std::shared_ptr<IDebugAdapter> &adapter) {
  const QList<DebugAdapterOption> options =
      adapter ? adapter->configurationOptions() : QList<DebugAdapterOption>{};

  QStringList signatureParts;
  for (const DebugAdapterOption &option : options) {
    signatureParts.append(option.key + "|" + option.label + "|" +
                          option.placeholder);
  }
  const QString newSignature = signatureParts.join("||");
  if (m_adapterOptionsSignature == newSignature) {
    m_adapterOptionsGroup->setVisible(!options.isEmpty());
    return;
  }

  m_adapterOptionsSignature = newSignature;
  m_adapterOptionEdits.clear();

  while (m_adapterOptionsLayout->rowCount() > 0) {
    m_adapterOptionsLayout->removeRow(0);
  }

  for (const DebugAdapterOption &option : options) {
    QLabel *label = new QLabel(option.label + ":");
    QLineEdit *edit = new QLineEdit();
    edit->setPlaceholderText(option.placeholder);
    connect(edit, &QLineEdit::textChanged, this,
            &DebugConfigurationDialog::updateAdapterUi);

    QWidget *fieldWidget = new QWidget();
    QHBoxLayout *fieldLayout = new QHBoxLayout(fieldWidget);
    fieldLayout->setContentsMargins(0, 0, 0, 0);
    fieldLayout->setSpacing(6);
    fieldLayout->addWidget(edit);

    if (option.browseable) {
      QPushButton *browse = new QPushButton("Browse...");
      connect(browse, &QPushButton::clicked, this,
              [this, key = option.key, title = option.label]() {
                const QString file = QFileDialog::getOpenFileName(
                    this, tr("Select %1").arg(title));
                if (!file.isEmpty() && m_adapterOptionEdits.contains(key) &&
                    m_adapterOptionEdits[key]) {
                  m_adapterOptionEdits[key]->setText(file);
                }
              });
      fieldLayout->addWidget(browse);
    }

    m_adapterOptionsLayout->addRow(label, fieldWidget);
    m_adapterOptionEdits.insert(option.key, edit);
  }

  m_adapterOptionsGroup->setVisible(!options.isEmpty());
}

void DebugConfigurationDialog::populateAdapterOptions(
    const DebugConfiguration &cfg) {
  const auto adapter = selectedAdapter();
  if (!adapter) {
    return;
  }

  for (const DebugAdapterOption &option : adapter->configurationOptions()) {
    const auto it = m_adapterOptionEdits.find(option.key);
    if (it != m_adapterOptionEdits.end() && it.value()) {
      it.value()->setText(cfg.adapterConfig[option.key].toString());
    }
  }
}

void DebugConfigurationDialog::onDuplicateConfig() {
  QListWidgetItem *current = m_configList->currentItem();
  if (!current) {
    return;
  }

  saveCurrentToModel();

  QString name = current->data(Qt::UserRole).toString();
  DebugConfiguration cfg =
      DebugConfigurationManager::instance().configuration(name);

  cfg.name = cfg.name + " (Copy)";
  int suffix = 2;
  QString baseName = cfg.name;
  while (!DebugConfigurationManager::instance()
              .configuration(cfg.name)
              .name.isEmpty()) {
    cfg.name = QString("%1 %2").arg(baseName).arg(suffix++);
  }

  DebugConfigurationManager::instance().addConfiguration(cfg);

  QListWidgetItem *item = new QListWidgetItem(cfg.name);
  item->setData(Qt::UserRole, cfg.name);
  m_configList->addItem(item);
  m_configList->setCurrentItem(item);
}

void DebugConfigurationDialog::onAddEnvVar() {
  int row = m_envTable->rowCount();
  m_envTable->insertRow(row);
  m_envTable->setItem(row, 0, new QTableWidgetItem(""));
  m_envTable->setItem(row, 1, new QTableWidgetItem(""));
  m_envTable->editItem(m_envTable->item(row, 0));
}

void DebugConfigurationDialog::onRemoveEnvVar() {
  QList<QTableWidgetItem *> selected = m_envTable->selectedItems();
  QSet<int> rows;
  for (QTableWidgetItem *item : selected) {
    rows.insert(item->row());
  }
  QList<int> sortedRows = rows.values();
  std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
  for (int row : sortedRows) {
    m_envTable->removeRow(row);
  }
}

void DebugConfigurationDialog::onBrowseProgram() {
  QString file = QFileDialog::getOpenFileName(this, "Select Program");
  if (!file.isEmpty()) {
    m_programEdit->setText(file);
  }
}

void DebugConfigurationDialog::onBrowseCwd() {
  QString dir =
      QFileDialog::getExistingDirectory(this, "Select Working Directory");
  if (!dir.isEmpty()) {
    m_cwdEdit->setText(dir);
  }
}

void DebugConfigurationDialog::onSave() {
  saveCurrentToModel();

  if (!DebugConfigurationManager::instance().saveToLightpadDir()) {
    QMessageBox::warning(this, "Debug Configurations",
                         "Failed to save debug configurations.");
    return;
  }
  accept();
}

void DebugConfigurationDialog::applyTheme(const Theme &theme) {
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  for (QGroupBox *groupBox : findChildren<QGroupBox *>()) {
    groupBox->setStyleSheet(UIStyleHelper::groupBoxStyle(theme));
  }

  for (QLineEdit *edit : findChildren<QLineEdit *>()) {
    edit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
  }

  for (QComboBox *combo : findChildren<QComboBox *>()) {
    combo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }

  for (QCheckBox *check : findChildren<QCheckBox *>()) {
    check->setStyleSheet(UIStyleHelper::checkBoxStyle(theme));
  }

  if (m_configList) {
    m_configList->setStyleSheet(UIStyleHelper::resultListStyle(theme));
  }

  QString tableStyle = QString("QTableWidget {"
                               "  background: %1;"
                               "  color: %2;"
                               "  border: 1px solid %3;"
                               "  border-radius: 4px;"
                               "  gridline-color: %3;"
                               "}"
                               "QHeaderView::section {"
                               "  background: %4;"
                               "  color: %2;"
                               "  border: none;"
                               "  border-bottom: 1px solid %3;"
                               "  padding: 4px 8px;"
                               "  font-weight: bold;"
                               "  font-size: 11px;"
                               "}")
                           .arg(theme.surfaceAltColor.name())
                           .arg(theme.foregroundColor.name())
                           .arg(theme.borderColor.name())
                           .arg(theme.surfaceColor.name());
  if (m_envTable) {
    m_envTable->setStyleSheet(tableStyle);
  }

  QString spinStyle = QString("QSpinBox {"
                              "  background: %1;"
                              "  color: %2;"
                              "  border: 1px solid %3;"
                              "  border-radius: 6px;"
                              "  padding: 4px 8px;"
                              "}"
                              "QSpinBox:focus {"
                              "  border-color: %4;"
                              "}")
                          .arg(theme.hoverColor.name())
                          .arg(theme.foregroundColor.name())
                          .arg(theme.borderColor.name())
                          .arg(theme.accentColor.name());
  for (QSpinBox *spin : findChildren<QSpinBox *>()) {
    spin->setStyleSheet(spinStyle);
  }

  if (m_adapterConfigEdit) {
    m_adapterConfigEdit->setStyleSheet(QString("QPlainTextEdit {"
                                               "  background: %1;"
                                               "  color: %2;"
                                               "  border: 1px solid %3;"
                                               "  border-radius: 4px;"
                                               "  font-family: monospace;"
                                               "  font-size: 12px;"
                                               "  padding: 4px;"
                                               "}"
                                               "QPlainTextEdit:focus {"
                                               "  border-color: %4;"
                                               "}")
                                           .arg(theme.surfaceAltColor.name())
                                           .arg(theme.foregroundColor.name())
                                           .arg(theme.borderColor.name())
                                           .arg(theme.accentColor.name()));
  }

  if (m_saveButton) {
    m_saveButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
  }
  if (m_cancelButton) {
    m_cancelButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
  }

  for (QPushButton *btn :
       {m_addConfigBtn, m_removeConfigBtn, m_duplicateConfigBtn,
        m_browseProgramBtn, m_browseCwdBtn, m_addEnvBtn, m_removeEnvBtn}) {
    if (btn) {
      btn->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
    }
  }
}
