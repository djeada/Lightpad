#include "runtemplateselector.h"
#include "../../run_templates/runtemplatemanager.h"
#include "../uistylehelper.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>

namespace {
QString quoteArgumentForDisplay(const QString &arg) {
  if (arg.isEmpty()) {
    return "\"\"";
  }

  bool needsQuotes =
      arg.contains(' ') || arg.contains('\t') || arg.contains('"');
  if (!needsQuotes) {
    return arg;
  }

  QString escaped = arg;
  escaped.replace('\\', "\\\\");
  escaped.replace('"', "\\\"");
  return QString("\"%1\"").arg(escaped);
}

QString joinArgumentsForDisplay(const QStringList &args) {
  QStringList rendered;
  rendered.reserve(args.size());
  for (const QString &arg : args) {
    rendered.append(quoteArgumentForDisplay(arg));
  }
  return rendered.join(" ");
}
} // namespace

RunTemplateSelector::RunTemplateSelector(const QString &filePath,
                                         QWidget *parent)
    : QDialog(parent), m_filePath(filePath) {
  setupUi();
  loadTemplates();

  FileTemplateAssignment assignment =
      RunTemplateManager::instance().getAssignmentForFile(filePath);
  if (!assignment.templateId.isEmpty()) {
    for (int i = 0; i < m_templateList->count(); ++i) {
      if (m_templateList->item(i)->data(Qt::UserRole).toString() ==
          assignment.templateId) {
        m_templateList->setCurrentRow(i);
        break;
      }
    }
    m_customArgsEdit->setText(joinArgumentsForDisplay(assignment.customArgs));

    for (const QString &src : assignment.sourceFiles) {
      m_sourceFilesList->addItem(src);
    }

    m_workingDirEdit->setText(assignment.workingDirectory);
    m_compilerFlagsEdit->setText(
        joinArgumentsForDisplay(assignment.compilerFlags));

    int row = 0;
    for (auto it = assignment.customEnv.begin();
         it != assignment.customEnv.end(); ++it) {
      m_envVarTable->insertRow(row);
      m_envVarTable->setItem(row, 0, new QTableWidgetItem(it.key()));
      m_envVarTable->setItem(row, 1, new QTableWidgetItem(it.value()));
      ++row;
    }

    m_preRunCommandEdit->setText(assignment.preRunCommand);
    m_postRunCommandEdit->setText(assignment.postRunCommand);
  }
}

RunTemplateSelector::~RunTemplateSelector() {}

