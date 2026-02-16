#include "formattemplateselector.h"
#include "../../format_templates/formattemplatemanager.h"
#include "../uistylehelper.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <functional>

FormatTemplateSelector::FormatTemplateSelector(const QString &filePath,
                                               QWidget *parent)
    : QDialog(parent), m_filePath(filePath) {
  setupUi();
  loadTemplates();

  FileFormatAssignment assignment =
      FormatTemplateManager::instance().getAssignmentForFile(filePath);
  if (!assignment.templateId.isEmpty()) {
    for (int i = 0; i < m_templateList->count(); ++i) {
      if (m_templateList->item(i)->data(Qt::UserRole).toString() ==
          assignment.templateId) {
        m_templateList->setCurrentRow(i);
        break;
      }
    }
  }

  m_customArgsEdit->setText(assignment.customArgs.join(" "));
  m_workingDirEdit->setText(assignment.workingDirectory);
  m_preFormatCommandEdit->setText(assignment.preFormatCommand);
  m_postFormatCommandEdit->setText(assignment.postFormatCommand);

  int row = 0;
  for (auto it = assignment.customEnv.begin(); it != assignment.customEnv.end();
       ++it) {
    m_envVarTable->insertRow(row);
    m_envVarTable->setItem(row, 0, new QTableWidgetItem(it.key()));
    m_envVarTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    ++row;
  }
}

FormatTemplateSelector::~FormatTemplateSelector() {}

void FormatTemplateSelector::setupUi() {
  setWindowTitle("Format Configuration");
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
  m_searchEdit->setPlaceholderText("Search formatters...");
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &FormatTemplateSelector::onFilterChanged);
  filterLayout->addWidget(m_searchEdit);

  m_languageCombo = new QComboBox();
  m_languageCombo->addItem("All Languages");
  connect(m_languageCombo, &QComboBox::currentTextChanged, this,
          &FormatTemplateSelector::onLanguageFilterChanged);
  filterLayout->addWidget(m_languageCombo);
  leftLayout->addLayout(filterLayout);

  QGroupBox *templatesGroup = new QGroupBox("Available Formatters");
  QVBoxLayout *templatesLayout = new QVBoxLayout(templatesGroup);

  m_templateList = new QListWidget();
  connect(m_templateList, &QListWidget::itemClicked, this,
          &FormatTemplateSelector::onTemplateSelected);
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
      "Additional formatter arguments (e.g., --line-length 120)");
  argsLayout->addWidget(m_customArgsEdit);
  rightLayout->addWidget(argsGroup);

  QGroupBox *wdGroup = new QGroupBox("Working Directory");
  QHBoxLayout *wdLayout = new QHBoxLayout(wdGroup);
  m_workingDirEdit = new QLineEdit();
  m_workingDirEdit->setPlaceholderText(
      "Override working directory (default: ${fileDir})");
  m_browseWorkingDirBtn = new QPushButton("Browse...");
  connect(m_browseWorkingDirBtn, &QPushButton::clicked, this,
          &FormatTemplateSelector::onBrowseWorkingDir);
  wdLayout->addWidget(m_workingDirEdit);
  wdLayout->addWidget(m_browseWorkingDirBtn);
  rightLayout->addWidget(wdGroup);

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
          &FormatTemplateSelector::onAddEnvVar);
  connect(m_removeEnvVarBtn, &QPushButton::clicked, this,
          &FormatTemplateSelector::onRemoveEnvVar);
  envButtonLayout->addWidget(m_addEnvVarBtn);
  envButtonLayout->addWidget(m_removeEnvVarBtn);
  envButtonLayout->addStretch();
  envLayout->addLayout(envButtonLayout);
  rightLayout->addWidget(envGroup);

  QGroupBox *hooksGroup = new QGroupBox("Pre/Post Format Commands");
  QVBoxLayout *hooksLayout = new QVBoxLayout(hooksGroup);

  hooksLayout->addWidget(new QLabel("Pre-format command:"));
  m_preFormatCommandEdit = new QLineEdit();
  m_preFormatCommandEdit->setPlaceholderText(
      "Command to run before formatter (optional)");
  hooksLayout->addWidget(m_preFormatCommandEdit);

  hooksLayout->addWidget(new QLabel("Post-format command:"));
  m_postFormatCommandEdit = new QLineEdit();
  m_postFormatCommandEdit->setPlaceholderText(
      "Command to run after formatter (optional)");
  hooksLayout->addWidget(m_postFormatCommandEdit);
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
          &FormatTemplateSelector::onRemoveAssignment);
  buttonLayout->addWidget(m_removeButton);

  buttonLayout->addStretch();

  m_okButton = new QPushButton("OK");
  m_okButton->setDefault(true);
  connect(m_okButton, &QPushButton::clicked, this,
          &FormatTemplateSelector::onAccept);
  buttonLayout->addWidget(m_okButton);

  m_cancelButton = new QPushButton("Cancel");
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  buttonLayout->addWidget(m_cancelButton);

  mainLayout->addLayout(buttonLayout);
  setLayout(mainLayout);
}

