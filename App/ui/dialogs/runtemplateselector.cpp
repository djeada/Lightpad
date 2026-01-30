#include "runtemplateselector.h"
#include "../../run_templates/runtemplatemanager.h"

#include <QFileInfo>
#include <QSet>

RunTemplateSelector::RunTemplateSelector(const QString& filePath, QWidget* parent)
    : QDialog(parent)
    , m_filePath(filePath)
{
    setupUi();
    loadTemplates();
    
    // Pre-select current assignment if any
    FileTemplateAssignment assignment = RunTemplateManager::instance().getAssignmentForFile(filePath);
    if (!assignment.templateId.isEmpty()) {
        for (int i = 0; i < m_templateList->count(); ++i) {
            if (m_templateList->item(i)->data(Qt::UserRole).toString() == assignment.templateId) {
                m_templateList->setCurrentRow(i);
                break;
            }
        }
        m_customArgsEdit->setText(assignment.customArgs.join(" "));
    }
}

RunTemplateSelector::~RunTemplateSelector()
{
}

void RunTemplateSelector::setupUi()
{
    setWindowTitle("Select Run Template");
    setMinimumSize(500, 400);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // File info
    QFileInfo fileInfo(m_filePath);
    QLabel* fileLabel = new QLabel(QString("File: <b>%1</b>").arg(fileInfo.fileName()));
    mainLayout->addWidget(fileLabel);
    
    // Search and filter
    QHBoxLayout* filterLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Search templates...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &RunTemplateSelector::onFilterChanged);
    filterLayout->addWidget(m_searchEdit);
    
    m_languageCombo = new QComboBox();
    m_languageCombo->addItem("All Languages");
    connect(m_languageCombo, &QComboBox::currentTextChanged, this, &RunTemplateSelector::onLanguageFilterChanged);
    filterLayout->addWidget(m_languageCombo);
    
    mainLayout->addLayout(filterLayout);
    
    // Templates list
    QGroupBox* templatesGroup = new QGroupBox("Available Templates");
    QVBoxLayout* templatesLayout = new QVBoxLayout(templatesGroup);
    
    m_templateList = new QListWidget();
    connect(m_templateList, &QListWidget::itemClicked, this, &RunTemplateSelector::onTemplateSelected);
    connect(m_templateList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onAccept();
    });
    templatesLayout->addWidget(m_templateList);
    
    mainLayout->addWidget(templatesGroup);
    
    // Template details
    QGroupBox* detailsGroup = new QGroupBox("Template Details");
    QVBoxLayout* detailsLayout = new QVBoxLayout(detailsGroup);
    
    m_descriptionLabel = new QLabel();
    m_descriptionLabel->setWordWrap(true);
    detailsLayout->addWidget(m_descriptionLabel);
    
    m_commandLabel = new QLabel();
    m_commandLabel->setWordWrap(true);
    m_commandLabel->setStyleSheet("font-family: monospace; background-color: #f0f0f0; padding: 5px;");
    detailsLayout->addWidget(m_commandLabel);
    
    mainLayout->addWidget(detailsGroup);
    
    // Custom arguments
    QHBoxLayout* argsLayout = new QHBoxLayout();
    argsLayout->addWidget(new QLabel("Custom Arguments:"));
    m_customArgsEdit = new QLineEdit();
    m_customArgsEdit->setPlaceholderText("Additional arguments (optional)");
    argsLayout->addWidget(m_customArgsEdit);
    mainLayout->addLayout(argsLayout);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_removeButton = new QPushButton("Remove Assignment");
    connect(m_removeButton, &QPushButton::clicked, this, &RunTemplateSelector::onRemoveAssignment);
    buttonLayout->addWidget(m_removeButton);
    
    buttonLayout->addStretch();
    
    m_okButton = new QPushButton("OK");
    m_okButton->setDefault(true);
    connect(m_okButton, &QPushButton::clicked, this, &RunTemplateSelector::onAccept);
    buttonLayout->addWidget(m_okButton);
    
    m_cancelButton = new QPushButton("Cancel");
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    setLayout(mainLayout);
}

void RunTemplateSelector::loadTemplates()
{
    RunTemplateManager& manager = RunTemplateManager::instance();
    
    // Ensure templates are loaded
    if (manager.getAllTemplates().isEmpty()) {
        manager.loadTemplates();
    }
    
    // Collect unique languages
    QSet<QString> languages;
    for (const RunTemplate& tmpl : manager.getAllTemplates()) {
        if (!tmpl.language.isEmpty()) {
            languages.insert(tmpl.language);
        }
    }
    
    // Populate language combo
    QStringList sortedLanguages = languages.values();
    sortedLanguages.sort();
    for (const QString& lang : sortedLanguages) {
        m_languageCombo->addItem(lang);
    }
    
    // Pre-select language based on file extension
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

void RunTemplateSelector::filterTemplates()
{
    m_templateList->clear();
    
    RunTemplateManager& manager = RunTemplateManager::instance();
    QList<RunTemplate> templates = manager.getAllTemplates();
    
    for (const RunTemplate& tmpl : templates) {
        // Apply language filter
        if (!m_currentLanguage.isEmpty() && 
            m_currentLanguage != "All Languages" && 
            tmpl.language != m_currentLanguage) {
            continue;
        }
        
        // Apply text filter
        if (!m_currentFilter.isEmpty()) {
            bool matches = tmpl.name.contains(m_currentFilter, Qt::CaseInsensitive) ||
                          tmpl.description.contains(m_currentFilter, Qt::CaseInsensitive) ||
                          tmpl.language.contains(m_currentFilter, Qt::CaseInsensitive);
            if (!matches) {
                continue;
            }
        }
        
        QListWidgetItem* item = new QListWidgetItem();
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

void RunTemplateSelector::onTemplateSelected(QListWidgetItem* item)
{
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

void RunTemplateSelector::onFilterChanged(const QString& filter)
{
    m_currentFilter = filter;
    filterTemplates();
}

void RunTemplateSelector::onLanguageFilterChanged(const QString& language)
{
    m_currentLanguage = language;
    filterTemplates();
}

void RunTemplateSelector::onAccept()
{
    if (!m_selectedTemplateId.isEmpty()) {
        QStringList customArgs;
        QString argsText = m_customArgsEdit->text().trimmed();
        if (!argsText.isEmpty()) {
            customArgs = argsText.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
        }
        
        RunTemplateManager::instance().assignTemplateToFile(
            m_filePath, m_selectedTemplateId, customArgs);
    }
    accept();
}

void RunTemplateSelector::onRemoveAssignment()
{
    RunTemplateManager::instance().removeAssignment(m_filePath);
    accept();
}

QString RunTemplateSelector::getSelectedTemplateId() const
{
    return m_selectedTemplateId;
}

QStringList RunTemplateSelector::getCustomArgs() const
{
    QString argsText = m_customArgsEdit->text().trimmed();
    if (argsText.isEmpty()) {
        return QStringList();
    }
    return argsText.split(QRegExp("\\s+"), Qt::SkipEmptyParts);
}
