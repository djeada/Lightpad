#ifndef GITINTEGRATION_H
#define GITINTEGRATION_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QProcess>

/**
 * @brief Timeout for git command execution in milliseconds
 */
constexpr int GIT_COMMAND_TIMEOUT_MS = 5000;

/**
 * @brief Git file status types
 */
enum class GitFileStatus {
    Untracked,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Unmerged,
    Ignored,
    Clean
};

/**
 * @brief Information about a single file's git status
 */
struct GitFileInfo {
    QString filePath;
    GitFileStatus indexStatus;   // Status in staging area
    GitFileStatus workTreeStatus; // Status in working tree
    QString originalPath;        // For renamed files
};

/**
 * @brief Information about a git diff hunk for a specific line
 */
struct GitDiffLineInfo {
    int lineNumber;
    enum class Type {
        Added,
        Modified,
        Deleted
    } type;
};

/**
 * @brief Git branch information
 */
struct GitBranchInfo {
    QString name;
    bool isCurrent;
    bool isRemote;
    QString trackingBranch;
    int aheadCount;
    int behindCount;
};

/**
 * @brief Git stash entry information
 */
struct GitStashEntry {
    int index;
    QString message;
    QString branch;
    QString commitHash;
};

/**
 * @brief Main Git integration class for Lightpad
 * 
 * Provides functionality to interact with Git repositories including
 * status checking, staging, committing, and branch management.
 */
class GitIntegration : public QObject {
    Q_OBJECT

public:
    explicit GitIntegration(QObject* parent = nullptr);
    ~GitIntegration();

    /**
     * @brief Set the repository path
     * @param path Path to the repository root or any file within it
     * @return true if path is within a git repository
     */
    bool setRepositoryPath(const QString& path);

    /**
     * @brief Get the current repository root path
     */
    QString repositoryPath() const;

    /**
     * @brief Check if currently in a valid git repository
     */
    bool isValidRepository() const;

    /**
     * @brief Get the current branch name
     */
    QString currentBranch() const;

    /**
     * @brief Get file status for all tracked and untracked files
     */
    QList<GitFileInfo> getStatus() const;

    /**
     * @brief Get status for a specific file
     */
    GitFileInfo getFileStatus(const QString& filePath) const;

    /**
     * @brief Get diff line information for a file (for gutter display)
     * @param filePath Path to the file
     * @return List of line information for added/modified/deleted lines
     */
    QList<GitDiffLineInfo> getDiffLines(const QString& filePath) const;

    /**
     * @brief Get list of all branches
     */
    QList<GitBranchInfo> getBranches() const;

    /**
     * @brief Stage a file for commit
     */
    bool stageFile(const QString& filePath);

    /**
     * @brief Stage all files
     */
    bool stageAll();

    /**
     * @brief Unstage a file
     */
    bool unstageFile(const QString& filePath);

    /**
     * @brief Commit staged changes
     * @param message Commit message
     */
    bool commit(const QString& message);

    /**
     * @brief Checkout a branch
     */
    bool checkoutBranch(const QString& branchName);

    /**
     * @brief Create a new branch
     */
    bool createBranch(const QString& branchName, bool checkout = true);

    /**
     * @brief Delete a branch
     */
    bool deleteBranch(const QString& branchName, bool force = false);

    /**
     * @brief Merge a branch into the current branch
     * @param branchName The branch to merge
     * @return true if merge was successful
     */
    bool mergeBranch(const QString& branchName);

    /**
     * @brief Get the diff output for a file
     */
    QString getFileDiff(const QString& filePath) const;

    /**
     * @brief Discard changes in a file (restore to HEAD)
     */
    bool discardChanges(const QString& filePath);

    // ==================== Remote Operations ====================

    /**
     * @brief Fetch from remote repository
     * @param remote The remote name (defaults to "origin")
     * @return true if fetch was successful
     */
    bool fetch(const QString& remote = "origin");

    /**
     * @brief Pull from remote repository
     * @param remote The remote name (defaults to "origin")
     * @param branch The branch to pull (defaults to current branch)
     * @return true if pull was successful
     */
    bool pull(const QString& remote = "origin", const QString& branch = "");

    /**
     * @brief Push to remote repository
     * @param remote The remote name (defaults to "origin")
     * @param branch The branch to push (defaults to current branch)
     * @param setUpstream Set upstream tracking reference
     * @return true if push was successful
     */
    bool push(const QString& remote = "origin", const QString& branch = "", bool setUpstream = false);

    /**
     * @brief Get list of configured remotes
     * @return List of remote names
     */
    QStringList getRemotes() const;

    // ==================== Stash Operations ====================

    /**
     * @brief Stash current changes
     * @param message Optional stash message
     * @param includeUntracked Include untracked files in stash
     * @return true if stash was successful
     */
    bool stash(const QString& message = "", bool includeUntracked = false);

    /**
     * @brief Pop the most recent stash
     * @param index Stash index to pop (default 0 for most recent)
     * @return true if pop was successful
     */
    bool stashPop(int index = 0);

    /**
     * @brief Apply a stash without removing it
     * @param index Stash index to apply
     * @return true if apply was successful
     */
    bool stashApply(int index = 0);

    /**
     * @brief Drop a stash entry
     * @param index Stash index to drop
     * @return true if drop was successful
     */
    bool stashDrop(int index = 0);

    /**
     * @brief List all stash entries
     * @return List of stash entries
     */
    QList<GitStashEntry> stashList() const;

    /**
     * @brief Clear all stash entries
     * @return true if clear was successful
     */
    bool stashClear();

    /**
     * @brief Refresh the repository status
     */
    void refresh();

signals:
    /**
     * @brief Emitted when repository status changes
     */
    void statusChanged();

    /**
     * @brief Emitted when current branch changes
     */
    void branchChanged(const QString& branchName);

    /**
     * @brief Emitted when an error occurs
     */
    void errorOccurred(const QString& error);

    /**
     * @brief Emitted when a git operation completes successfully
     */
    void operationCompleted(const QString& operation);

private:
    QString m_repositoryPath;
    bool m_isValid;
    QString m_currentBranch;
    QList<GitFileInfo> m_statusCache;

    /**
     * @brief Execute a git command and return the output
     */
    QString executeGitCommand(const QStringList& args, bool* success = nullptr) const;

    /**
     * @brief Parse git status --porcelain output
     */
    QList<GitFileInfo> parseStatusOutput(const QString& output) const;

    /**
     * @brief Parse git status character to enum
     */
    GitFileStatus parseStatusChar(QChar c) const;

    /**
     * @brief Find the repository root from a given path
     */
    QString findRepositoryRoot(const QString& path) const;

    /**
     * @brief Update the current branch name
     */
    void updateCurrentBranch();
};

#endif // GITINTEGRATION_H