void RunTemplateSelector::setupUi() {
  setWindowTitle("Run Configuration");
  setMinimumSize(720, 620);
  resize(780, 700);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  QFileInfo fileInfo(m_filePath);
  QLabel *fileLabel =
      new QLabel(QString("File: <b>%1</b>").arg(fileInfo.fileName()));
  mainLayout->addWidget(fileLabel);

  QSplitter *splitter = new QSplitter(Qt::Horizontal);

  QWidget *leftPanel = new QWidget();
  QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);

  QHBoxLayout *filterLayout = new QHBoxLayout();
  m_searchEdit = new QLineEdit();
  m_searchEdit->setPlaceholderText("Search templates...");
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &RunTemplateSelector::onFilterChanged);
  filterLayout->addWidget(m_searchEdit);

  m_languageCombo = new QComboBox();
  m_languageCombo->addItem("All Languages");
  connect(m_languageCombo, &QComboBox::currentTextChanged, this,
          &RunTemplateSelector::onLanguageFilterChanged);
  filterLayout->addWidget(m_languageCombo);
  leftLayout->addLayout(filterLayout);

  QGroupBox *templatesGroup = new QGroupBox("Available Templates");
  QVBoxLayout *templatesLayout = new QVBoxLayout(templatesGroup);
  m_templateList = new QListWidget();
  connect(m_templateList, &QListWidget::itemClicked, this,
          &RunTemplateSelector::onTemplateSelected);
  connect(m_templateList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { onAccept(); });
  templatesLayout->addWidget(m_templateList);

  m_descriptionLabel = new QLabel();
  m_descriptionLabel->setWordWrap(true);
  templatesLayout->addWidget(m_descriptionLabel);

  m_commandLabel = new QLabel();
  m_commandLabel->setWordWrap(true);
  m_commandLabel->setStyleSheet(
      "font-family: monospace; background-color: #1f2632; color: #e6edf3; "
      "padding: 6px; border-radius: 6px;");
  templatesLayout->addWidget(m_commandLabel);
  leftLayout->addWidget(templatesGroup);

  splitter->addWidget(leftPanel);

  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);

  QWidget *rightPanel = new QWidget();
  QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);

  QGroupBox *argsGroup = new QGroupBox("Arguments");
  QVBoxLayout *argsLayout = new QVBoxLayout(argsGroup);
  m_customArgsEdit = new QLineEdit();
  m_customArgsEdit->setPlaceholderText(
      "Additional arguments (e.g., --verbose -n 10)");
  argsLayout->addWidget(m_customArgsEdit);
  rightLayout->addWidget(argsGroup);

  QGroupBox *sourceGroup = new QGroupBox("Source Files");
  QVBoxLayout *sourceLayout = new QVBoxLayout(sourceGroup);
  QLabel *sourceHint = new QLabel(
      "Additional source files for compilation (e.g., multi-file C/C++).\n"
      "Supports variables: ${fileDir}, ${workspaceFolder}");
  sourceHint->setWordWrap(true);
  sourceHint->setStyleSheet("font-size: 11px; color: #8b949e;");
  sourceLayout->addWidget(sourceHint);

  m_sourceFilesList = new QListWidget();
  m_sourceFilesList->setMaximumHeight(120);
  sourceLayout->addWidget(m_sourceFilesList);

  QHBoxLayout *sourceButtonLayout = new QHBoxLayout();
  m_addSourceFileBtn = new QPushButton("Add File...");
  m_removeSourceFileBtn = new QPushButton("Remove");
  connect(m_addSourceFileBtn, &QPushButton::clicked, this,
          &RunTemplateSelector::onAddSourceFile);
  connect(m_removeSourceFileBtn, &QPushButton::clicked, this,
          &RunTemplateSelector::onRemoveSourceFile);
  sourceButtonLayout->addWidget(m_addSourceFileBtn);
  sourceButtonLayout->addWidget(m_removeSourceFileBtn);
  sourceButtonLayout->addStretch();
  sourceLayout->addLayout(sourceButtonLayout);
  rightLayout->addWidget(sourceGroup);

  QGroupBox *wdGroup = new QGroupBox("Working Directory");
  QHBoxLayout *wdLayout = new QHBoxLayout(wdGroup);
  m_workingDirEdit = new QLineEdit();
  m_workingDirEdit->setPlaceholderText(
      "Override working directory (default: ${fileDir})");
  m_browseWorkingDirBtn = new QPushButton("Browse...");
  connect(m_browseWorkingDirBtn, &QPushButton::clicked, this,
          &RunTemplateSelector::onBrowseWorkingDir);
  wdLayout->addWidget(m_workingDirEdit);
  wdLayout->addWidget(m_browseWorkingDirBtn);
  rightLayout->addWidget(wdGroup);

  QGroupBox *flagsGroup = new QGroupBox("Compiler / Linker Flags");
  QVBoxLayout *flagsLayout = new QVBoxLayout(flagsGroup);
  m_compilerFlagsEdit = new QLineEdit();
  m_compilerFlagsEdit->setPlaceholderText(
      "e.g., -std=c++17 -Wall -O2 -lpthread");
  flagsLayout->addWidget(m_compilerFlagsEdit);
  rightLayout->addWidget(flagsGroup);

  QGroupBox *envGroup = new QGroupBox("Environment Variables");
  QVBoxLayout *envLayout = new QVBoxLayout(envGroup);

  m_envVarTable = new QTableWidget(0, 2);
  m_envVarTable->setHorizontalHeaderLabels({"Variable", "Value"});
  m_envVarTable->horizontalHeader()->setStretchLastSection(true);
  m_envVarTable->horizontalHeader()->setSectionResizeMode(0,
                                                          QHeaderView::Stretch);
  m_envVarTable->setMaximumHeight(120);
  m_envVarTable->verticalHeader()->setVisible(false);
  envLayout->addWidget(m_envVarTable);

  QHBoxLayout *envButtonLayout = new QHBoxLayout();
  m_addEnvVarBtn = new QPushButton("Add");
  m_removeEnvVarBtn = new QPushButton("Remove");
  connect(m_addEnvVarBtn, &QPushButton::clicked, this,
          &RunTemplateSelector::onAddEnvVar);
  connect(m_removeEnvVarBtn, &QPushButton::clicked, this,
          &RunTemplateSelector::onRemoveEnvVar);
  envButtonLayout->addWidget(m_addEnvVarBtn);
  envButtonLayout->addWidget(m_removeEnvVarBtn);
  envButtonLayout->addStretch();
  envLayout->addLayout(envButtonLayout);
  rightLayout->addWidget(envGroup);

  QGroupBox *hooksGroup = new QGroupBox("Pre/Post Run Commands");
  QVBoxLayout *hooksLayout = new QVBoxLayout(hooksGroup);

  hooksLayout->addWidget(new QLabel("Pre-run command:"));
  m_preRunCommandEdit = new QLineEdit();
  m_preRunCommandEdit->setPlaceholderText(
      "Command to run before execution (e.g., make, cmake --build build)");
  hooksLayout->addWidget(m_preRunCommandEdit);

  hooksLayout->addWidget(new QLabel("Post-run command:"));
  m_postRunCommandEdit = new QLineEdit();
  m_postRunCommandEdit->setPlaceholderText(
      "Command to run after execution (e.g., cleanup script)");
  hooksLayout->addWidget(m_postRunCommandEdit);
  rightLayout->addWidget(hooksGroup);

  rightLayout->addStretch();

  scrollArea->setWidget(rightPanel);
  splitter->addWidget(scrollArea);

  splitter->setStretchFactor(0, 2);
  splitter->setStretchFactor(1, 3);

  mainLayout->addWidget(splitter, 1);

  QHBoxLayout *buttonLayout = new QHBoxLayout();

  m_removeButton = new QPushButton("Remove Assignment");
  connect(m_removeButton, &QPushButton::clicked, this,
          &RunTemplateSelector::onRemoveAssignment);
  buttonLayout->addWidget(m_removeButton);

  buttonLayout->addStretch();

  m_okButton = new QPushButton("OK");
  m_okButton->setDefault(true);
  connect(m_okButton, &QPushButton::clicked, this,
          &RunTemplateSelector::onAccept);
  buttonLayout->addWidget(m_okButton);

  m_cancelButton = new QPushButton("Cancel");
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  buttonLayout->addWidget(m_cancelButton);

  mainLayout->addLayout(buttonLayout);
  setLayout(mainLayout);
}

