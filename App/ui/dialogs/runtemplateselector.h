#ifndef RUNTEMPLATESELECTOR_H
#define RUNTEMPLATESELECTOR_H

#include "../../settings/theme.h"
#include <QCheckBox>
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
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

class RunTemplateSelector : public QDialog {
  Q_OBJECT

public:
  explicit RunTemplateSelector(const QString &filePath,
                               QWidget *parent = nullptr);
  ~RunTemplateSelector();

  QString getSelectedTemplateId() const;
  QStringList getCustomArgs() const;

  void applyTheme(const Theme &theme);

private slots:
  void onTemplateSelected(QListWidgetItem *item);
  void onFilterChanged(const QString &filter);
  void onLanguageFilterChanged(const QString &language);
  void onAccept();
  void onRemoveAssignment();
  void onAddSourceFile();
  void onRemoveSourceFile();
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

  QListWidget *m_sourceFilesList;
  QPushButton *m_addSourceFileBtn;
  QPushButton *m_removeSourceFileBtn;

  QLineEdit *m_workingDirEdit;
  QPushButton *m_browseWorkingDirBtn;

  QLineEdit *m_compilerFlagsEdit;

  QTableWidget *m_envVarTable;
  QPushButton *m_addEnvVarBtn;
  QPushButton *m_removeEnvVarBtn;

  QLineEdit *m_preRunCommandEdit;
  QLineEdit *m_postRunCommandEdit;

  QPushButton *m_okButton;
  QPushButton *m_cancelButton;
  QPushButton *m_removeButton;

  QString m_currentFilter;
  QString m_currentLanguage;
};

#endif