void FormatTemplateSelector::loadTemplates() {
  FormatTemplateManager &manager = FormatTemplateManager::instance();

  if (manager.getAllTemplates().isEmpty()) {
    manager.loadTemplates();
  }

  QSet<QString> languages;
  for (const FormatTemplate &tmpl : manager.getAllTemplates()) {
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
  QList<FormatTemplate> matchingTemplates =
      manager.getTemplatesForExtension(ext);
  if (!matchingTemplates.isEmpty()) {
    QString matchedLanguage = matchingTemplates.first().language;
    int idx = m_languageCombo->findText(matchedLanguage);
    if (idx >= 0) {
      m_languageCombo->setCurrentIndex(idx);
    }
  }

  filterTemplates();
}

void FormatTemplateSelector::filterTemplates() {
  m_templateList->clear();

  FormatTemplateManager &manager = FormatTemplateManager::instance();
  QList<FormatTemplate> templates = manager.getAllTemplates();

  for (const FormatTemplate &tmpl : templates) {

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
    bool restoredSelection = false;
    if (!m_selectedTemplateId.isEmpty()) {
      for (int i = 0; i < m_templateList->count(); ++i) {
        if (m_templateList->item(i)->data(Qt::UserRole).toString() ==
            m_selectedTemplateId) {
          m_templateList->setCurrentRow(i);
          restoredSelection = true;
          break;
        }
      }
    }
    if (!restoredSelection) {
      m_templateList->setCurrentRow(0);
    }
    onTemplateSelected(m_templateList->currentItem());
  } else {
    onTemplateSelected(nullptr);
  }
}

void FormatTemplateSelector::onTemplateSelected(QListWidgetItem *item) {
  if (!item) {
    m_descriptionLabel->clear();
    m_commandLabel->clear();
    m_selectedTemplateId.clear();
    return;
  }

  QString templateId = item->data(Qt::UserRole).toString();
  FormatTemplate tmpl =
      FormatTemplateManager::instance().getTemplateById(templateId);

  m_selectedTemplateId = templateId;
  m_descriptionLabel->setText(tmpl.description);

  QString commandPreview = tmpl.command;
  if (!tmpl.args.isEmpty()) {
    commandPreview += " " + tmpl.args.join(" ");
  }
  m_commandLabel->setText(QString("<b>Command:</b> %1").arg(commandPreview));
}

void FormatTemplateSelector::onFilterChanged(const QString &filter) {
  m_currentFilter = filter;
  filterTemplates();
}

void FormatTemplateSelector::onLanguageFilterChanged(const QString &language) {
  m_currentLanguage = language;
  filterTemplates();
}

void FormatTemplateSelector::onAccept() {
  if (!m_selectedTemplateId.isEmpty()) {
    FileFormatAssignment assignment;
    assignment.filePath = m_filePath;
    assignment.templateId = m_selectedTemplateId;

    QString argsText = m_customArgsEdit->text().trimmed();
    if (!argsText.isEmpty()) {
      assignment.customArgs =
          argsText.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    }

    assignment.workingDirectory = m_workingDirEdit->text().trimmed();
    assignment.preFormatCommand = m_preFormatCommandEdit->text().trimmed();
    assignment.postFormatCommand = m_postFormatCommandEdit->text().trimmed();

    for (int row = 0; row < m_envVarTable->rowCount(); ++row) {
      QTableWidgetItem *keyItem = m_envVarTable->item(row, 0);
      QTableWidgetItem *valItem = m_envVarTable->item(row, 1);
      if (keyItem && !keyItem->text().trimmed().isEmpty()) {
        assignment.customEnv[keyItem->text().trimmed()] =
            valItem ? valItem->text() : QString();
      }
    }

    FormatTemplateManager::instance().assignTemplateToFile(m_filePath,
                                                           assignment);
  }
  accept();
}

void FormatTemplateSelector::onRemoveAssignment() {
  FormatTemplateManager::instance().removeAssignment(m_filePath);
  accept();
}

void FormatTemplateSelector::onBrowseWorkingDir() {
  QFileInfo fi(m_filePath);
  QString dir = QFileDialog::getExistingDirectory(
      this, "Select Working Directory", fi.absolutePath());
  if (!dir.isEmpty()) {
    m_workingDirEdit->setText(dir);
  }
}

void FormatTemplateSelector::onAddEnvVar() {
  int row = m_envVarTable->rowCount();
  m_envVarTable->insertRow(row);
  m_envVarTable->setItem(row, 0, new QTableWidgetItem(""));
  m_envVarTable->setItem(row, 1, new QTableWidgetItem(""));
  m_envVarTable->editItem(m_envVarTable->item(row, 0));
}

void FormatTemplateSelector::onRemoveEnvVar() {
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

QString FormatTemplateSelector::getSelectedTemplateId() const {
  return m_selectedTemplateId;
}

QStringList FormatTemplateSelector::getCustomArgs() const {
  QString argsText = m_customArgsEdit->text().trimmed();
  if (argsText.isEmpty()) {
    return QStringList();
  }
  return argsText.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
}

void FormatTemplateSelector::applyTheme(const Theme &theme) {
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
       {m_browseWorkingDirBtn, m_addEnvVarBtn, m_removeEnvVarBtn}) {
    if (btn) {
      btn->setStyleSheet(UIStyleHelper::secondaryButtonStyle(theme));
    }
  }
}
