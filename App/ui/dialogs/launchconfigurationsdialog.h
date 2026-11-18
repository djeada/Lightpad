#ifndef LAUNCHCONFIGURATIONSDIALOG_H
#define LAUNCHCONFIGURATIONSDIALOG_H

#include "../../dap/debugconfiguration.h"
#include "../../run_templates/launchplanner.h"
#include "../../run_templates/runtemplatemanager.h"
#include "styleddialog.h"

#include <QHash>
#include <QMap>
#include <memory>
#include <optional>

class IDebugAdapter;
class KeyValueTable;
class PythonEnvironmentWidget;
class QFormLayout;
class QStackedWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

// One place to decide how things start: the file in the editor (run
// template, arguments, environment, ...) and the workspace's saved launch
// configurations and compounds. Every entry can be run or debugged. Edits
// stay local until Save.
class LaunchConfigurationsDialog : public StyledDialog {
  Q_OBJECT
  friend class TestLaunchConfigurations;
  friend class TestDap;

public:
  struct Context {
    QString workspaceFolder;
    // The file the "Current File" entry configures; may be empty.
    QString currentFilePath;
    QString languageId;
    // Passed to RunTargetResolver for the current file's previews.
    QString preferredCMakeTarget;
    LaunchTarget selection;
  };

  explicit LaunchConfigurationsDialog(const Context &context,
                                      QWidget *parent = nullptr);

  // The entry that was selected when the dialog was accepted.
  LaunchTarget selectedTarget() const;
  // Set when the dialog was closed with Run or Debug.
  std::optional<LaunchMode> requestedLaunch() const {
    return m_requestedLaunch;
  }

  // Adds a configuration from a launch template and selects it; returns its
  // name, or an empty string when the template does not exist.
  QString addConfigurationFromTemplate(const QString &templateId);

  void applyTheme(const Theme &theme) override;

public slots:
  // Validates and writes everything; returns false and shows the problem
  // when something cannot be saved.
  bool save();
  // Lets the user pick a template and adds a configuration from it.
  void openTemplateGallery();

private:
  enum class EntryKind { None, File, Configuration, Compound, Section };

  void setupUi();
  QWidget *buildFilePage();
  QWidget *buildConfigurationPage();
  QWidget *buildCompoundPage();
  QWidget *buildEmptyPage();
  QWidget *buildFooter();

  void rebuildTree(EntryKind kind, int index);
  void onCurrentItemChanged(QTreeWidgetItem *current,
                            QTreeWidgetItem *previous);
  EntryKind kindOf(const QTreeWidgetItem *item) const;
  int indexOf(const QTreeWidgetItem *item) const;
  void showEntry(EntryKind kind, int index);
  bool commitCurrentEntry();
  void updateButtons();
  void showValidationError(const QString &message);

  // Current file
  void loadFileSettings();
  void populateFileTemplates();
  FileTemplateAssignment fileAssignmentFromForm() const;
  void scheduleFilePreview();
  void refreshFilePreview();
  void resetFileSettings();

  // Saved configurations
  void loadConfigurationIntoForm(const DebugConfiguration &config);
  bool configurationFromForm(DebugConfiguration *config, QString *error) const;
  void populateDebuggerCombo(const DebugConfiguration &config);
  QString selectedType() const;
  QString selectedAdapterId() const;
  std::shared_ptr<IDebugAdapter> selectedAdapter() const;
  void updateConfigurationFieldVisibility();
  void rebuildAdapterOptions();
  void scheduleConfigurationPreview();
  void refreshConfigurationPreview();
  void refreshDebuggerStatus();
  void addConfiguration(const DebugConfiguration &config);
  void duplicateCurrent();
  void removeCurrent();
  void populateAddMenu();

  // Compounds
  void addCompound();
  void loadCompoundIntoForm(const CompoundDebugConfiguration &compound);
  CompoundDebugConfiguration compoundFromForm() const;
  void moveCompoundEntry(int delta);

  bool nameInUse(const QString &name, EntryKind except, int exceptIndex) const;
  QString uniqueName(const QString &baseName) const;
  void launchSelected(LaunchMode mode);

