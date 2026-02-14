#ifndef GITINTEGRATION_H
#define GITINTEGRATION_H

#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

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
  GitFileStatus indexStatus;    // Status in staging area
  GitFileStatus workTreeStatus; // Status in working tree
  QString originalPath;         // For renamed files
};

/**
 * @brief Information about a git diff hunk for a specific line
 */
struct GitDiffLineInfo {
  int lineNumber;
  enum class Type { Added, Modified, Deleted } type;
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
 * @brief Git remote information
 */
struct GitRemoteInfo {
  QString name;
  QString fetchUrl;
  QString pushUrl;
};

/**
 * @brief Git stash entry information
 */
struct GitStashEntry {
  int index;          ///< The stash position (0 = most recent)
  QString message;    ///< The stash description/commit message
  QString branch;     ///< The branch the stash was created from
  QString commitHash; ///< The commit hash of the stash entry
};

/**
 * @brief Information about a merge conflict in a file
 */
struct GitConflictMarker {
  int startLine;
  int separatorLine;
  int endLine;
  QString oursContent;
  QString theirsContent;
};

/**
 * @brief Information about a git commit
 */
struct GitCommitInfo {
  QString hash;         ///< Full commit hash
  QString shortHash;    ///< Abbreviated hash (7 chars)
  QString author;       ///< Author name
  QString authorEmail;  ///< Author email
  QString date;         ///< Commit date (ISO format)
  QString relativeDate; ///< Relative date (e.g., "2 days ago")
  QString subject;      ///< Commit subject (first line)
  QString body;         ///< Commit body (remaining lines)
  QStringList parents;  ///< Parent commit hashes
};

/**
 * @brief Per-line git blame annotation
 */
struct GitBlameLineInfo {
  int lineNumber; ///< 1-based line number
  QString author;
  QString authorEmail;
  QString summary;
  QString shortHash;
  QString relativeDate;
  QString date; ///< Absolute date (ISO format)
};

/**
 * @brief Diff hunk content at a specific line
 */
struct GitDiffHunk {
  int startLine;     ///< First line number of the hunk in the new file
  int lineCount;     ///< Number of lines in the hunk
  QString header;    ///< Hunk header (@@ ... @@)
  QStringList lines; ///< Raw diff lines (prefixed with +/-/space)
};

/**
 * @brief Stats for files changed in a commit
 */
struct GitCommitFileStat {
  QString filePath;
  int additions;
  int deletions;
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
  explicit GitIntegration(QObject *parent = nullptr);
  ~GitIntegration();

  /**
   * @brief Set the repository path
   * @param path Path to the repository root or any file within it
   * @return true if path is within a git repository
   */
  bool setRepositoryPath(const QString &path);

  /**
   * @brief Get the current repository root path
   */
  QString repositoryPath() const;

  /**
   * @brief Get the working path (used when not yet a repository)
   */
  QString workingPath() const;

  /**
   * @brief Set the working path for a project not yet a git repository
   * @param path Directory path to set as working path
   */
  void setWorkingPath(const QString &path);

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
  GitFileInfo getFileStatus(const QString &filePath) const;

  /**
   * @brief Get diff line information for a file (for gutter display)
   * @param filePath Path to the file
   * @return List of line information for added/modified/deleted lines
   */
  QList<GitDiffLineInfo> getDiffLines(const QString &filePath) const;

  /**
   * @brief Get list of all branches
   */
  QList<GitBranchInfo> getBranches() const;

  /**
   * @brief Stage a file for commit
   */
  bool stageFile(const QString &filePath);

  /**
   * @brief Stage all files
   */
  bool stageAll();

  /**
   * @brief Unstage a file
   */
  bool unstageFile(const QString &filePath);

  /**
   * @brief Commit staged changes
   * @param message Commit message
   */
  bool commit(const QString &message);

  /**
   * @brief Amend the last commit with staged changes
   * @param message New commit message (if empty, reuses the previous message)
   * @return true if amend was successful
   */
  bool commitAmend(const QString &message = QString());