void RunTemplateSelector::loadTemplates() {
  RunTemplateManager &manager = RunTemplateManager::instance();

  if (manager.getAllTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  QSet<QString> languages;
  for (const RunTemplate &tmpl : manager.getAllTemplates()) {
    if (!tmpl.language.isEmpty()) {
      languages.insert(tmpl.language);
    }
  }

  QStringList sortedLanguages = languages.values();
  sortedLanguages.sort();
  for (const QString &lang : sortedLanguages) {
    m_languageCombo->addItem(lang);
  }

  QFileInfo fileInfo(m_filePath);
  QString ext = fileInfo.suffix().toLower();
  QList<RunTemplate> matchingTemplates = manager.getTemplatesForExtension(ext);
  if (!matchingTemplates.isEmpty()) {
    QString matchedLanguage = matchingTemplates.first().language;
    int idx = m_languageCombo->findText(matchedLanguage);
    if (idx >= 0) {
      m_languageCombo->setCurrentIndex(idx);
    }
  }

  filterTemplates();
}

void RunTemplateSelector::filterTemplates() {
  m_templateList->clear();

  RunTemplateManager &manager = RunTemplateManager::instance();
  QList<RunTemplate> templates = manager.getAllTemplates();

  for (const RunTemplate &tmpl : templates) {

    if (!m_currentLanguage.isEmpty() && m_currentLanguage != "All Languages" &&
        tmpl.language != m_currentLanguage) {
      continue;
    }

    if (!m_currentFilter.isEmpty()) {
      bool matches =
          tmpl.name.contains(m_currentFilter, Qt::CaseInsensitive) ||
          tmpl.description.contains(m_currentFilter, Qt::CaseInsensitive) ||
          tmpl.language.contains(m_currentFilter, Qt::CaseInsensitive);
      if (!matches) {
        continue;
      }
    }

    QListWidgetItem *item = new QListWidgetItem();
    item->setText(QString("%1 (%2)").arg(tmpl.name, tmpl.language));
    item->setData(Qt::UserRole, tmpl.id);
    item->setToolTip(tmpl.description);
    m_templateList->addItem(item);
  }

  if (m_templateList->count() > 0) {
    m_templateList->setCurrentRow(0);
    onTemplateSelected(m_templateList->currentItem());
  }
}

void RunTemplateSelector::onTemplateSelected(QListWidgetItem *item) {
  if (!item) {
    m_descriptionLabel->clear();
    m_commandLabel->clear();
    m_selectedTemplateId.clear();
    return;
  }

  QString templateId = item->data(Qt::UserRole).toString();
  RunTemplate tmpl = RunTemplateManager::instance().getTemplateById(templateId);

  m_selectedTemplateId = templateId;
  m_descriptionLabel->setText(tmpl.description);

  QString commandPreview = tmpl.command;
  if (!tmpl.args.isEmpty()) {
    commandPreview += " " + tmpl.args.join(" ");
  }
  m_commandLabel->setText(QString("<b>Command:</b> %1").arg(commandPreview));
}

void RunTemplateSelector::onFilterChanged(const QString &filter) {
  m_currentFilter = filter;
  filterTemplates();
}

void RunTemplateSelector::onLanguageFilterChanged(const QString &language) {
  m_currentLanguage = language;
  filterTemplates();
}

void RunTemplateSelector::onAccept() {
  if (!m_selectedTemplateId.isEmpty()) {
    FileTemplateAssignment assignment;
    assignment.filePath = m_filePath;
    assignment.templateId = m_selectedTemplateId;

    QString argsText = m_customArgsEdit->text().trimmed();
    if (!argsText.isEmpty()) {
      assignment.customArgs = QProcess::splitCommand(argsText);
    }

    for (int i = 0; i < m_sourceFilesList->count(); ++i) {
      assignment.sourceFiles.append(m_sourceFilesList->item(i)->text());
    }

    assignment.workingDirectory = m_workingDirEdit->text().trimmed();

    QString flagsText = m_compilerFlagsEdit->text().trimmed();
    if (!flagsText.isEmpty()) {
      assignment.compilerFlags = QProcess::splitCommand(flagsText);
    }

    for (int row = 0; row < m_envVarTable->rowCount(); ++row) {
      QTableWidgetItem *keyItem = m_envVarTable->item(row, 0);
      QTableWidgetItem *valItem = m_envVarTable->item(row, 1);
      if (keyItem && !keyItem->text().trimmed().isEmpty()) {
        assignment.customEnv[keyItem->text().trimmed()] =
            valItem ? valItem->text() : QString();
      }
    }

    assignment.preRunCommand = m_preRunCommandEdit->text().trimmed();
    assignment.postRunCommand = m_postRunCommandEdit->text().trimmed();

    RunTemplateManager::instance().assignTemplateToFile(m_filePath, assignment);
  }
  accept();
}

void RunTemplateSelector::onRemoveAssignment() {
  RunTemplateManager::instance().removeAssignment(m_filePath);
  accept();
}

void RunTemplateSelector::onAddSourceFile() {
  QFileInfo fi(m_filePath);
  QStringList files = QFileDialog::getOpenFileNames(
      this, "Add Source Files", fi.absolutePath(),
      "Source Files (*.c *.cc *.cpp *.cxx *.h *.hpp *.hxx *.s *.S *.asm "
      "*.f *.f90 *.rs *.go *.m *.mm);;All Files (*)");
  for (const QString &file : files) {
    m_sourceFilesList->addItem(file);
  }
}

void RunTemplateSelector::onRemoveSourceFile() {
  QList<QListWidgetItem *> selected = m_sourceFilesList->selectedItems();
  for (QListWidgetItem *item : selected) {
    delete item;
  }
}

void RunTemplateSelector::onBrowseWorkingDir() {
  QFileInfo fi(m_filePath);
  QString dir = QFileDialog::getExistingDirectory(
      this, "Select Working Directory", fi.absolutePath());
  if (!dir.isEmpty()) {
    m_workingDirEdit->setText(dir);
  }
}

void RunTemplateSelector::onAddEnvVar() {
  int row = m_envVarTable->rowCount();
  m_envVarTable->insertRow(row);
  m_envVarTable->setItem(row, 0, new QTableWidgetItem(""));
  m_envVarTable->setItem(row, 1, new QTableWidgetItem(""));
  m_envVarTable->editItem(m_envVarTable->item(row, 0));
}

void RunTemplateSelector::onRemoveEnvVar() {
  QList<QTableWidgetItem *> selected = m_envVarTable->selectedItems();
  QSet<int> rows;
  for (QTableWidgetItem *item : selected) {
    rows.insert(item->row());
  }
  QList<int> sortedRows = rows.values();
  std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
  for (int row : sortedRows) {
    m_envVarTable->removeRow(row);
  }
}

QString RunTemplateSelector::getSelectedTemplateId() const {
  return m_selectedTemplateId;
}

QStringList RunTemplateSelector::getCustomArgs() const {
  QString argsText = m_customArgsEdit->text().trimmed();
  if (argsText.isEmpty()) {
    return QStringList();
  }
  return QProcess::splitCommand(argsText);
}

void RunTemplateSelector::applyTheme(const Theme &theme) {
  setStyleSheet(UIStyleHelper::formDialogStyle(theme));

  for (QGroupBox *groupBox : findChildren<QGroupBox *>()) {
    groupBox->setStyleSheet(UIStyleHelper::groupBoxStyle(theme));
  }

  if (m_searchEdit) {
    m_searchEdit->setStyleSheet(UIStyleHelper::searchBoxStyle(theme));
  }

  if (m_languageCombo) {
    m_languageCombo->setStyleSheet(UIStyleHelper::comboBoxStyle(theme));
  }

  if (m_templateList) {
    m_templateList->setStyleSheet(UIStyleHelper::resultListStyle(theme));
  }

  for (QLineEdit *edit : findChildren<QLineEdit *>()) {
    if (edit != m_searchEdit) {
      edit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
    }
  }

  if (m_sourceFilesList) {
    m_sourceFilesList->setStyleSheet(UIStyleHelper::resultListStyle(theme));
  }

  if (m_envVarTable) {
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
    m_envVarTable->setStyleSheet(tableStyle);
  }

  if (m_descriptionLabel) {
    m_descriptionLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
  }
  if (m_commandLabel) {
    m_commandLabel->setStyleSheet(UIStyleHelper::subduedLabelStyle(theme));
  }

  if (m_okButton) {
    m_okButton->setStyleSheet(UIStyleHelper::primaryButtonStyle(theme));
  }
  if (m_cancelButton) {
    m_cancelButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
  }
  if (m_removeButton) {
    m_removeButton->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
  }

  for (QPushButton *btn :
       {m_addSourceFileBtn, m_removeSourceFileBtn, m_browseWorkingDirBtn,
        m_addEnvVarBtn, m_removeEnvVarBtn}) {
    if (btn) {
      btn->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
    }
  }
}