  Context m_context;
  QList<DebugConfiguration> m_configurations;
  QList<CompoundDebugConfiguration> m_compounds;
  FileTemplateAssignment m_fileAssignment;
  EntryKind m_currentKind = EntryKind::None;
  int m_currentIndex = -1;
  bool m_loadingForm = false;
  std::optional<LaunchMode> m_requestedLaunch;

  // Layout
  QTreeWidget *m_tree = nullptr;
  QPushButton *m_addButton = nullptr;
  QMenu *m_addMenu = nullptr;
  QPushButton *m_duplicateButton = nullptr;
  QPushButton *m_removeButton = nullptr;
  QStackedWidget *m_pages = nullptr;
  QWidget *m_filePage = nullptr;
  QWidget *m_configurationPage = nullptr;
  QWidget *m_compoundPage = nullptr;
  QWidget *m_emptyPage = nullptr;
  QLabel *m_validationLabel = nullptr;
  QPushButton *m_runButton = nullptr;
  QPushButton *m_debugButton = nullptr;
  QPushButton *m_cancelButton = nullptr;
  QPushButton *m_saveButton = nullptr;

  // Current file page
  QLabel *m_fileTitle = nullptr;
  QLabel *m_filePath = nullptr;
  QComboBox *m_fileTemplateCombo = nullptr;
  QLabel *m_fileTemplateDescription = nullptr;
  QLineEdit *m_fileArgsEdit = nullptr;
  QLineEdit *m_fileCwdEdit = nullptr;
  KeyValueTable *m_fileEnvTable = nullptr;
  QFormLayout *m_fileForm = nullptr;
  QLineEdit *m_fileFlagsEdit = nullptr;
  QLineEdit *m_fileSourcesEdit = nullptr;
  QLineEdit *m_filePreEdit = nullptr;
  QLineEdit *m_filePostEdit = nullptr;
  PythonEnvironmentWidget *m_filePythonWidget = nullptr;
  // What the Python widget showed after loading, to tell edits from the
  // default paths it fills in.
  PythonEnvironmentPreference m_filePythonShown;
  QLabel *m_filePreview = nullptr;
  QPushButton *m_fileResetButton = nullptr;
  QTimer *m_filePreviewTimer = nullptr;

  // Configuration page
  QLineEdit *m_nameEdit = nullptr;
  QComboBox *m_debuggerCombo = nullptr;
  QComboBox *m_requestCombo = nullptr;
  QFormLayout *m_launchForm = nullptr;
  QLineEdit *m_programEdit = nullptr;
  QLineEdit *m_moduleEdit = nullptr;
  QLineEdit *m_runtimeArgsEdit = nullptr;
  QLineEdit *m_argsEdit = nullptr;
  QLineEdit *m_cwdEdit = nullptr;
  KeyValueTable *m_envTable = nullptr;
  QLineEdit *m_processEdit = nullptr;
  QLineEdit *m_hostEdit = nullptr;
  QSpinBox *m_portSpin = nullptr;
  QLineEdit *m_preLaunchEdit = nullptr;
  QLineEdit *m_postLaunchEdit = nullptr;
  PythonEnvironmentWidget *m_pythonWidget = nullptr;
  PythonEnvironmentPreference m_pythonShown;
  QWidget *m_debuggerGroup = nullptr;
  QCheckBox *m_stopOnEntryCheck = nullptr;
  QFormLayout *m_adapterOptionsLayout = nullptr;
  QMap<QString, QLineEdit *> m_adapterOptionEdits;
  QString m_adapterOptionsSignature;
  QToolButton *m_advancedToggle = nullptr;
  QPlainTextEdit *m_advancedEdit = nullptr;
  QLabel *m_configurationPreview = nullptr;
  QLabel *m_debuggerStatus = nullptr;
  QTimer *m_configurationPreviewTimer = nullptr;
  QTimer *m_statusTimer = nullptr;
  QHash<QString, QString> m_statusCache;
  std::shared_ptr<IDebugAdapter> m_pendingStatusAdapter;
  DebugConfiguration m_pendingStatusConfiguration;
  QString m_pendingStatusKey;

  // Compound page
  QLineEdit *m_compoundNameEdit = nullptr;
  QListWidget *m_compoundList = nullptr;
  QCheckBox *m_compoundStopAllCheck = nullptr;
};

#endif