  /**
   * @brief Checkout a branch
   */
  bool checkoutBranch(const QString &branchName);

  /**
   * @brief Checkout a commit (detached HEAD)
   */
  bool checkoutCommit(const QString &commitHash);

  /**
   * @brief Create a new branch
   */
  bool createBranch(const QString &branchName, bool checkout = true);

  /**
   * @brief Create a new branch at a specific commit
   */
  bool createBranchFromCommit(const QString &branchName,
                              const QString &commitHash, bool checkout = true);

  /**
   * @brief Delete a branch
   */
  bool deleteBranch(const QString &branchName, bool force = false);

  /**
   * @brief Merge a branch into the current branch
   * @param branchName The branch to merge
   * @return true if merge was successful
   */
  bool mergeBranch(const QString &branchName);

  /**
   * @brief Get the diff output for a file
   * @param filePath Path to the file
   * @param staged When true, returns the staged diff; otherwise returns the
   * unstaged diff
   */
  QString getFileDiff(const QString &filePath, bool staged = false) const;

  /**
   * @brief Discard changes in a file (restore to HEAD)
   */
  bool discardChanges(const QString &filePath);

  /**
   * @brief Discard all unstaged changes (restore all files to HEAD)
   * @return true if discard was successful
   */
  bool discardAllChanges();

  // ==================== Commit History ====================

  /**
   * @brief Get commit history/log
   * @param maxCount Maximum number of commits to retrieve (default 50)
   * @param branch Branch name to get commits from (defaults to current branch)
   * @return List of commit information
   */
  QList<GitCommitInfo> getCommitLog(int maxCount = 50,
                                    const QString &branch = QString()) const;

  /**
   * @brief Get details of a specific commit
   * @param commitHash The commit hash to get details for
   * @return Commit information
   */
  GitCommitInfo getCommitDetails(const QString &commitHash) const;

  /**
   * @brief Get the diff for a specific commit
   * @param commitHash The commit hash to get the diff for
   * @return Diff output as a string
   */
  QString getCommitDiff(const QString &commitHash) const;

  /**
   * @brief Get the author of a specific commit
   * @param commitHash The commit hash
   * @return Author name and email
   */
  QString getCommitAuthor(const QString &commitHash) const;

  /**
   * @brief Get the date of a specific commit
   * @param commitHash The commit hash
   * @return Commit date as formatted string
   */
  QString getCommitDate(const QString &commitHash) const;

  /**
   * @brief Get the commit message
   * @param commitHash The commit hash
   * @return Full commit message
   */
  QString getCommitMessage(const QString &commitHash) const;

  /**
   * @brief Execute git command for word diff rendering
   * @param args Git command arguments
   * @return Command output (empty if failed)
   */
  QString executeWordDiff(const QStringList &args) const;

  /**
   * @brief Get blame annotations for a file
   * @param filePath Path to the file
   * @return List of blame info for each line
   */
  QList<GitBlameLineInfo> getBlameInfo(const QString &filePath) const;

  /**
   * @brief Get the diff hunk that contains a specific line
   * @param filePath Path to the file
   * @param lineNumber 1-based line number
   * @return Diff hunk at that line, or empty hunk if none
   */
  GitDiffHunk getDiffHunkAtLine(const QString &filePath, int lineNumber) const;

  /**
   * @brief Get file-level change stats for a commit
   * @param commitHash The commit hash
   * @return List of files changed with addition/deletion counts
   */
  QList<GitCommitFileStat> getCommitFileStats(const QString &commitHash) const;

  /**
   * @brief Get commit history for a specific file
   * @param filePath Path to the file
   * @param maxCount Maximum commits to return
   * @return List of commits that touched this file
   */
  QList<GitCommitInfo> getFileLog(const QString &filePath,
                                  int maxCount = 50) const;

  /**
   * @brief Get history for a specific line range using git log -L
   * @param filePath Path to the file
   * @param startLine 1-based start line
   * @param endLine 1-based end line
   * @return List of commits that modified this line range
   */
  QList<GitCommitInfo> getLineHistory(const QString &filePath, int startLine,
                                      int endLine) const;

