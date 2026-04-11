#ifndef DEBUGCONFIGURATIONDIALOG_H
#define DEBUGCONFIGURATIONDIALOG_H

#include "styleddialog.h"
#include "../../dap/debugconfiguration.h"
#include "../../dap/idebugadapter.h"
#include "../widgets/pythonenvironmentwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
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
#include <memory>

class DebugConfigurationDialog : public StyledDialog {
  Q_OBJECT

public:
  explicit DebugConfigurationDialog(QWidget *parent = nullptr);
  ~DebugConfigurationDialog();

  void applyTheme(const Theme &theme) override;

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
  void updateAdapterUi();

private:
  void setupUi();
  void loadConfigurations();
  void saveCurrentToModel();
  void loadConfigIntoForm(const DebugConfiguration &cfg);
  void clearForm();
  DebugConfiguration
  createTemplateConfiguration(const QString &templateId) const;
  void addConfigurationFromTemplate(const QString &templateId);
  std::shared_ptr<IDebugAdapter> selectedAdapter() const;
  void rebuildAdapterOptionsUi(const std::shared_ptr<IDebugAdapter> &adapter);
  void populateAdapterOptions(const DebugConfiguration &cfg);
  void applyAdapterOptionsToConfig(DebugConfiguration &cfg) const;

  QListWidget *m_configList;
  QPushButton *m_addConfigBtn;
  QPushButton *m_addTemplateBtn;
  QPushButton *m_removeConfigBtn;
  QPushButton *m_duplicateConfigBtn;

  QLineEdit *m_nameEdit;
  QComboBox *m_adapterCombo;
  QComboBox *m_typeCombo;
  QComboBox *m_requestCombo;
  QLineEdit *m_programEdit;
  QPushButton *m_browseProgramBtn;
  QLineEdit *m_argsEdit;
  QLineEdit *m_cwdEdit;
  QPushButton *m_browseCwdBtn;
  PythonEnvironmentWidget *m_pythonEnvironmentWidget;
  QGroupBox *m_adapterOptionsGroup;
  QFormLayout *m_adapterOptionsLayout;
  QLabel *m_adapterStatusLabel;
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
  QString m_adapterOptionsSignature;
  QMap<QString, QLineEdit *> m_adapterOptionEdits;
};

#endif
