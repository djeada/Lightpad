#ifndef GITINTEGRATION_H
#define GITINTEGRATION_H

#include "gitcommandlog.h"
#include "gitcommittypes.h"
#include "gitconflictmodel.h"
#include "gitdiffmodel.h"
#include "gitfiletimeline.h"
#include "gitoperationpreview.h"
#include "gitrebaseplan.h"
#include "gitrepositorystate.h"
#include "gitsyncmodel.h"
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <functional>

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

struct GitStashDetail {
  int index = 0;
  QString message;
  QString branch;
  QString commitHash;
  QString baseHash;
  QString baseSubject;
  QString relativeDate;
  int parentCount = 0;
};

struct GitStashEntry {
  int index;
  QString message;
  QString branch;
  QString commitHash;
};

struct GitConflictIdentities {
  QString oursLabel;
  QString oursHash;
  QString oursAuthor;
  QString oursSubject;

  QString theirsLabel;
  QString theirsHash;
  QString theirsAuthor;
  QString theirsSubject;

  QString mergeBase;
  QString mergeBaseSubject;
};

struct GitConflictMarker {
  int startLine;
  int separatorLine;
  int endLine;
  QString oursContent;
  QString theirsContent;
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

struct GitTagInfo {
  QString name;
  QString hash;
  bool isAnnotated;
};

struct GitReflogEntry {
  QString hash;
  QString shortHash;
  QString action;
  QString subject;
  QString relativeDate;
};

struct GitRefDecoration {

  enum class Kind { LocalBranch, RemoteBranch, Tag, Worktree, Stash };
  Kind kind = Kind::LocalBranch;
  QString name;
  bool isHead = false;

  bool isUpstream = false;
};

struct GitLogOptions {

  QString revisionRange;

  QString pathFilter;

  bool firstParentOnly = false;

  bool allRefs = false;
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

  QString branchRefDetails() const;

  QSet<QString> mergedBranchNames(const QString &baseRef) const;

  QMap<QString, QString> branchWorktreePaths() const;

  int countCommitsNotIn(const QString &ref, const QString &baseRef) const;

  bool stageFile(const QString &filePath);

  bool stageAll();

  bool unstageFile(const QString &filePath);

  bool unstageAll();

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

  QList<GitCommitInfo> getCommitLogPage(const QString &branch, int skip,
                                        int limit) const;

  QMap<QString, QList<GitRefDecoration>> getCommitRefsMap() const;

  void getCommitLogPageAsync(
      const QString &branch, int skip, int limit,
      const std::function<void(QList<GitCommitInfo>)> &callback);

  void getCommitRefsMapAsync(
      const std::function<void(QMap<QString, QList<GitRefDecoration>>)>
          &callback);

  GitCommitInfo getCommitDetails(const QString &commitHash) const;

  QString getCommitDiff(const QString &commitHash) const;

  QString getCommitAuthor(const QString &commitHash) const;

  QString getCommitDate(const QString &commitHash) const;

  QString getCommitMessage(const QString &commitHash) const;

  QString executeWordDiff(const QStringList &args) const;

  QList<GitBlameLineInfo> getBlameInfo(const QString &filePath) const;

  QString blameCommitForLine(const QString &filePath, int line,
                             bool detectMoves) const;

  QString blameOriginalPath(const QString &filePath, int line) const;

  QString fileLines(const QString &filePath, int startLine, int endLine) const;
  QString fileLinesAtRevision(const QString &filePath, const QString &revision,
                              int startLine, int endLine) const;

  GitDiffHunk getDiffHunkAtLine(const QString &filePath, int lineNumber) const;

  QList<GitCommitFileStat> getCommitFileStats(const QString &commitHash) const;

  QList<GitCommitInfo> getFileLog(const QString &filePath,
                                  int maxCount = 50) const;

  QList<GitCommitInfo> getFileLogRange(const QString &filePath,
                                       const QString &range,
                                       int maxCount = 50) const;

  QList<GitCommitInfo> getLineHistory(const QString &filePath, int startLine,
                                      int endLine) const;

  QString getFileAtRevision(const QString &filePath,
                            const QString &revision) const;

  QString getBranchDiff(const QString &branch1, const QString &branch2) const;

  QString getMergeBase(const QString &refA, const QString &refB) const;

  QStringList predictMergeConflicts(const QString &refA,
                                    const QString &refB) const;

  QList<GitCommitInfo> getUnreachableAfterDelete(const QString &branchName,
                                                 int maxCount = 100) const;

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

  QList<GitStashDetail> stashDetails() const;

  QString stashNumstat(int index) const;
  QString stashNameStatus(int index) const;
  QString stashDiff(int index) const;

  bool stashPaths(const QStringList &paths, const QString &message,
                  bool includeUntracked = false);

  bool restoreStashPaths(int index, const QStringList &paths);

  QList<GitStashEntry> getStashList() const;

  bool stashClear();

  bool stashBranch(const QString &branchName, int index);

  void refresh();

  bool initRepository(const QString &path);

  bool hasMergeConflicts() const;

  QStringList getConflictedFiles() const;

  QList<QPair<QString, QString>> unmergedEntries() const;

  QString stageContent(const QString &filePath, int stage) const;

  QString workingFileContent(const QString &filePath) const;

  GitConflictIdentities conflictIdentities() const;

  bool resolveConflictWith(const QString &filePath, const QString &content);

  QList<GitConflictMarker> getConflictMarkers(const QString &filePath) const;

