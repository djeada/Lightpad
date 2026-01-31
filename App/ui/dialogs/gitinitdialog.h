#ifndef GITINITDIALOG_H
#define GITINITDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QGroupBox>

/**
 * @brief Dialog for initializing a new Git repository
 * 
 * Shows when a project is not yet a git repository
 * and provides options to initialize one.
 */
class GitInitDialog : public QDialog {
    Q_OBJECT

public:
    explicit GitInitDialog(const QString& projectPath, QWidget* parent = nullptr);
    ~GitInitDialog();

    /**
     * @brief Get the path where to initialize the repository
     */
    QString repositoryPath() const;

    /**
     * @brief Check if user wants to create an initial commit
     */
    bool createInitialCommit() const;

    /**
     * @brief Check if user wants to add a .gitignore file
     */
    bool addGitIgnore() const;

    /**
     * @brief Get the remote URL if user wants to add one
     */
    QString remoteUrl() const;

signals:
    /**
     * @brief Emitted when user confirms initialization
     */
    void initializeRequested(const QString& path);

private slots:
    void onInitClicked();
    void onCancelClicked();
    void onBrowseClicked();

private:
    void setupUI();
    void applyStyles();

    QString m_projectPath;
    QLineEdit* m_pathEdit;
    QPushButton* m_browseButton;
    QCheckBox* m_initialCommitCheckbox;
    QCheckBox* m_gitIgnoreCheckbox;
    QLineEdit* m_remoteEdit;
    QPushButton* m_initButton;
    QPushButton* m_cancelButton;
};

#endif // GITINITDIALOG_H
