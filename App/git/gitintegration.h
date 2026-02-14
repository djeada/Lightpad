#ifndef GITINTEGRATION_H
#define GITINTEGRATION_H

#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

constexpr int GIT_COMMAND_TIMEOUT_MS = 5000;

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

struct GitFileInfo {
  QString filePath;
  GitFileStatus indexStatus;
  GitFileStatus workTreeStatus;
  QString originalPath;
};

struct GitDiffLineInfo {
  int lineNumber;
  enum class Type { Added, Modified, Deleted } type;
};

struct GitBranchInfo {
  QString name;
  bool isCurrent;
  bool isRemote;
  QString trackingBranch;
  int aheadCount;
  int behindCount;
};

struct GitRemoteInfo {
  QString name;
  QString fetchUrl;
  QString pushUrl;
};

struct GitStashEntry {
  int index;
  QString message;
  QString branch;
  QString commitHash;
};

struct GitConflictMarker {
  int startLine;
  int separatorLine;
  int endLine;
  QString oursContent;
  QString theirsContent;
};

struct GitCommitInfo {
  QString hash;
  QString shortHash;
  QString author;
  QString authorEmail;
  QString date;
  QString relativeDate;
  QString subject;
  QString body;
  QStringList parents;
};

struct GitBlameLineInfo {
  int lineNumber;
  QString author;
  QString authorEmail;
  QString summary;
  QString shortHash;
  QString relativeDate;
  QString date;
};

struct GitDiffHunk {
  int startLine;
  int lineCount;
  QString header;
  QStringList lines;
};

struct GitCommitFileStat {
  QString filePath;
  int additions;
  int deletions;
};

class GitIntegration : public QObject {
  Q_OBJECT

public:
  explicit GitIntegration(QObject *parent = nullptr);
  ~GitIntegration();

  bool setRepositoryPath(const QString &path);

  QString repositoryPath() const;

  QString workingPath() const;

  void setWorkingPath(const QString &path);

  bool isValidRepository() const;

  QString currentBranch() const;

  QList<GitFileInfo> getStatus() const;

  GitFileInfo getFileStatus(const QString &filePath) const;

  QList<GitDiffLineInfo> getDiffLines(const QString &filePath) const;

  QList<GitBranchInfo> getBranches() const;

  bool stageFile(const QString &filePath);

  bool stageAll();

  bool unstageFile(const QString &filePath);

  bool commit(const QString &message);

  bool commitAmend(const QString &message = QString());

  bool checkoutBranch(const QString &branchName);

  bool checkoutCommit(const QString &commitHash);

  bool createBranch(const QString &branchName, bool checkout = true);

  bool createBranchFromCommit(const QString &branchName,
                              const QString &commitHash, bool checkout = true);

  bool deleteBranch(const QString &branchName, bool force = false);

  bool mergeBranch(const QString &branchName);

  QString getFileDiff(const QString &filePath, bool staged = false) const;

  bool discardChanges(const QString &filePath);

  bool discardAllChanges();

  QList<GitCommitInfo> getCommitLog(int maxCount = 50,
                                    const QString &branch = QString()) const;

  GitCommitInfo getCommitDetails(const QString &commitHash) const;

  QString getCommitDiff(const QString &commitHash) const;

  QString getCommitAuthor(const QString &commitHash) const;

  QString getCommitDate(const QString &commitHash) const;

  QString getCommitMessage(const QString &commitHash) const;

  QString executeWordDiff(const QStringList &args) const;

  QList<GitBlameLineInfo> getBlameInfo(const QString &filePath) const;

  GitDiffHunk getDiffHunkAtLine(const QString &filePath, int lineNumber) const;

  QList<GitCommitFileStat> getCommitFileStats(const QString &commitHash) const;

  QList<GitCommitInfo> getFileLog(const QString &filePath,
                                  int maxCount = 50) const;

  QList<GitCommitInfo> getLineHistory(const QString &filePath, int startLine,
                                      int endLine) const;

  QString getFileAtRevision(const QString &filePath,
                            const QString &revision) const;

  QString getBranchDiff(const QString &branch1, const QString &branch2) const;

  bool getAheadBehind(int &ahead, int &behind) const;

  bool isDirty() const;

  bool stageHunkAtLine(const QString &filePath, int lineNumber);

  bool revertHunkAtLine(const QString &filePath, int lineNumber);

  bool fetch(const QString &remoteName = "origin");

  bool pull(const QString &remoteName = "origin",
            const QString &branchName = QString());

  bool push(const QString &remoteName = "origin",
            const QString &branchName = QString(), bool setUpstream = false);

  QList<GitRemoteInfo> getRemotes() const;

  bool addRemote(const QString &name, const QString &url);

  bool removeRemote(const QString &name);

  bool stash(const QString &message = QString(), bool includeUntracked = false);

  bool stashPop(int index = 0);

  bool stashApply(int index = 0);

  bool stashDrop(int index = 0);

  QList<GitStashEntry> stashList() const;

  QList<GitStashEntry> getStashList() const;

  bool stashClear();

  void refresh();

  bool initRepository(const QString &path);

  bool hasMergeConflicts() const;

  QStringList getConflictedFiles() const;

  QList<GitConflictMarker> getConflictMarkers(const QString &filePath) const;

  bool resolveConflictOurs(const QString &filePath);

  bool resolveConflictTheirs(const QString &filePath);

  bool markConflictResolved(const QString &filePath);

  bool abortMerge();

  bool continueMerge();

  bool isMergeInProgress() const;

  bool cherryPick(const QString &commitHash);

  QList<QPair<QString, QString>> listWorktrees() const;

  bool addWorktree(const QString &path, const QString &branch,
                   bool createBranch = false);

  bool removeWorktree(const QString &path);

  QMap<int, qint64> getBlameTimestamps(const QString &filePath) const;

signals:

  void statusChanged();

  void branchChanged(const QString &branchName);

  void errorOccurred(const QString &error);

  void operationCompleted(const QString &operation);

  void mergeConflictsDetected(const QStringList &conflictedFiles);

  void repositoryInitialized(const QString &path);

  void pushCompleted(const QString &remoteName, const QString &branchName);

  void pullCompleted(const QString &remoteName, const QString &branchName);

private:
  QString m_repositoryPath;
  QString m_workingPath;
  bool m_isValid;
  QString m_currentBranch;
  QList<GitFileInfo> m_statusCache;

  QString executeGitCommand(const QStringList &args,
                            bool *success = nullptr) const;

  QString executeGitCommandAtPath(const QString &path, const QStringList &args,
                                  bool *success = nullptr) const;

  QList<GitFileInfo> parseStatusOutput(const QString &output) const;

  GitFileStatus parseStatusChar(QChar c) const;

  QString findRepositoryRoot(const QString &path) const;

  void updateCurrentBranch();

  QList<GitStashEntry> parseStashListOutput(const QString &output) const;
};

#endif
