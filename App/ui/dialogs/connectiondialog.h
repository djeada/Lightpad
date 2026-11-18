#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include "../../database/dbconnection.h"
#include "../../database/dbconnectiontemplates.h"
#include "../../database/dbprojectdiscovery.h"
#include "../../database/dockerdiscovery.h"
#include "styleddialog.h"

#include <QFormLayout>
#include <QPointer>
#include <QRadioButton>
#include <QStackedWidget>
#include <QToolButton>

#include <functional>
#include <memory>

class QTreeWidget;
class QTreeWidgetItem;

// Creates or edits a database connection.
//
// A new connection starts on a chooser: databases found in the project
// (.env files, docker compose services, SQLite files), running Docker
// containers, and templates for local servers and managed cloud databases.
// A connection URL can be pasted at any point. The form then shows only the
// fields the chosen engine and transport use, keeps the URL in sync with
// them, and can test the connection before saving.
class ConnectionDialog : public StyledDialog {
  Q_OBJECT
  friend class TestConnectionDialog;

public:
  explicit ConnectionDialog(QWidget *parent = nullptr);
  ~ConnectionDialog() override;

  // Project folder scanned for connections on the chooser.
  void setWorkspaceFolder(const QString &folder);
  // Connections that already exist; detected entries matching one say so.
  void setSavedProfiles(const QVector<DbConnectionProfile> &profiles);

  // Edits an existing connection (opens on the form).
  void setProfile(const DbConnectionProfile &profile);
  bool startFromTemplate(const QString &templateId);
  // Fills the form from any supported connection string.
  bool applyConnectionString(const QString &text, QString *error = nullptr);

  DbConnectionProfile profile() const;
  // The password typed or found for this session ("" for no password).
  QString password() const;
  bool connectAfterSave() const { return m_connectAfterSave; }
  bool isChoosing() const;
  void setNameSuggestionProvider(std::function<QString(const QString &)> fn) {
    m_uniqueName = std::move(fn);
  }

  void applyTheme(const Theme &theme) override;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  enum class ChoiceKind { Template = 1, Detected, Container };

  void buildChooser();
  void buildForm();
  void populateChooser();
  void filterChooser(const QString &text);
  void selectFirstChoice();
  void updateChoiceDetails();
  void chooseCurrent();
  void useChooserUrl();
  void showChooser();
  void showForm();

  // Loads a starting point into the form.
  void loadProfile(const DbConnectionProfile &profile,
                   const QString &password = QString(),
                   bool passwordFound = false);
  void applyTemplateHints(const DbConnectionTemplate &entry);
  void clearTemplateHints();

  void onEngineChanged();
  void onTransportChanged();
  void onFieldEdited();
  void onUrlEdited(const QString &text);
  void onPasswordSourceChanged();
  void updateVisibility();
  void updateAutoName();
  void updateUrlField(bool force = false);
  void updateHeader();
  void updateContainerChoices();
  void refreshContainers();
  void onContainersFound(const QVector<DockerContainer> &containers);
  void onCredentials(const QString &container,
                     const DockerCredentialHints &hints);
  void browseFile();
  void newFile();
  void testConnection();
  void finishTest(const QString &text, bool ok);
  void setTestStatus(const QString &text, UIStyleHelper::Tone tone);
  void accept() override;
  QString suggestedName() const;
  QString savedNameFor(const DbConnectionProfile &profile) const;
  DbEngine currentEngine() const;
  QString linkHtml(const QString &url, const QString &text) const;

  QStackedWidget *m_pages = nullptr;

  // Chooser
  QWidget *m_chooserPage = nullptr;
  QLineEdit *m_chooserUrl = nullptr;
  QPushButton *m_chooserUseUrl = nullptr;
  QLabel *m_chooserUrlStatus = nullptr;
  QLineEdit *m_search = nullptr;
  QTreeWidget *m_choices = nullptr;
  QLabel *m_choiceTitle = nullptr;
  QLabel *m_choiceBadge = nullptr;
  QLabel *m_choiceDescription = nullptr;
  QLabel *m_choicePreview = nullptr;
  QLabel *m_choiceNote = nullptr;
  QPushButton *m_continueButton = nullptr;
  QTreeWidgetItem *m_dockerGroup = nullptr;

  // Form
  QWidget *m_formPage = nullptr;
  QPushButton *m_backButton = nullptr;
  QLabel *m_formTitle = nullptr;
  QLabel *m_formBadge = nullptr;
  QLabel *m_help = nullptr;
  QFormLayout *m_form = nullptr;
  QLineEdit *m_url = nullptr;
  QPushButton *m_copyUrl = nullptr;
  QLabel *m_urlHint = nullptr;
  QLineEdit *m_name = nullptr;
  QComboBox *m_color = nullptr;
  QComboBox *m_engine = nullptr;
  QWidget *m_transportRow = nullptr;
  QRadioButton *m_direct = nullptr;
  QRadioButton *m_docker = nullptr;
  QWidget *m_containerRow = nullptr;
  QComboBox *m_container = nullptr;
  QPushButton *m_refreshContainers = nullptr;
  QWidget *m_hostRow = nullptr;
  QLineEdit *m_host = nullptr;
  QSpinBox *m_port = nullptr;
  QWidget *m_fileRow = nullptr;
  QLineEdit *m_file = nullptr;
  QPushButton *m_browse = nullptr;
  QPushButton *m_newFile = nullptr;
  QLineEdit *m_database = nullptr;
  QLineEdit *m_user = nullptr;
  QWidget *m_passwordRow = nullptr;
  QLineEdit *m_password = nullptr;
  QComboBox *m_passwordSource = nullptr;
  QLineEdit *m_passwordEnv = nullptr;
  QLabel *m_passwordHint = nullptr;
  QComboBox *m_sslMode = nullptr;
  QCheckBox *m_trustCertificate = nullptr;
  QWidget *m_safetyRow = nullptr;
  QCheckBox *m_readOnly = nullptr;
  QCheckBox *m_confirmDestructive = nullptr;
  QToolButton *m_advancedToggle = nullptr;
  QLineEdit *m_clientPath = nullptr;
  QLineEdit *m_extraArgs = nullptr;
  QPushButton *m_testButton = nullptr;
  QLabel *m_testStatus = nullptr;
  QPushButton *m_cancelButton = nullptr;
  QPushButton *m_saveButton = nullptr;
  QPushButton *m_saveConnectButton = nullptr;

  DockerDiscovery *m_discovery = nullptr;
  QVector<DockerContainer> m_containers;
  bool m_dockerSearched = false;
  bool m_dockerFailed = false;
  QString m_dockerMessage;
  QVector<DbDetectedConnection> m_detected;
  QVector<DbConnectionProfile> m_saved;
  QString m_workspaceFolder;

  QString m_id;
  QString m_templateName;
  QString m_templateId;
  bool m_editing = false;
  bool m_nameEdited = false;
  bool m_loading = false;
  bool m_connectAfterSave = false;
  std::function<QString(const QString &)> m_uniqueName;
  std::unique_ptr<DbConnection> m_testConnection;
  QMetaObject::Connection m_testResultConnection;
  quint64 m_testRequest = 0;
};

#endif
