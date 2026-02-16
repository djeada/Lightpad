#include "debugconfigurationdialog.h"
#include "../../dap/debugconfiguration.h"
#include "../uistylehelper.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
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
  m_removeConfigBtn = new QPushButton("Remove");
  m_duplicateConfigBtn = new QPushButton("Duplicate");
  connect(m_addConfigBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onAddConfig);
  connect(m_removeConfigBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onRemoveConfig);
  connect(m_duplicateConfigBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onDuplicateConfig);
  listBtnLayout->addWidget(m_addConfigBtn);
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

  QHBoxLayout *typeLayout = new QHBoxLayout();
  typeLayout->addWidget(new QLabel("Type:"));
  m_typeCombo = new QComboBox();
  m_typeCombo->setEditable(true);
  m_typeCombo->addItems({"cppdbg", "lldb", "python", "node", "go", "coreclr",
                         "java", "php", "ruby", "rust-gdb", "rust-lldb"});
  typeLayout->addWidget(m_typeCombo);
  basicLayout->addLayout(typeLayout);

  QHBoxLayout *requestLayout = new QHBoxLayout();
  requestLayout->addWidget(new QLabel("Request:"));
  m_requestCombo = new QComboBox();
  m_requestCombo->addItems({"launch", "attach"});
  requestLayout->addWidget(m_requestCombo);
  basicLayout->addLayout(requestLayout);

  rightLayout->addWidget(basicGroup);

  QGroupBox *execGroup = new QGroupBox("Execution");
  QVBoxLayout *execLayout = new QVBoxLayout(execGroup);

  QHBoxLayout *progLayout = new QHBoxLayout();
  progLayout->addWidget(new QLabel("Program:"));
  m_programEdit = new QLineEdit();
  m_programEdit->setPlaceholderText(
      "Path to executable (e.g., ${workspaceFolder}/build/myapp)");
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
  m_browseCwdBtn = new QPushButton("Browse...");
  connect(m_browseCwdBtn, &QPushButton::clicked, this,
          &DebugConfigurationDialog::onBrowseCwd);
  cwdLayout->addWidget(m_cwdEdit);
  cwdLayout->addWidget(m_browseCwdBtn);
  execLayout->addLayout(cwdLayout);

  m_stopOnEntryCheck = new QCheckBox("Stop on entry");
  execLayout->addWidget(m_stopOnEntryCheck);

  rightLayout->addWidget(execGroup);

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

  m_preLaunchTaskEdit->setText(cfg.preLaunchTask);
  m_postDebugTaskEdit->setText(cfg.postDebugTask);

  if (!cfg.adapterConfig.isEmpty()) {
    QJsonDocument doc(cfg.adapterConfig);
    m_adapterConfigEdit->setPlainText(
        QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
  } else {
    m_adapterConfigEdit->clear();
  }
}

void DebugConfigurationDialog::clearForm() {
  m_currentConfigName.clear();
  m_nameEdit->clear();
  m_typeCombo->setCurrentIndex(0);
  m_requestCombo->setCurrentIndex(0);
  m_programEdit->clear();
  m_argsEdit->clear();
  m_cwdEdit->clear();
  m_stopOnEntryCheck->setChecked(false);
  m_envTable->setRowCount(0);
  m_processIdSpin->setValue(0);
  m_hostEdit->clear();
  m_portSpin->setValue(0);
  m_preLaunchTaskEdit->clear();
  m_postDebugTaskEdit->clear();
  m_adapterConfigEdit->clear();
}

void DebugConfigurationDialog::saveCurrentToModel() {
  if (m_currentConfigName.isEmpty()) {
    return;
  }

  DebugConfiguration cfg;
  cfg.name = m_nameEdit->text().trimmed();
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
