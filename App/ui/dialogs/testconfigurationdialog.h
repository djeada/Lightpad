#ifndef TESTCONFIGURATIONDIALOG_H
#define TESTCONFIGURATIONDIALOG_H

#include "../../test_templates/testconfiguration.h"
#include "styleddialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>

class TestConfigurationDialog : public StyledDialog {
  Q_OBJECT

public:
  explicit TestConfigurationDialog(QWidget *parent = nullptr,
                                   const QString &initialConfigId = QString());
  ~TestConfigurationDialog() override = default;

  void applyTheme(const Theme &theme) override;

private slots:
  void onConfigSelected(QListWidgetItem *current, QListWidgetItem *previous);
  void onAddConfiguration();
  void onAddFromTemplate(const QString &templateId);
  void onDuplicateConfiguration();
  void onRemoveConfiguration();
  void onAddEnvVar();
  void onRemoveEnvVar();
  void onSave();

private:
  void setupUi();
  void populateTemplateCombo();
  void reloadConfigurationList(const QString &selectedId = QString());
  void loadConfigurationIntoForm(const TestConfiguration &cfg,
                                 bool isUserConfig, bool isDraftConfig);
  void clearForm();
  TestConfiguration formConfiguration() const;
  QStringList parseArgs(QPlainTextEdit *edit) const;
  void setArgs(QPlainTextEdit *edit, const QStringList &args);
  QStringList parseExtensions(const QString &text) const;
  QString uniqueName(const QString &baseName,
                     const QString &excludeId = QString()) const;
  void setOriginText();
  bool currentSelectionIsUserConfiguration() const;

  QListWidget *m_configList;
  QLabel *m_originLabel;
  QPushButton *m_addConfigBtn;
  QPushButton *m_addTemplateBtn;
  QPushButton *m_duplicateConfigBtn;
  QPushButton *m_removeConfigBtn;

  QLineEdit *m_nameEdit;
  QComboBox *m_templateCombo;
  QCheckBox *m_defaultCheck;
  QLineEdit *m_languageEdit;
  QLineEdit *m_extensionsEdit;
  QComboBox *m_outputFormatCombo;
  QLineEdit *m_testFilePatternEdit;
  QLineEdit *m_commandEdit;
  QPlainTextEdit *m_argsEdit;
  QLineEdit *m_workingDirectoryEdit;
  QLineEdit *m_discoveryDirectoryEdit;
  QPlainTextEdit *m_runFileArgsEdit;
  QPlainTextEdit *m_runSingleTestArgsEdit;
  QPlainTextEdit *m_runFailedArgsEdit;
  QPlainTextEdit *m_runSuiteArgsEdit;
  QTableWidget *m_envTable;
  QPushButton *m_addEnvBtn;
  QPushButton *m_removeEnvBtn;
  QLineEdit *m_preLaunchTaskEdit;
  QLineEdit *m_postRunTaskEdit;

  QPushButton *m_saveButton;
  QPushButton *m_cancelButton;

  QString m_initialConfigId;
  QString m_currentConfigId;
  bool m_currentIsUserConfig = false;
  bool m_currentIsDraftConfig = false;
  TestConfiguration m_loadedConfig;
};

#endif