  /**
   * @brief Get file content at a specific revision
   * @param filePath Path to the file
   * @param revision Git revision (commit hash, branch, tag, HEAD~N)
   * @return File content at that revision, or empty string on failure
   */
  QString getFileAtRevision(const QString &filePath,
                            const QString &revision) const;

  /**
   * @brief Get diff between two branches
   * @param branch1 First branch name
   * @param branch2 Second branch name
   * @return Diff output as string
   */
  QString getBranchDiff(const QString &branch1, const QString &branch2) const;

  /**
   * @brief Get ahead/behind counts for current branch vs upstream
   * @param ahead Output: commits ahead of upstream
   * @param behind Output: commits behind upstream
   * @return true if upstream tracking info is available
   */
  bool getAheadBehind(int &ahead, int &behind) const;

  /**
   * @brief Check if the working tree has uncommitted changes
   */
  bool isDirty() const;

  /**
   * @brief Stage a single hunk at a line (for gutter stage action)
   * @param filePath Path to the file
   * @param lineNumber 1-based line number within the hunk
   * @return true if staging succeeded
   */
  bool stageHunkAtLine(const QString &filePath, int lineNumber);

  /**
   * @brief Revert a single hunk at a line (discard that change)
   * @param filePath Path to the file
   * @param lineNumber 1-based line number within the hunk
   * @return true if revert succeeded
   */
  bool revertHunkAtLine(const QString &filePath, int lineNumber);

  // ==================== Remote Operations ====================

  /**
   * @brief Fetch from remote repository
   * @param remoteName Name of the remote (defaults to "origin")
   * @return true if fetch was successful
   */
  bool fetch(const QString &remoteName = "origin");

  /**
   * @brief Pull from remote repository
   * @param remoteName Name of the remote (defaults to "origin")
   * @param branchName Branch to pull (defaults to current branch)
   * @return true if pull was successful
   */
  bool pull(const QString &remoteName = "origin",
            const QString &branchName = QString());

  /**
   * @brief Push to remote repository
   * @param remoteName Name of the remote (defaults to "origin")
   * @param branchName Branch to push (defaults to current branch)
   * @param setUpstream Set upstream tracking for the branch
   * @return true if push was successful
   */
  bool push(const QString &remoteName = "origin",
            const QString &branchName = QString(), bool setUpstream = false);

  /**
   * @brief Get list of configured remotes
   * @return List of remote information
   */
  QList<GitRemoteInfo> getRemotes() const;

  /**
   * @brief Add a remote
   * @param name Remote name (e.g., "origin")
   * @param url Remote URL
   */
  bool addRemote(const QString &name, const QString &url);

  /**
   * @brief Remove a remote
   */
  bool removeRemote(const QString &name);

  // ==================== Stash Operations ====================

  /**
   * @brief Stash current changes
   * @param message Optional stash message
   * @param includeUntracked Include untracked files in stash
   * @return true if stash was successful
   */
  bool stash(const QString &message = QString(), bool includeUntracked = false);

  /**
   * @brief Pop a stash entry
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
   * @brief Get list of stash entries (alias for stashList)
   */
  QList<GitStashEntry> getStashList() const;

  /**
   * @brief Clear all stash entries
   * @return true if clear was successful
   */
  bool stashClear();

  /**
   * @brief Refresh the repository status
   */
  void refresh();

  // ========== Repository Initialization ==========

  /**
   * @brief Initialize a new git repository at the specified path
   * @param path Directory path where to initialize the repository
   * @return true if initialization was successful
   */
  bool initRepository(const QString &path);

  // ========== Merge Conflict Handling ==========

  /**
   * @brief Check if there are merge conflicts
   */
  bool hasMergeConflicts() const;

  /**
   * @brief Get list of files with merge conflicts
   */
  QStringList getConflictedFiles() const;

