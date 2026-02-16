#ifndef DEBUGCONFIGURATIONDIALOG_H
#define DEBUGCONFIGURATIONDIALOG_H

#include "../../dap/debugconfiguration.h"
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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

class DebugConfigurationDialog : public QDialog {
  Q_OBJECT

public:
  explicit DebugConfigurationDialog(QWidget *parent = nullptr);
  ~DebugConfigurationDialog();

  void applyTheme(const Theme &theme);

private slots:
  void onConfigSelected(QListWidgetItem *current, QListWidgetItem *previous);
  void onAddConfig();
  void onRemoveConfig();
  void onDuplicateConfig();
  void onAddEnvVar();
  void onRemoveEnvVar();
  void onBrowseProgram();
  void onBrowseCwd();
  void onSave();

private:
  void setupUi();
  void loadConfigurations();
  void saveCurrentToModel();
  void loadConfigIntoForm(const DebugConfiguration &cfg);
  void clearForm();

  QListWidget *m_configList;
  QPushButton *m_addConfigBtn;
  QPushButton *m_removeConfigBtn;
  QPushButton *m_duplicateConfigBtn;

  QLineEdit *m_nameEdit;
  QComboBox *m_typeCombo;
  QComboBox *m_requestCombo;
  QLineEdit *m_programEdit;
  QPushButton *m_browseProgramBtn;
  QLineEdit *m_argsEdit;
  QLineEdit *m_cwdEdit;
  QPushButton *m_browseCwdBtn;
  QCheckBox *m_stopOnEntryCheck;

  QTableWidget *m_envTable;
  QPushButton *m_addEnvBtn;
  QPushButton *m_removeEnvBtn;

  QSpinBox *m_processIdSpin;
  QLineEdit *m_hostEdit;
  QSpinBox *m_portSpin;

  QLineEdit *m_preLaunchTaskEdit;
  QLineEdit *m_postDebugTaskEdit;

  QPlainTextEdit *m_adapterConfigEdit;

  QPushButton *m_saveButton;
  QPushButton *m_cancelButton;

  QString m_currentConfigName;
};

#endif
