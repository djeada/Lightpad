#ifndef FORMATTEMPLATESELECTOR_H
#define FORMATTEMPLATESELECTOR_H

#include "../../settings/theme.h"
#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

class FormatTemplateSelector : public QDialog {
  Q_OBJECT

public:
  explicit FormatTemplateSelector(const QString &filePath,
                                  QWidget *parent = nullptr);
  ~FormatTemplateSelector();

  QString getSelectedTemplateId() const;
  QStringList getCustomArgs() const;

  void applyTheme(const Theme &theme);

private slots:
  void onTemplateSelected(QListWidgetItem *item);
  void onFilterChanged(const QString &filter);
  void onLanguageFilterChanged(const QString &language);
  void onAccept();
  void onRemoveAssignment();
  void onBrowseWorkingDir();
  void onAddEnvVar();
  void onRemoveEnvVar();

private:
  void setupUi();
  void loadTemplates();
  void filterTemplates();

  QString m_filePath;
  QString m_selectedTemplateId;

  QLineEdit *m_searchEdit;
  QComboBox *m_languageCombo;
  QListWidget *m_templateList;
  QLabel *m_descriptionLabel;
  QLabel *m_commandLabel;
  QLineEdit *m_customArgsEdit;
  QLineEdit *m_workingDirEdit;
  QPushButton *m_browseWorkingDirBtn;
  QTableWidget *m_envVarTable;
  QPushButton *m_addEnvVarBtn;
  QPushButton *m_removeEnvVarBtn;
  QLineEdit *m_preFormatCommandEdit;
  QLineEdit *m_postFormatCommandEdit;
  QPushButton *m_okButton;
  QPushButton *m_cancelButton;
  QPushButton *m_removeButton;

  QString m_currentFilter;
  QString m_currentLanguage;
};

#endif