  /**
   * @brief Get conflict markers in a file
   * @param filePath Path to the file with conflicts
   */
  QList<GitConflictMarker> getConflictMarkers(const QString &filePath) const;

  /**
   * @brief Resolve a conflict by accepting ours version
   */
  bool resolveConflictOurs(const QString &filePath);

  /**
   * @brief Resolve a conflict by accepting theirs version
   */
  bool resolveConflictTheirs(const QString &filePath);

  /**
   * @brief Mark a file as resolved after manual editing
   */
  bool markConflictResolved(const QString &filePath);

  /**
   * @brief Abort the current merge operation
   */
  bool abortMerge();

  /**
   * @brief Continue merge after resolving conflicts
   */
  bool continueMerge();

  /**
   * @brief Check if a merge is in progress
   */
  bool isMergeInProgress() const;

  // ==================== Cherry-pick ====================

  /**
   * @brief Cherry-pick a commit into the current branch
   * @param commitHash Hash of the commit to cherry-pick
   * @return true if cherry-pick succeeded
   */
  bool cherryPick(const QString &commitHash);

  // ==================== Worktree Operations ====================

  /**
   * @brief List all git worktrees
   * @return List of (path, branch) pairs
   */
  QList<QPair<QString, QString>> listWorktrees() const;

  /**
   * @brief Add a new worktree
   * @param path Filesystem path for the new worktree
   * @param branch Branch to check out (created if -b flag needed)
   * @param createBranch If true, create a new branch
   * @return true if worktree was added successfully
   */
  bool addWorktree(const QString &path, const QString &branch,
                   bool createBranch = false);

  /**
   * @brief Remove a worktree
   * @param path Filesystem path of the worktree to remove
   * @return true if removal succeeded
   */
  bool removeWorktree(const QString &path);

  /**
   * @brief Get blame timestamps for heatmap coloring
   * @param filePath Path to the file
   * @return Map of line number -> epoch timestamp
   */
  QMap<int, qint64> getBlameTimestamps(const QString &filePath) const;

signals:
  /**
   * @brief Emitted when repository status changes
   */
  void statusChanged();

  /**
   * @brief Emitted when current branch changes
   */
  void branchChanged(const QString &branchName);

  /**
   * @brief Emitted when an error occurs
   */
  void errorOccurred(const QString &error);

  /**
   * @brief Emitted when a git operation completes successfully
   */
  void operationCompleted(const QString &operation);

  /**
   * @brief Emitted when merge conflicts are detected
   */
  void mergeConflictsDetected(const QStringList &conflictedFiles);

  /**
   * @brief Emitted when repository is initialized
   */
  void repositoryInitialized(const QString &path);

  /**
   * @brief Emitted when a push operation completes
   */
  void pushCompleted(const QString &remoteName, const QString &branchName);

  /**
   * @brief Emitted when a pull operation completes
   */
  void pullCompleted(const QString &remoteName, const QString &branchName);

private:
  QString m_repositoryPath;
  QString m_workingPath; // Path used when not yet a valid repository
  bool m_isValid;
  QString m_currentBranch;
  QList<GitFileInfo> m_statusCache;

  /**
   * @brief Execute a git command and return the output
   */
  QString executeGitCommand(const QStringList &args,
                            bool *success = nullptr) const;

  /**
   * @brief Execute a git command at a specific path
   */
  QString executeGitCommandAtPath(const QString &path, const QStringList &args,
                                  bool *success = nullptr) const;

  /**
   * @brief Parse git status --porcelain output
   */
  QList<GitFileInfo> parseStatusOutput(const QString &output) const;

  /**
   * @brief Parse git status character to enum
   */
  GitFileStatus parseStatusChar(QChar c) const;

  /**
   * @brief Find the repository root from a given path
   */
  QString findRepositoryRoot(const QString &path) const;

  /**
   * @brief Update the current branch name
   */
  void updateCurrentBranch();

  /**
   * @brief Parse stash list output
   */
  QList<GitStashEntry> parseStashListOutput(const QString &output) const;
};

#endif // GITINTEGRATION_H
