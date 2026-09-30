#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include "../../database/dbconnection.h"
#include "../../database/dockerdiscovery.h"
#include "styleddialog.h"

#include <QFormLayout>
#include <QListWidget>
#include <QRadioButton>
#include <QScrollArea>
#include <QToolButton>

#include <memory>

class ConnectionDialog : public StyledDialog {
  Q_OBJECT

public:
  explicit ConnectionDialog(QWidget *parent = nullptr);

  void setProfile(const DbConnectionProfile &profile);
  DbConnectionProfile profile() const;

  QString password() const { return m_password->text(); }
  bool connectAfterSave() const { return m_connectAfterSave; }
  void setNameSuggestionProvider(std::function<QString(const QString &)> fn) {
    m_uniqueName = std::move(fn);
  }

  void applyTheme(const Theme &theme) override;

private slots:
  void onEngineChanged();
  void onTransportChanged();
  void refreshContainers();
  void onContainersFound(const QVector<DockerContainer> &containers);
  void onContainerPicked();
  void onCredentials(const QString &container,
                     const DockerCredentialHints &hints);
  void browseFile();
  void newFile();
  void testConnection();
  void accept() override;

private:
  void buildUi();
  void updateVisibility();
  void updateAutoName();
  void setTestStatus(const QString &text, bool ok, bool busy = false);
  QString suggestedName() const;

  QVector<DockerContainer> m_containers;
  DockerDiscovery *m_discovery;

  QGroupBox *m_dockerBox;
  QWidget *m_body;
  QScrollArea *m_scroll;
  QWidget *m_transportRow;
  QWidget *m_hostRow;
  QWidget *m_fileRow;
  QListWidget *m_containerList;
  QLabel *m_dockerStatus;
  QPushButton *m_refreshDocker;

  QLineEdit *m_name;
  QComboBox *m_engine;
  QRadioButton *m_direct;
  QRadioButton *m_docker;
  QComboBox *m_container;
  QLineEdit *m_host;
  QSpinBox *m_port;
  QLineEdit *m_user;
  QLineEdit *m_password;
  QLineEdit *m_database;
  QLineEdit *m_file;
  QPushButton *m_browse;
  QPushButton *m_newFile;
  QCheckBox *m_readOnly;
  QCheckBox *m_confirmDestructive;
  QCheckBox *m_trustCertificate;
  QLineEdit *m_clientPath;
  QLineEdit *m_extraArgs;
  QComboBox *m_color;
  QLabel *m_passwordHint;
  QLabel *m_testStatus;
  QPushButton *m_testButton;
  QPushButton *m_saveButton;
  QPushButton *m_saveConnectButton;
  QFormLayout *m_form;

  QString m_id;
  bool m_nameEdited = false;
  bool m_settingProfile = false;
  bool m_connectAfterSave = false;
  std::function<QString(const QString &)> m_uniqueName;
  std::unique_ptr<DbConnection> m_testConnection;
};

#endif