  bool resolveConflictOurs(const QString &filePath);

  bool resolveConflictTheirs(const QString &filePath);

  bool markConflictResolved(const QString &filePath);

  bool abortMerge();

  bool continueMerge();

  bool isMergeInProgress() const;

  bool cherryPick(const QString &commitHash);

  bool cherryPickNoCommit(const QString &commitHash);

  bool revertCommit(const QString &commitHash);

  bool resetToCommit(const QString &commitHash, const QString &mode = "mixed");

  bool rewordCommit(const QString &commitHash, const QString &newMessage);

  bool dropCommit(const QString &commitHash);

  bool squashCommits(const QStringList &commitHashes,
                     const QString &newMessage);

  bool moveCommitToBranch(const QString &commitHash,
                          const QString &targetBranch);

  QList<QPair<QString, QString>> listWorktrees() const;

  QString worktreeListRaw() const;

  GitRepositoryState worktreeState(const QString &worktreePath) const;

  bool pruneWorktrees();

  bool addWorktree(const QString &path, const QString &branch,
                   bool createBranch = false);

  bool removeWorktree(const QString &path);

  QMap<int, qint64> getBlameTimestamps(const QString &filePath) const;

  QList<GitTagInfo> getTags() const;

  bool createTag(const QString &name, const QString &commitHash = QString(),
                 const QString &message = QString());

  bool deleteTag(const QString &name);

  bool renameBranch(const QString &oldName, const QString &newName);

  bool rebaseBranch(const QString &ontoBranch);

  bool startInteractiveRebase(const QString &onto, const GitRebasePlan &plan);

  bool rebaseContinue();
  bool rebaseSkip();
  bool rebaseAbort();

  bool isRebaseInProgress() const;

  bool rebaseProgress(int &current, int &total) const;

  QList<GitReflogEntry> getReflog(int count = 20) const;

  QString reflogRaw(int count = 100) const;

  bool isCommitReachable(const QString &commitHash) const;

  QStringList getBackupRefs() const;

  bool isBisecting() const;
  QString bisectStart(const QString &badRef, const QString &goodRef);
  QString bisectMark(const QString &verdict);
  QString bisectLog() const;
  bool bisectReset();

  QString bisectRun(const QString &command, int *exitCode = nullptr);

  QList<GitCommitInfo> getCommitsWithoutRef(int maxCount = 100) const;

  QString worktreeDirtySummary(const QString &worktreePath) const;

  QStringList getCommitRefs(const QString &hash) const;

  QList<GitCommandRecord> commandHistory() const { return m_commandHistory; }
  void clearCommandHistory();

  static GitCommandMirrorMode mirrorMode();
  static void setMirrorMode(GitCommandMirrorMode mode);

  GitRepositoryState repositoryState() const;

  QString gitDirPath() const;

  QString getWorkingVsHeadDiff(const QString &filePath) const;

  bool applyPatch(const QString &patch, bool cached, bool reverse);

  QList<GitCommitInfo> getLogPage(const GitLogOptions &options, int skip,
                                  int limit) const;

  void
  getLogPageAsync(const GitLogOptions &options, int skip, int limit,
                  const std::function<void(QList<GitCommitInfo>)> &callback);

  QMap<QString, QStringList> getWorktreeAnchors() const;

  QMap<QString, QStringList> getStashAnchors() const;

  GitSyncState syncState(int maxCommits = 200) const;

  QList<GitFileRevision> getFileTimeline(const QString &filePath,
                                         const GitFileTimelineOptions &options,
                                         int skip, int limit) const;

  GitPullStrategy configuredPullStrategy() const;

  bool pullWithStrategy(const QString &remoteName, const QString &branchName,
                        GitPullStrategy strategy);

  bool pushWithForce(const QString &remoteName, const QString &branchName,
                     bool setUpstream, GitPushForce force);

  bool setUpstreamBranch(const QString &remoteName, const QString &branchName);

  bool unsetUpstream(const QString &branchName);

signals:

  void statusChanged();

  void commandExecuted(const GitCommandRecord &record);

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

  mutable QList<GitCommandRecord> m_commandHistory;

  void recordCommand(const QStringList &args, const QString &workingDirectory,
                     const QString &output, const QString &error,
                     int exitCode) const;

  QString executeGitCommand(const QStringList &args,
                            bool *success = nullptr) const;

  QString executeGitCommandAtPath(const QString &path, const QStringList &args,
                                  bool *success = nullptr) const;

  QString executeGitCommandWithInput(const QStringList &args,
                                     const QByteArray &input,
                                     bool *success = nullptr,
                                     QString *errorOutput = nullptr) const;

  QString executeGitCommandWithEnv(const QStringList &args,
                                   const QMap<QString, QString> &extraEnv,
                                   bool *success = nullptr,
                                   QString *errorOutput = nullptr) const;

  QString prepareRebaseHelpers(const QString &todoText,
                               const QStringList &messages) const;

  QMap<QString, QString> rebaseHelperEnvironment() const;

  QList<GitFileInfo> parseStatusOutput(const QString &output) const;

  QList<GitCommitInfo> parseCommitLogOutput(const QString &output) const;

  GitFileStatus parseStatusChar(QChar c) const;

  QString findRepositoryRoot(const QString &path) const;

  void updateCurrentBranch();

  QList<GitStashEntry> parseStashListOutput(const QString &output) const;
};

#endif
