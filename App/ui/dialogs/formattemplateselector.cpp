#include "formattemplateselector.h"
#include "../../format_templates/formattemplatemanager.h"
#include "../uistylehelper.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

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
    m_customArgsEdit->setText(assignment.customArgs.join(" "));
  }
}

FormatTemplateSelector::~FormatTemplateSelector() {}

void FormatTemplateSelector::setupUi() {
  setWindowTitle("Select Format Template");
  setMinimumSize(500, 400);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  QFileInfo fileInfo(m_filePath);
  QLabel *fileLabel =
      new QLabel(QString("File: <b>%1</b>").arg(fileInfo.fileName()));
  mainLayout->addWidget(fileLabel);

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

  mainLayout->addLayout(filterLayout);

  QGroupBox *templatesGroup = new QGroupBox("Available Formatters");
  QVBoxLayout *templatesLayout = new QVBoxLayout(templatesGroup);

  m_templateList = new QListWidget();
  connect(m_templateList, &QListWidget::itemClicked, this,
          &FormatTemplateSelector::onTemplateSelected);
  connect(m_templateList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { onAccept(); });
  templatesLayout->addWidget(m_templateList);

  mainLayout->addWidget(templatesGroup);

  QGroupBox *detailsGroup = new QGroupBox("Formatter Details");
  QVBoxLayout *detailsLayout = new QVBoxLayout(detailsGroup);

  m_descriptionLabel = new QLabel();
  m_descriptionLabel->setWordWrap(true);
  detailsLayout->addWidget(m_descriptionLabel);

  m_commandLabel = new QLabel();
  m_commandLabel->setWordWrap(true);
  m_commandLabel->setStyleSheet(
      "font-family: monospace; background-color: #1f2632; color: #e6edf3; "
      "padding: 6px; border-radius: 6px;");
  detailsLayout->addWidget(m_commandLabel);

  mainLayout->addWidget(detailsGroup);

  QHBoxLayout *argsLayout = new QHBoxLayout();
  argsLayout->addWidget(new QLabel("Custom Arguments:"));
  m_customArgsEdit = new QLineEdit();
  m_customArgsEdit->setPlaceholderText("Additional arguments (optional)");
  argsLayout->addWidget(m_customArgsEdit);
  mainLayout->addLayout(argsLayout);

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
    m_templateList->setCurrentRow(0);
    onTemplateSelected(m_templateList->currentItem());
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
    QStringList customArgs;
    QString argsText = m_customArgsEdit->text().trimmed();
    if (!argsText.isEmpty()) {
      customArgs =
          argsText.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    }

    FormatTemplateManager::instance().assignTemplateToFile(
        m_filePath, m_selectedTemplateId, customArgs);
  }
  accept();
}

void FormatTemplateSelector::onRemoveAssignment() {
  FormatTemplateManager::instance().removeAssignment(m_filePath);
  accept();
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

  if (m_customArgsEdit) {
    m_customArgsEdit->setStyleSheet(UIStyleHelper::lineEditStyle(theme));
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
}
