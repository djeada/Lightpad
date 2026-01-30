#ifndef RUNTEMPLATESELECTOR_H
#define RUNTEMPLATESELECTOR_H

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>

class RunTemplateSelector : public QDialog {
    Q_OBJECT

public:
    explicit RunTemplateSelector(const QString& filePath, QWidget* parent = nullptr);
    ~RunTemplateSelector();
    
    QString getSelectedTemplateId() const;
    QStringList getCustomArgs() const;

private slots:
    void onTemplateSelected(QListWidgetItem* item);
    void onFilterChanged(const QString& filter);
    void onLanguageFilterChanged(const QString& language);
    void onAccept();
    void onRemoveAssignment();

private:
    void setupUi();
    void loadTemplates();
    void filterTemplates();
    
    QString m_filePath;
    QString m_selectedTemplateId;
    
    QLineEdit* m_searchEdit;
    QComboBox* m_languageCombo;
    QListWidget* m_templateList;
    QLabel* m_descriptionLabel;
    QLabel* m_commandLabel;
    QLineEdit* m_customArgsEdit;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_removeButton;
    
    QString m_currentFilter;
    QString m_currentLanguage;
};

#endif // RUNTEMPLATESELECTOR_H
