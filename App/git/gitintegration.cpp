#include "gitintegration.h"
#include "../core/logging/logger.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QTemporaryFile>
#include <QTextStream>
#include <QThreadPool>

GitIntegration::GitIntegration(QObject *parent)
    : QObject(parent), m_isValid(false) {}

GitIntegration::~GitIntegration() {}

bool GitIntegration::setRepositoryPath(const QString &path) {
  QString repoRoot = findRepositoryRoot(path);

  if (repoRoot.isEmpty()) {
    m_isValid = false;
    m_repositoryPath.clear();
    m_currentBranch.clear();
    LOG_DEBUG("No git repository found at: " + path);
    return false;
  }

  m_repositoryPath = repoRoot;
  m_isValid = true;
  updateCurrentBranch();

  LOG_INFO("Git repository found at: " + m_repositoryPath);
  return true;
}

QString GitIntegration::repositoryPath() const { return m_repositoryPath; }

bool GitIntegration::isValidRepository() const { return m_isValid; }

QString GitIntegration::currentBranch() const { return m_currentBranch; }

QString GitIntegration::findRepositoryRoot(const QString &path) const {
  QDir dir(path);

  QFileInfo info(path);
  if (info.isFile()) {
    dir = info.dir();
  }

  while (true) {
    if (dir.exists(".git")) {
      return dir.absolutePath();
    }

    if (!dir.cdUp()) {
      break;
    }
  }

  return QString();
}

QString GitIntegration::executeGitCommand(const QStringList &args,
                                          bool *success) const {
  if (!m_isValid && !args.contains("rev-parse")) {
    if (success)
      *success = false;
    return QString();
  }

  QProcess process;
  process.setWorkingDirectory(m_repositoryPath.isEmpty() ? QDir::currentPath()
                                                         : m_repositoryPath);
  process.start("git", args);

  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    LOG_WARNING("Git command timed out: git " + args.join(" "));
    if (success)
      *success = false;
    return QString();
  }

  if (success) {
    *success = (process.exitCode() == 0);
  }

  QString output = QString::fromUtf8(process.readAllStandardOutput());
  int end = output.size();
  while (end > 0) {
    QChar ch = output[end - 1];
    if (ch == '\n' || ch == '\r' || ch == ' ')
      --end;
    else
      break;
  }
  output.truncate(end);

  const QString error = QString::fromUtf8(process.readAllStandardError());
  recordCommand(args, process.workingDirectory(), output, error,
                process.exitCode());

  if (process.exitCode() != 0) {
    LOG_DEBUG("Git command failed: git " + args.join(" ") + " - " + error);
    return output;
  }

  return output;
}

QString GitIntegration::executeWordDiff(const QStringList &args) const {
  if (!m_isValid) {
    return QString();
  }

  bool success = false;
  QString output = executeGitCommand(args, &success);
  if (success) {
    return output;
  }
  return QString();
}

void GitIntegration::updateCurrentBranch() {
  bool success;
  QString branch =
      executeGitCommand({"rev-parse", "--abbrev-ref", "HEAD"}, &success);

  if (success) {
    if (m_currentBranch != branch) {
      m_currentBranch = branch;
      emit branchChanged(m_currentBranch);
    }
  }
}

QList<GitFileInfo> GitIntegration::getStatus() const {
  bool success;
  QString output =
      executeGitCommand({"status", "--porcelain", "-uall"}, &success);

  if (!success) {
    return QList<GitFileInfo>();
  }

  return parseStatusOutput(output);
}

GitFileInfo GitIntegration::getFileStatus(const QString &filePath) const {
  GitFileInfo info;
  info.filePath = filePath;
  info.indexStatus = GitFileStatus::Clean;
  info.workTreeStatus = GitFileStatus::Clean;

  if (!m_isValid) {
    return info;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  QString output = executeGitCommand(
      {"status", "--porcelain", "-uall", "--", relativePath}, &success);

  if (!success || output.isEmpty()) {
    return info;
  }

  QList<GitFileInfo> parsed = parseStatusOutput(output);
  if (!parsed.isEmpty()) {
    return parsed.first();
  }

  return info;
}

QList<GitFileInfo>
GitIntegration::parseStatusOutput(const QString &output) const {
  QList<GitFileInfo> result;

  if (output.isEmpty()) {
    return result;
  }

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);

  for (const QString &line : lines) {
    if (line.length() < 4) {
      continue;
    }

    GitFileInfo info;

    QChar indexChar = line[0];
    QChar workTreeChar = line[1];
    QString path = line.mid(3);

    if (path.contains(" -> ")) {
      QStringList parts = path.split(" -> ");
      info.originalPath = parts[0];
      path = parts[1];
    }

    info.filePath = path;
    info.indexStatus = parseStatusChar(indexChar);
    info.workTreeStatus = parseStatusChar(workTreeChar);

    result.append(info);
  }

  return result;
}

GitFileStatus GitIntegration::parseStatusChar(QChar c) const {
  switch (c.toLatin1()) {
  case ' ':
    return GitFileStatus::Clean;
  case 'M':
    return GitFileStatus::Modified;
  case 'A':
    return GitFileStatus::Added;
  case 'D':
    return GitFileStatus::Deleted;
  case 'R':
    return GitFileStatus::Renamed;
  case 'C':
    return GitFileStatus::Copied;
  case 'U':
    return GitFileStatus::Unmerged;
  case '?':
    return GitFileStatus::Untracked;
  case '!':
    return GitFileStatus::Ignored;
  default:
    return GitFileStatus::Clean;
  }
}

QList<GitDiffLineInfo>
GitIntegration::getDiffLines(const QString &filePath) const {
  QList<GitDiffLineInfo> result;

  if (!m_isValid) {
    return result;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;

  QString output =
      executeGitCommand({"diff", "-U0", "--", relativePath}, &success);

  if (output.isEmpty()) {

    output = executeGitCommand({"diff", "-U0", "--cached", "--", relativePath},
                               &success);
    if (output.isEmpty()) {
      return result;
    }
  }

  QRegularExpression hunkHeader(
      R"(@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@)");
  QStringList lines = output.split('\n');

  int currentNewLine = 0;
  bool inHunk = false;

  for (const QString &line : lines) {
    QRegularExpressionMatch match = hunkHeader.match(line);
    if (match.hasMatch()) {
      currentNewLine = match.captured(3).toInt();
      int oldCount =
          match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
      int newCount =
          match.captured(4).isEmpty() ? 1 : match.captured(4).toInt();

      if (oldCount == 0 && newCount > 0) {
        for (int i = 0; i < newCount; ++i) {
          GitDiffLineInfo info;
          info.lineNumber = currentNewLine + i;
          info.type = GitDiffLineInfo::Type::Added;
          result.append(info);
        }
      } else if (newCount == 0 && oldCount > 0) {

        GitDiffLineInfo info;
        info.lineNumber = currentNewLine > 0 ? currentNewLine : 1;
        info.type = GitDiffLineInfo::Type::Deleted;
        result.append(info);
      }

      inHunk = true;
      continue;
    }

    if (inHunk && !line.isEmpty()) {
      if (line[0] == '+') {

        bool alreadyMarked = false;
        for (const auto &existing : result) {
          if (existing.lineNumber == currentNewLine) {
            alreadyMarked = true;
            break;
          }
        }
        if (!alreadyMarked) {
          GitDiffLineInfo info;
          info.lineNumber = currentNewLine;
          info.type = GitDiffLineInfo::Type::Modified;
          result.append(info);
        }
        currentNewLine++;
      } else if (line[0] == '-') {

      } else if (line[0] != '\\') {

        currentNewLine++;
      }
    }
  }

  return result;
}

QList<GitBranchInfo> GitIntegration::getBranches() const {
  QList<GitBranchInfo> result;

  if (!m_isValid) {
    return result;
  }

  bool success;
  QString output = executeGitCommand(
      {"branch", "-a",
       "--format=%(refname:short)%(HEAD)\t%(upstream:short)\t%(symref:short)"},
      &success);

  if (!success) {
    return result;
  }

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);

  for (const QString &line : lines) {
    GitBranchInfo info;

    QString trimmedLine = line.trimmed();
    if (trimmedLine.isEmpty()) {
      continue;
    }

    QStringList parts = trimmedLine.split('\t');
    QString namePart = parts.value(0).trimmed();
    QString symref = parts.value(2).trimmed();

    if (!symref.isEmpty()) {
      continue;
    }

    if (namePart.endsWith('*')) {
      info.isCurrent = true;
      namePart.chop(1);
    } else {
      info.isCurrent = false;
    }

    info.isRemote =
        namePart.startsWith("remotes/") || namePart.startsWith("origin/");
    info.name = namePart;
    info.trackingBranch = parts.value(1).trimmed();
    info.aheadCount = 0;
    info.behindCount = 0;

    result.append(info);
  }

  return result;
}

bool GitIntegration::stageFile(const QString &filePath) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  executeGitCommand({"add", "--", relativePath}, &success);

  if (success) {
    emit operationCompleted("File staged: " + relativePath);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to stage file: " + relativePath);
  }

  return success;
}

bool GitIntegration::stageAll() {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  executeGitCommand({"add", "-A"}, &success);

  if (success) {
    emit operationCompleted("All changes staged");
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to stage all changes");
  }

  return success;
}

bool GitIntegration::unstageFile(const QString &filePath) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  executeGitCommand({"reset", "HEAD", "--", relativePath}, &success);

  if (success) {
    emit operationCompleted("File unstaged: " + relativePath);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to unstage file: " + relativePath);
  }

  return success;
}

bool GitIntegration::commit(const QString &message) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (message.isEmpty()) {
    emit errorOccurred("Commit message cannot be empty");
    return false;
  }

  bool success;
  executeGitCommand({"commit", "-m", message}, &success);

  if (success) {
    emit operationCompleted("Changes committed");
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to commit changes");
  }

  return success;
}

bool GitIntegration::commitAmend(const QString &message) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  if (message.isEmpty()) {
    executeGitCommand({"commit", "--amend", "--no-edit"}, &success);
  } else {
    executeGitCommand({"commit", "--amend", "-m", message}, &success);
  }

  if (success) {
    emit operationCompleted("Last commit amended");
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to amend commit");
  }

  return success;
}

bool GitIntegration::checkoutBranch(const QString &branchName) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  executeGitCommand({"checkout", branchName}, &success);

  if (success) {
    updateCurrentBranch();
    emit operationCompleted("Switched to branch: " + branchName);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to checkout branch: " + branchName);
  }

  return success;
}

bool GitIntegration::checkoutCommit(const QString &commitHash) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (commitHash.isEmpty()) {
    emit errorOccurred("Commit hash cannot be empty");
    return false;
  }

  bool success;
  executeGitCommand({"checkout", commitHash}, &success);

  if (success) {
    updateCurrentBranch();
    emit operationCompleted("Checked out commit: " + commitHash.left(7));
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to checkout commit: " + commitHash.left(7));
  }

  return success;
}

bool GitIntegration::createBranch(const QString &branchName, bool checkout) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;

  if (checkout) {
    executeGitCommand({"checkout", "-b", branchName}, &success);
  } else {
    executeGitCommand({"branch", branchName}, &success);
  }

  if (success) {
    if (checkout) {
      updateCurrentBranch();
    }
    emit operationCompleted("Branch created: " + branchName);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to create branch: " + branchName);
  }

  return success;
}

bool GitIntegration::createBranchFromCommit(const QString &branchName,
                                            const QString &commitHash,
                                            bool checkout) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (branchName.isEmpty() || commitHash.isEmpty()) {
    emit errorOccurred("Branch name or commit hash cannot be empty");
    return false;
  }

  bool success;
  QStringList args =
      checkout ? QStringList({"checkout", "-b", branchName, commitHash})
               : QStringList({"branch", branchName, commitHash});
  executeGitCommand(args, &success);

  if (success) {
    if (checkout) {
      updateCurrentBranch();
    }
    emit operationCompleted("Branch created: " + branchName);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to create branch: " + branchName);
  }

  return success;
}

bool GitIntegration::deleteBranch(const QString &branchName, bool force) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  QStringList args = {"branch", force ? "-D" : "-d", branchName};
  executeGitCommand(args, &success);

  if (success) {
    emit operationCompleted("Branch deleted: " + branchName);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to delete branch: " + branchName);
  }

  return success;
}

QString GitIntegration::getFileDiff(const QString &filePath,
                                    bool staged) const {
  if (!m_isValid) {
    return QString();
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  if (staged) {
    return executeGitCommand({"diff", "--cached", "--", relativePath},
                             &success);
  }

  QString diff = executeGitCommand({"diff", "--", relativePath}, &success);
  if (!diff.isEmpty()) {
    return diff;
  }

  GitFileInfo status = getFileStatus(filePath);
  if (status.workTreeStatus == GitFileStatus::Untracked ||
      status.indexStatus == GitFileStatus::Untracked) {
    const QString nullDevice =
#ifdef Q_OS_WIN
        "NUL";
#else
        "/dev/null";
#endif
    return executeGitCommand(
        {"diff", "--no-index", "--", nullDevice, relativePath}, &success);
  }

  return diff;
}

bool GitIntegration::discardChanges(const QString &filePath) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  executeGitCommand({"checkout", "--", relativePath}, &success);

  if (success) {
    emit operationCompleted("Changes discarded: " + relativePath);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to discard changes: " + relativePath);
  }

  return success;
}

bool GitIntegration::discardAllChanges() {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  executeGitCommand({"checkout", "--", "."}, &success);

  if (success) {

    executeGitCommand({"clean", "-fd"}, &success);
    emit operationCompleted("All changes discarded");
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to discard all changes");
  }

  return success;
}

QList<GitCommitInfo> GitIntegration::getCommitLog(int maxCount,
                                                  const QString &branch) const {
  if (!m_isValid) {
    return {};
  }

  QString format = "%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P";

  QStringList args = {"log", QString("--max-count=%1").arg(maxCount),
                      QString("--pretty=format:%1").arg(format)};

  if (!branch.isEmpty()) {
    args.append(branch);
  }

  bool success;
  QString output = executeGitCommand(args, &success);

  if (!success || output.isEmpty()) {
    return {};
  }

  return parseCommitLogOutput(output);
}

QList<GitCommitInfo>
GitIntegration::parseCommitLogOutput(const QString &output) const {
  QList<GitCommitInfo> result;

  if (output.isEmpty()) {
    return result;
  }

  const QStringList commits = output.split('\n', Qt::SkipEmptyParts);
  result.reserve(commits.size());

  for (const QString &line : commits) {
    QStringList parts = line.split(QChar('\0'));
    if (parts.size() < 7) {
      continue;
    }

    GitCommitInfo info;
    info.hash = parts[0];
    info.shortHash = parts[1];
    info.author = parts[2];
    info.authorEmail = parts[3];
    info.date = parts[4];
    info.relativeDate = parts[5];
    info.subject = parts[6];

    if (parts.size() > 7) {
      info.parents = parts[7].split(' ', Qt::SkipEmptyParts);
    }

    result.append(info);
  }

  return result;
}

QList<GitCommitInfo> GitIntegration::getCommitLogPage(const QString &branch,
                                                      int skip,
                                                      int limit) const {
  if (!m_isValid || limit <= 0 || skip < 0) {
    return {};
  }

  const QString format = "%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P";

  QStringList args = {"log", "--date-order", QString("--skip=%1").arg(skip),
                      QString("--max-count=%1").arg(limit),
                      QString("--pretty=format:%1").arg(format)};

  if (!branch.isEmpty()) {
    args.append(branch);
  }

  bool success;
  const QString output = executeGitCommand(args, &success);

  if (!success) {
    return {};
  }

  return parseCommitLogOutput(output);
}

QMap<QString, QList<GitRefDecoration>>
GitIntegration::getCommitRefsMap() const {
  QMap<QString, QList<GitRefDecoration>> result;

  if (!m_isValid) {
    return result;
  }

  bool success = false;
  const QString headHash =
      executeGitCommand({"rev-parse", "HEAD"}, &success).trimmed();
  const bool headValid = success && !headHash.isEmpty();

  const QString refsOutput = executeGitCommand(
      {"for-each-ref", "--format=%(objectname)%00%(*objectname)%00%(refname)",
       "refs/heads", "refs/tags", "refs/remotes"},
      &success);

  if (success && !refsOutput.isEmpty()) {
    const QStringList lines = refsOutput.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
      const QStringList parts = line.split(QChar('\0'));
      if (parts.size() < 3) {
        continue;
      }

      QString hash = parts[0];
      if (!parts[1].isEmpty()) {
        hash = parts[1];
      }
      if (hash.isEmpty()) {
        continue;
      }

      GitRefDecoration decoration;
      const QString refname = parts[2];
      if (refname.startsWith("refs/heads/")) {
        decoration.kind = GitRefDecoration::Kind::LocalBranch;
        decoration.name = refname.mid(11);
      } else if (refname.startsWith("refs/remotes/")) {
        decoration.kind = GitRefDecoration::Kind::RemoteBranch;
        decoration.name = refname.mid(13);
      } else if (refname.startsWith("refs/tags/")) {
        decoration.kind = GitRefDecoration::Kind::Tag;
        decoration.name = refname.mid(10);
      } else {
        continue;
      }

      if (decoration.name.isEmpty()) {
        continue;
      }

      if (headValid && hash == headHash &&
          decoration.kind == GitRefDecoration::Kind::LocalBranch &&
          decoration.name == m_currentBranch) {
        decoration.isHead = true;
      }

      result[hash].append(decoration);
    }
  }

  return result;
}

void GitIntegration::getCommitLogPageAsync(
    const QString &branch, int skip, int limit,
    const std::function<void(QList<GitCommitInfo>)> &callback) {

  QPointer<GitIntegration> self(this);
  const QString repoPath = m_repositoryPath;

  QThreadPool::globalInstance()->start([self, repoPath, branch, skip, limit,
                                        callback]() {
    QList<GitCommitInfo> page;

    if (self && !repoPath.isEmpty() && limit > 0 && skip >= 0) {
      const QString format = "%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P";

      QStringList args = {"log", "--date-order", QString("--skip=%1").arg(skip),
                          QString("--max-count=%1").arg(limit),
                          QString("--pretty=format:%1").arg(format)};
      if (!branch.isEmpty()) {
        args.append(branch);
      }

      bool success = false;
      const QString output =
          self->executeGitCommandAtPath(repoPath, args, &success);
      if (success) {
        page = self->parseCommitLogOutput(output);
      }
    }

    if (!self) {
      return;
    }

    QMetaObject::invokeMethod(
        self,
        [self, callback, page]() {
          if (self && callback) {
            callback(page);
          }
        },
        Qt::QueuedConnection);
  });
}

void GitIntegration::getCommitRefsMapAsync(
    const std::function<void(QMap<QString, QList<GitRefDecoration>>)>
        &callback) {

  QPointer<GitIntegration> self(this);
  const QString repoPath = m_repositoryPath;

  QThreadPool::globalInstance()->start([self, repoPath, callback]() {
    QMap<QString, QList<GitRefDecoration>> refs;

    if (self && !repoPath.isEmpty()) {
      bool headSuccess = false;
      const QString headHash =
          self->executeGitCommandAtPath(repoPath, {"rev-parse", "HEAD"},
                                        &headSuccess)
              .trimmed();

      bool refsSuccess = false;
      const QString refsOutput = self->executeGitCommandAtPath(
          repoPath,
          {"for-each-ref",
           "--format=%(objectname)%00%(*objectname)%00%(refname)", "refs/heads",
           "refs/tags", "refs/remotes"},
          &refsSuccess);

      if (refsSuccess && !refsOutput.isEmpty()) {
        const QStringList lines = refsOutput.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
          const QStringList parts = line.split(QChar('\0'));
          if (parts.size() < 3) {
            continue;
          }

          QString hash = parts[1].isEmpty() ? parts[0] : parts[1];
          if (hash.isEmpty()) {
            continue;
          }

          GitRefDecoration decoration;
          const QString refname = parts[2];
          if (refname.startsWith("refs/heads/")) {
            decoration.kind = GitRefDecoration::Kind::LocalBranch;
            decoration.name = refname.mid(11);
          } else if (refname.startsWith("refs/remotes/")) {
            decoration.kind = GitRefDecoration::Kind::RemoteBranch;
            decoration.name = refname.mid(13);
          } else if (refname.startsWith("refs/tags/")) {
            decoration.kind = GitRefDecoration::Kind::Tag;
            decoration.name = refname.mid(10);
          } else {
            continue;
          }

          if (decoration.name.isEmpty()) {
            continue;
          }

          if (headSuccess && !headHash.isEmpty() && hash == headHash &&
              decoration.kind == GitRefDecoration::Kind::LocalBranch) {
            decoration.isHead = true;
          }

          refs[hash].append(decoration);
        }
      }
    }

    if (!self) {
      return;
    }

    QMetaObject::invokeMethod(
        self,
        [self, callback, refs]() {
          if (self && callback) {
            callback(refs);
          }
        },
        Qt::QueuedConnection);
  });
}

GitCommitInfo
GitIntegration::getCommitDetails(const QString &commitHash) const {
  GitCommitInfo info;

  if (!m_isValid || commitHash.isEmpty()) {
    return info;
  }

  QString format = "%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P%x00%b";

  bool success;
  QString output = executeGitCommand(
      {"show", "-s", QString("--pretty=format:%1").arg(format), commitHash},
      &success);

  if (!success || output.isEmpty()) {
    return info;
  }

  QStringList parts = output.split(QChar('\0'));
  if (parts.size() < 7) {
    return info;
  }

  info.hash = parts[0];
  info.shortHash = parts[1];
  info.author = parts[2];
  info.authorEmail = parts[3];
  info.date = parts[4];
  info.relativeDate = parts[5];
  info.subject = parts[6];

  if (parts.size() > 7) {
    info.parents = parts[7].split(' ', Qt::SkipEmptyParts);
  }

  if (parts.size() > 8) {
    info.body = parts[8].trimmed();
  }

  return info;
}

QString GitIntegration::getCommitDiff(const QString &commitHash) const {
  if (!m_isValid || commitHash.isEmpty()) {
    return QString();
  }

  bool success;
  QString diff =
      executeGitCommand({"show", "--pretty=format:", commitHash}, &success);

  return success ? diff : QString();
}

QString GitIntegration::getCommitAuthor(const QString &commitHash) const {
  if (!m_isValid || commitHash.isEmpty()) {
    return QString();
  }

  bool success;
  QString author = executeGitCommand(
      {"show", "-s", "--format=%an <%ae>", commitHash}, &success);

  return success ? author.trimmed() : QString();
}

QString GitIntegration::getCommitDate(const QString &commitHash) const {
  if (!m_isValid || commitHash.isEmpty()) {
    return QString();
  }

  bool success;
  QString date =
      executeGitCommand({"show", "-s", "--format=%ci", commitHash}, &success);

  return success ? date.trimmed() : QString();
}

QString GitIntegration::getCommitMessage(const QString &commitHash) const {
  if (!m_isValid || commitHash.isEmpty()) {
    return QString();
  }

  bool success;
  QString message =
      executeGitCommand({"show", "-s", "--format=%B", commitHash}, &success);

  return success ? message.trimmed() : QString();
}

QList<GitBlameLineInfo>
GitIntegration::getBlameInfo(const QString &filePath) const {
  QList<GitBlameLineInfo> result;

  if (!m_isValid) {
    return result;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  QString output = executeGitCommand(
      {"blame", "--line-porcelain", "--", relativePath}, &success);

  if (success && !output.isEmpty()) {
    QStringList lines = output.split('\n');
    GitBlameLineInfo current;
    bool hasCurrent = false;
    QRegularExpression headerPattern(
        R"(^([0-9a-f]{7,40})\s+\d+\s+(\d+)(?:\s+\d+)?)");

    for (const QString &line : lines) {
      if (line.startsWith('\t')) {
        if (hasCurrent) {
          result.append(current);
          hasCurrent = false;
        }
        continue;
      }

      QRegularExpressionMatch headerMatch = headerPattern.match(line);
      if (headerMatch.hasMatch()) {
        current = GitBlameLineInfo();
        current.shortHash = headerMatch.captured(1).left(7);
        current.lineNumber = headerMatch.captured(2).toInt();
        hasCurrent = true;
        continue;
      }

      if (!hasCurrent) {
        continue;
      }

      if (line.startsWith("author ")) {
        current.author = line.mid(QString("author ").size());
        continue;
      }

      if (line.startsWith("author-mail ")) {
        current.authorEmail =
            line.mid(QString("author-mail ").size()).remove('<').remove('>');
        continue;
      }

      if (line.startsWith("author-time ")) {
        qint64 timestamp =
            line.mid(QString("author-time ").size()).toLongLong();
        QDateTime dt = QDateTime::fromSecsSinceEpoch(timestamp);
        current.date = dt.toString(Qt::ISODate);
        qint64 secsAgo =
            QDateTime::currentDateTime().toSecsSinceEpoch() - timestamp;
        if (secsAgo < 60)
          current.relativeDate = "just now";
        else if (secsAgo < 3600)
          current.relativeDate = QString("%1 minutes ago").arg(secsAgo / 60);
        else if (secsAgo < 86400)
          current.relativeDate = QString("%1 hours ago").arg(secsAgo / 3600);
        else if (secsAgo < 2592000)
          current.relativeDate = QString("%1 days ago").arg(secsAgo / 86400);
        else if (secsAgo < 31536000)
          current.relativeDate =
              QString("%1 months ago").arg(secsAgo / 2592000);
        else
          current.relativeDate =
              QString("%1 years ago").arg(secsAgo / 31536000);
        continue;
      }

      if (line.startsWith("summary ")) {
        current.summary = line.mid(QString("summary ").size());
        continue;
      }
    }

    if (!result.isEmpty()) {
      return result;
    }
  }

  output = executeGitCommand({"blame", "--", relativePath}, &success);
  if (!success || output.isEmpty()) {
    return result;
  }

  QStringList blameLines = output.split('\n', Qt::SkipEmptyParts);
  int lineIndex = 1;
  QRegularExpression blamePattern(
      R"(^([0-9a-f]{7,40})\s+\((.+?)\s+(\d{4}-\d{2}-\d{2})\s+.+?\)\s(.*)$)");

  for (const QString &blameLine : blameLines) {
    QRegularExpressionMatch match = blamePattern.match(blameLine);
    if (!match.hasMatch()) {
      lineIndex++;
      continue;
    }

    GitBlameLineInfo info;
    info.lineNumber = lineIndex++;
    info.shortHash = match.captured(1).left(7);
    info.author = match.captured(2).trimmed();
    info.relativeDate = match.captured(3);
    info.summary = match.captured(4).trimmed();
    result.append(info);
  }

  return result;
}

void GitIntegration::refresh() {
  if (!m_isValid) {
    return;
  }

  updateCurrentBranch();
  emit statusChanged();
}

GitDiffHunk GitIntegration::getDiffHunkAtLine(const QString &filePath,
                                              int lineNumber) const {
  GitDiffHunk result;
  if (!m_isValid) {
    return result;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  QString output =
      executeGitCommand({"diff", "-U3", "--", relativePath}, &success);
  if (output.isEmpty()) {
    output = executeGitCommand({"diff", "-U3", "--cached", "--", relativePath},
                               &success);
  }
  if (output.isEmpty()) {
    return result;
  }

  QRegularExpression hunkHeader(R"(@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@.*)");
  QStringList lines = output.split('\n');

  int i = 0;
  while (i < lines.size()) {
    QRegularExpressionMatch match = hunkHeader.match(lines[i]);
    if (!match.hasMatch()) {
      ++i;
      continue;
    }

    int hunkStart = match.captured(1).toInt();
    int hunkCount = match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
    QString header = lines[i];
    QStringList hunkLines;
    ++i;

    int newLine = hunkStart;
    while (i < lines.size() && !lines[i].startsWith("@@") &&
           !lines[i].startsWith("diff ")) {
      hunkLines.append(lines[i]);
      if (!lines[i].isEmpty() && lines[i][0] != '-') {
        ++newLine;
      }
      ++i;
    }

    if (lineNumber >= hunkStart && lineNumber < hunkStart + hunkCount) {
      result.startLine = hunkStart;
      result.lineCount = hunkCount;
      result.header = header;
      result.lines = hunkLines;
      return result;
    }
  }

  return result;
}

QList<GitCommitFileStat>
GitIntegration::getCommitFileStats(const QString &commitHash) const {
  QList<GitCommitFileStat> result;
  if (!m_isValid || commitHash.isEmpty()) {
    return result;
  }

  bool success;
  QString output = executeGitCommand(
      {"show", "--numstat", "--pretty=format:", commitHash}, &success);
  if (!success || output.isEmpty()) {
    return result;
  }

  for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
    QStringList parts = line.split('\t');
    if (parts.size() >= 3) {
      GitCommitFileStat stat;
      stat.additions = parts[0] == "-" ? 0 : parts[0].toInt();
      stat.deletions = parts[1] == "-" ? 0 : parts[1].toInt();
      stat.filePath = parts[2];
      result.append(stat);
    }
  }
  return result;
}

QList<GitCommitInfo> GitIntegration::getFileLog(const QString &filePath,
                                                int maxCount) const {
  QList<GitCommitInfo> result;
  if (!m_isValid || filePath.isEmpty()) {
    return result;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  QString format = "%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P%x00%b";
  bool success;
  QString output = executeGitCommand({"log", QString("-n%1").arg(maxCount),
                                      QString("--pretty=format:%1").arg(format),
                                      "--follow", "--", relativePath},
                                     &success);

  if (!success || output.isEmpty()) {
    return result;
  }

  QStringList entries = output.split('\n');
  QString currentEntry;
  for (const QString &line : entries) {
    if (!currentEntry.isEmpty() && line.contains(QChar('\0'))) {

      QStringList parts = currentEntry.split(QChar('\0'));
      if (parts.size() >= 7) {
        GitCommitInfo info;
        info.hash = parts[0];
        info.shortHash = parts[1];
        info.author = parts[2];
        info.authorEmail = parts[3];
        info.date = parts[4];
        info.relativeDate = parts[5];
        info.subject = parts[6];
        if (parts.size() > 7)
          info.parents = parts[7].split(' ', Qt::SkipEmptyParts);
        if (parts.size() > 8)
          info.body = parts[8].trimmed();
        result.append(info);
      }
      currentEntry = line;
    } else {
      if (!currentEntry.isEmpty())
        currentEntry += '\n';
      currentEntry += line;
    }
  }

  if (!currentEntry.isEmpty()) {
    QStringList parts = currentEntry.split(QChar('\0'));
    if (parts.size() >= 7) {
      GitCommitInfo info;
      info.hash = parts[0];
      info.shortHash = parts[1];
      info.author = parts[2];
      info.authorEmail = parts[3];
      info.date = parts[4];
      info.relativeDate = parts[5];
      info.subject = parts[6];
      if (parts.size() > 7)
        info.parents = parts[7].split(' ', Qt::SkipEmptyParts);
      if (parts.size() > 8)
        info.body = parts[8].trimmed();
      result.append(info);
    }
  }

  return result;
}

QList<GitCommitInfo> GitIntegration::getLineHistory(const QString &filePath,
                                                    int startLine,
                                                    int endLine) const {
  QList<GitCommitInfo> result;
  if (!m_isValid || filePath.isEmpty()) {
    return result;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  QString range =
      QString("-L%1,%2:%3").arg(startLine).arg(endLine).arg(relativePath);
  bool success;
  QString output = executeGitCommand(
      {"log", "--no-patch",
       "--pretty=format:%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s", range},
      &success);

  if (!success || output.isEmpty()) {
    return result;
  }

  for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
    QStringList parts = line.split(QChar('\0'));
    if (parts.size() >= 7) {
      GitCommitInfo info;
      info.hash = parts[0];
      info.shortHash = parts[1];
      info.author = parts[2];
      info.authorEmail = parts[3];
      info.date = parts[4];
      info.relativeDate = parts[5];
      info.subject = parts[6];
      result.append(info);
    }
  }

  return result;
}

QString GitIntegration::getFileAtRevision(const QString &filePath,
                                          const QString &revision) const {
  if (!m_isValid || filePath.isEmpty() || revision.isEmpty()) {
    return QString();
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  QString content = executeGitCommand(
      {"show", QString("%1:%2").arg(revision).arg(relativePath)}, &success);

  return success ? content : QString();
}

QString GitIntegration::getBranchDiff(const QString &branch1,
                                      const QString &branch2) const {
  if (!m_isValid || branch1.isEmpty() || branch2.isEmpty()) {
    return QString();
  }

  bool success;
  QString diff = executeGitCommand(
      {"diff", QString("%1...%2").arg(branch1).arg(branch2)}, &success);

  return success ? diff : QString();
}

bool GitIntegration::getAheadBehind(int &ahead, int &behind) const {
  ahead = 0;
  behind = 0;

  if (!m_isValid) {
    return false;
  }

  bool success;
  QString output = executeGitCommand(
      {"rev-list", "--left-right", "--count", "@{upstream}...HEAD"}, &success);

  if (!success || output.trimmed().isEmpty()) {
    return false;
  }

  QStringList parts = output.trimmed().split('\t');
  if (parts.size() == 2) {
    behind = parts[0].toInt();
    ahead = parts[1].toInt();
    return true;
  }
  return false;
}

bool GitIntegration::isDirty() const {
  if (!m_isValid) {
    return false;
  }

  bool success;
  QString output = executeGitCommand({"status", "--porcelain"}, &success);
  return success && !output.trimmed().isEmpty();
}

bool GitIntegration::stageHunkAtLine(const QString &filePath, int lineNumber) {
  GitDiffHunk hunk = getDiffHunkAtLine(filePath, lineNumber);
  if (hunk.lines.isEmpty()) {
    return false;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  QString patch = QString("diff --git a/%1 b/%1\n--- a/%1\n+++ b/%1\n%2\n")
                      .arg(relativePath)
                      .arg(hunk.header);
  for (const QString &line : hunk.lines) {
    patch += line + '\n';
  }

  QString tempPath = QDir::temp().filePath("lightpad_stage_hunk.patch");
  QFile tempFile(tempPath);
  if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  tempFile.write(patch.toUtf8());
  tempFile.close();

  bool success;
  executeGitCommand({"apply", "--cached", tempPath}, &success);
  QFile::remove(tempPath);

  if (success) {
    emit statusChanged();
  }
  return success;
}

bool GitIntegration::revertHunkAtLine(const QString &filePath, int lineNumber) {
  GitDiffHunk hunk = getDiffHunkAtLine(filePath, lineNumber);
  if (hunk.lines.isEmpty()) {
    return false;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  QString patch = QString("diff --git a/%1 b/%1\n--- a/%1\n+++ b/%1\n%2\n")
                      .arg(relativePath)
                      .arg(hunk.header);
  for (const QString &line : hunk.lines) {
    patch += line + '\n';
  }

  QString tempPath = QDir::temp().filePath("lightpad_revert_hunk.patch");
  QFile tempFile(tempPath);
  if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  tempFile.write(patch.toUtf8());
  tempFile.close();

  bool success;
  executeGitCommand({"apply", "--reverse", tempPath}, &success);
  QFile::remove(tempPath);

  if (success) {
    emit statusChanged();
  }
  return success;
}

QString GitIntegration::workingPath() const { return m_workingPath; }

void GitIntegration::setWorkingPath(const QString &path) {
  QFileInfo info(path);
  if (info.isFile()) {
    m_workingPath = info.dir().absolutePath();
  } else {
    m_workingPath = QDir(path).absolutePath();
  }
}

QString GitIntegration::executeGitCommandAtPath(const QString &path,
                                                const QStringList &args,
                                                bool *success) const {
  QProcess process;
  process.setWorkingDirectory(path);
  process.start("git", args);

  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    LOG_WARNING("Git command timed out: git " + args.join(" "));
    if (success)
      *success = false;
    return QString();
  }

  if (success) {
    *success = (process.exitCode() == 0);
  }

  const QString error = QString::fromUtf8(process.readAllStandardError());
  const QString output =
      QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  recordCommand(args, path, output, error, process.exitCode());

  if (process.exitCode() != 0) {
    LOG_DEBUG("Git command failed: git " + args.join(" ") + " - " + error);
    return error;
  }

  return output;
}

bool GitIntegration::initRepository(const QString &path) {
  QDir dir(path);
  if (!dir.exists()) {
    emit errorOccurred("Directory does not exist: " + path);
    return false;
  }

  bool success;
  executeGitCommandAtPath(path, {"init"}, &success);

  if (success) {

    m_repositoryPath = dir.absolutePath();
    m_isValid = true;
    m_workingPath = m_repositoryPath;
    updateCurrentBranch();

    emit repositoryInitialized(m_repositoryPath);
    emit operationCompleted("Repository initialized at: " + m_repositoryPath);
    emit statusChanged();

    LOG_INFO("Git repository initialized at: " + m_repositoryPath);
  } else {
    emit errorOccurred("Failed to initialize repository at: " + path);
  }

  return success;
}

QList<GitRemoteInfo> GitIntegration::getRemotes() const {
  QList<GitRemoteInfo> result;

  if (!m_isValid) {
    return result;
  }

  bool success;
  QString output = executeGitCommand({"remote", "-v"}, &success);

  if (!success || output.isEmpty()) {
    return result;
  }

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  QMap<QString, GitRemoteInfo> remoteMap;

  for (const QString &line : lines) {
    QStringList parts = line.split(QRegularExpression("\\s+"));
    if (parts.size() >= 2) {
      QString name = parts[0];
      QString url = parts[1];
      QString type = parts.size() > 2 ? parts[2] : "";

      if (!remoteMap.contains(name)) {
        GitRemoteInfo info;
        info.name = name;
        remoteMap[name] = info;
      }

      if (type.contains("fetch")) {
        remoteMap[name].fetchUrl = url;
      } else if (type.contains("push")) {
        remoteMap[name].pushUrl = url;
      } else {

        remoteMap[name].fetchUrl = url;
        remoteMap[name].pushUrl = url;
      }
    }
  }

  for (const auto &info : remoteMap) {
    result.append(info);
  }

  return result;
}

bool GitIntegration::addRemote(const QString &name, const QString &url) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (name.isEmpty() || url.isEmpty()) {
    emit errorOccurred("Remote name and URL cannot be empty");
    return false;
  }

  bool success;
  executeGitCommand({"remote", "add", name, url}, &success);

  if (success) {
    emit operationCompleted("Remote added: " + name);
  } else {
    emit errorOccurred("Failed to add remote: " + name);
  }

  return success;
}

bool GitIntegration::removeRemote(const QString &name) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  executeGitCommand({"remote", "remove", name}, &success);

  if (success) {
    emit operationCompleted("Remote removed: " + name);
  } else {
    emit errorOccurred("Failed to remove remote: " + name);
  }

  return success;
}

bool GitIntegration::fetch(const QString &remoteName) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  executeGitCommand({"fetch", remoteName}, &success);

  if (success) {
    emit operationCompleted("Fetched from: " + remoteName);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to fetch from: " + remoteName);
  }

  return success;
}

bool GitIntegration::pull(const QString &remoteName,
                          const QString &branchName) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QStringList args = {"pull", remoteName};
  QString branch = branchName.isEmpty() ? m_currentBranch : branchName;
  if (!branch.isEmpty()) {
    args << branch;
  }

  bool success;
  QString output = executeGitCommand(args, &success);

  if (success) {
    emit pullCompleted(remoteName, branch);
    emit operationCompleted("Pulled from: " + remoteName + "/" + branch);
    updateCurrentBranch();
    emit statusChanged();

    if (hasMergeConflicts()) {
      QStringList conflicts = getConflictedFiles();
      emit mergeConflictsDetected(conflicts);
    }
  } else {

    if (hasMergeConflicts()) {
      QStringList conflicts = getConflictedFiles();
      emit mergeConflictsDetected(conflicts);
      emit errorOccurred("Pull resulted in merge conflicts");
    } else {
      emit errorOccurred("Failed to pull from: " + remoteName);
    }
  }

  return success;
}

bool GitIntegration::push(const QString &remoteName, const QString &branchName,
                          bool setUpstream) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QStringList args = {"push"};
  if (setUpstream) {
    args << "-u";
  }
  args << remoteName;

  QString branch = branchName.isEmpty() ? m_currentBranch : branchName;
  if (!branch.isEmpty()) {
    args << branch;
  }

  bool success;
  executeGitCommand(args, &success);

  if (success) {
    emit pushCompleted(remoteName, branch);
    emit operationCompleted("Pushed to: " + remoteName + "/" + branch);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to push to: " + remoteName);
  }

  return success;
}

bool GitIntegration::hasMergeConflicts() const {
  if (!m_isValid) {
    return false;
  }

  return !getConflictedFiles().isEmpty();
}

QStringList GitIntegration::getConflictedFiles() const {
  QStringList result;

  if (!m_isValid) {
    return result;
  }

  bool success;
  QString output =
      executeGitCommand({"diff", "--name-only", "--diff-filter=U"}, &success);

  if (success && !output.isEmpty()) {
    result = output.split('\n', Qt::SkipEmptyParts);
  }

  return result;
}

QList<GitConflictMarker>
GitIntegration::getConflictMarkers(const QString &filePath) const {
  QList<GitConflictMarker> result;

  QString fullPath = filePath;
  if (!fullPath.startsWith('/') && !m_repositoryPath.isEmpty()) {
    fullPath = m_repositoryPath + "/" + filePath;
  }

  QFile file(fullPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return result;
  }

  QTextStream in(&file);
  QStringList lines;
  while (!in.atEnd()) {
    lines << in.readLine();
  }
  file.close();

  GitConflictMarker currentMarker;
  bool inConflict = false;
  bool inOurs = false;

  for (int i = 0; i < lines.size(); ++i) {
    const QString &line = lines[i];

    if (line.startsWith("<<<<<<<")) {
      currentMarker = GitConflictMarker();
      currentMarker.startLine = i + 1;
      inConflict = true;
      inOurs = true;
    } else if (line.startsWith("=======") && inConflict) {
      currentMarker.separatorLine = i + 1;
      inOurs = false;
    } else if (line.startsWith(">>>>>>>") && inConflict) {
      currentMarker.endLine = i + 1;
      result.append(currentMarker);
      inConflict = false;
    } else if (inConflict) {
      if (inOurs) {
        currentMarker.oursContent += line + "\n";
      } else {
        currentMarker.theirsContent += line + "\n";
      }
    }
  }

  return result;
}

bool GitIntegration::resolveConflictOurs(const QString &filePath) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  executeGitCommand({"checkout", "--ours", "--", relativePath}, &success);

  if (success) {

    executeGitCommand({"add", "--", relativePath}, &success);
    if (success) {
      emit operationCompleted("Conflict resolved (ours): " + relativePath);
      emit statusChanged();
    }
  } else {
    emit errorOccurred("Failed to resolve conflict: " + relativePath);
  }

  return success;
}

bool GitIntegration::resolveConflictTheirs(const QString &filePath) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  executeGitCommand({"checkout", "--theirs", "--", relativePath}, &success);

  if (success) {

    executeGitCommand({"add", "--", relativePath}, &success);
    if (success) {
      emit operationCompleted("Conflict resolved (theirs): " + relativePath);
      emit statusChanged();
    }
  } else {
    emit errorOccurred("Failed to resolve conflict: " + relativePath);
  }

  return success;
}

bool GitIntegration::markConflictResolved(const QString &filePath) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QString relativePath = filePath;
  if (filePath.startsWith(m_repositoryPath)) {
    relativePath = filePath.mid(m_repositoryPath.length() + 1);
  }

  bool success;
  executeGitCommand({"add", "--", relativePath}, &success);

  if (success) {
    emit operationCompleted("Marked as resolved: " + relativePath);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to mark as resolved: " + relativePath);
  }

  return success;
}

bool GitIntegration::abortMerge() {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  executeGitCommand({"merge", "--abort"}, &success);

  if (success) {
    emit operationCompleted("Merge aborted");
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to abort merge");
  }

  return success;
}

bool GitIntegration::continueMerge() {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (hasMergeConflicts()) {
    emit errorOccurred("Cannot continue merge: unresolved conflicts remain");
    return false;
  }

  bool success;
  executeGitCommand({"commit", "--no-edit"}, &success);

  if (success) {
    emit operationCompleted("Merge completed");
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to complete merge");
  }

  return success;
}

bool GitIntegration::isMergeInProgress() const {
  if (!m_isValid) {
    return false;
  }

  QFile mergeHead(m_repositoryPath + "/.git/MERGE_HEAD");
  return mergeHead.exists();
}

bool GitIntegration::mergeBranch(const QString &branchName) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  executeGitCommand({"merge", branchName}, &success);

  if (success) {
    emit operationCompleted("Merged branch: " + branchName);
    emit statusChanged();
  } else {

    if (hasMergeConflicts()) {
      QStringList conflicts = getConflictedFiles();
      emit mergeConflictsDetected(conflicts);
      emit errorOccurred("Merge conflicts detected");
    } else {
      emit errorOccurred("Failed to merge branch: " + branchName);
    }
  }

  return success && !hasMergeConflicts();
}

QList<GitStashEntry> GitIntegration::getStashList() const {
  QList<GitStashEntry> result;

  if (!m_isValid) {
    return result;
  }

  bool success;
  QString output = executeGitCommand({"stash", "list"}, &success);

  if (!success || output.isEmpty()) {
    return result;
  }

  return parseStashListOutput(output);
}

QList<GitStashEntry>
GitIntegration::parseStashListOutput(const QString &output) const {
  QList<GitStashEntry> result;

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);

  QRegularExpression stashPattern(
      R"(stash@\{(\d+)\}: (?:On|WIP on) ([^:]+): (.+))");

  constexpr int MIN_HASH_LENGTH = 4;
  constexpr int MAX_ABBREV_HASH_LENGTH = 12;

  for (const QString &line : lines) {
    QRegularExpressionMatch match = stashPattern.match(line);
    if (match.hasMatch()) {
      GitStashEntry entry;
      entry.index = match.captured(1).toInt();
      entry.branch = match.captured(2).trimmed();
      entry.message = match.captured(3).trimmed();

      QString msgPart = entry.message;
      int spaceIndex = msgPart.indexOf(' ');
      if (spaceIndex >= MIN_HASH_LENGTH &&
          spaceIndex <= MAX_ABBREV_HASH_LENGTH) {
        QString potentialHash = msgPart.left(spaceIndex);

        static QRegularExpression hexPattern("^[0-9a-f]+$");
        if (hexPattern.match(potentialHash).hasMatch()) {
          entry.commitHash = potentialHash;
          entry.message = msgPart.mid(spaceIndex + 1);
        }
      }

      result.append(entry);
    }
  }

  return result;
}

QList<GitStashEntry> GitIntegration::stashList() const {
  return getStashList();
}

bool GitIntegration::stash(const QString &message, bool includeUntracked) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QStringList args = {"stash", "push"};
  if (includeUntracked) {
    args << "-u";
  }
  if (!message.isEmpty()) {
    args << "-m" << message;
  }

  bool success;
  executeGitCommand(args, &success);

  if (success) {
    emit operationCompleted("Changes stashed" +
                            (message.isEmpty() ? "" : ": " + message));
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to stash changes");
  }

  return success;
}

bool GitIntegration::stashPop(int index) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  QString stashRef = QString("stash@{%1}").arg(index);
  executeGitCommand({"stash", "pop", stashRef}, &success);

  if (success) {
    emit operationCompleted("Stash popped");
    emit statusChanged();

    if (hasMergeConflicts()) {
      QStringList conflicts = getConflictedFiles();
      emit mergeConflictsDetected(conflicts);
    }
  } else {
    if (hasMergeConflicts()) {
      QStringList conflicts = getConflictedFiles();
      emit mergeConflictsDetected(conflicts);
      emit errorOccurred("Stash pop resulted in conflicts");
    } else {
      emit errorOccurred("Failed to pop stash");
    }
  }

  return success;
}

bool GitIntegration::stashApply(int index) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  QString stashRef = QString("stash@{%1}").arg(index);
  executeGitCommand({"stash", "apply", stashRef}, &success);

  if (success) {
    emit operationCompleted(QString("Stash %1 applied").arg(index));
    emit statusChanged();

    if (hasMergeConflicts()) {
      QStringList conflicts = getConflictedFiles();
      emit mergeConflictsDetected(conflicts);
    }
  } else {
    if (hasMergeConflicts()) {
      QStringList conflicts = getConflictedFiles();
      emit mergeConflictsDetected(conflicts);
      emit errorOccurred("Stash apply resulted in conflicts");
    } else {
      emit errorOccurred(QString("Failed to apply stash %1").arg(index));
    }
  }

  return success;
}

bool GitIntegration::stashDrop(int index) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  QString stashRef = QString("stash@{%1}").arg(index);
  executeGitCommand({"stash", "drop", stashRef}, &success);

  if (success) {
    emit operationCompleted(QString("Stash %1 dropped").arg(index));
    emit statusChanged();
  } else {
    emit errorOccurred(QString("Failed to drop stash %1").arg(index));
  }

  return success;
}

bool GitIntegration::stashClear() {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success;
  executeGitCommand({"stash", "clear"}, &success);

  if (success) {
    emit operationCompleted("All stashes cleared");
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to clear stashes");
  }

  return success;
}

bool GitIntegration::cherryPick(const QString &commitHash) {
  if (!m_isValid)
    return false;

  bool success;
  QString output = executeGitCommand({"cherry-pick", commitHash}, &success);

  if (success) {
    emit operationCompleted(
        QString("Cherry-picked %1").arg(commitHash.left(7)));
    emit statusChanged();
  } else {
    emit errorOccurred(QString("Cherry-pick failed: %1").arg(output.trimmed()));
  }

  return success;
}

bool GitIntegration::revertCommit(const QString &commitHash) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (commitHash.isEmpty()) {
    emit errorOccurred("Commit hash cannot be empty");
    return false;
  }

  bool success;
  QString output =
      executeGitCommand({"revert", "--no-edit", commitHash}, &success);

  if (success) {
    emit operationCompleted(
        QString("Reverted commit %1").arg(commitHash.left(7)));
    emit statusChanged();
  } else {
    emit errorOccurred(QString("Revert failed: %1").arg(output.trimmed()));
  }

  return success;
}

bool GitIntegration::resetToCommit(const QString &commitHash,
                                   const QString &mode) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (commitHash.isEmpty()) {
    emit errorOccurred("Commit hash cannot be empty");
    return false;
  }

  QStringList validModes = {"soft", "mixed", "hard"};
  QString resetMode = mode.toLower();
  if (!validModes.contains(resetMode)) {
    emit errorOccurred("Invalid reset mode: " + mode);
    return false;
  }

  bool success;
  QString output =
      executeGitCommand({"reset", "--" + resetMode, commitHash}, &success);

  if (success) {
    updateCurrentBranch();
    emit operationCompleted(
        QString("Reset (%1) to %2").arg(resetMode, commitHash.left(7)));
    emit statusChanged();
  } else {
    emit errorOccurred(QString("Reset failed: %1").arg(output.trimmed()));
  }

  return success;
}

bool GitIntegration::rewordCommit(const QString &commitHash,
                                  const QString &newMessage) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (commitHash.isEmpty() || newMessage.isEmpty()) {
    emit errorOccurred("Commit hash and new message cannot be empty");
    return false;
  }

  bool success;
  QString headHash =
      executeGitCommand({"rev-parse", "HEAD"}, &success).trimmed();

  if (!success) {
    emit errorOccurred("Failed to determine HEAD commit");
    return false;
  }

  if (headHash.startsWith(commitHash) || commitHash.startsWith(headHash)) {
    executeGitCommand({"commit", "--amend", "-m", newMessage}, &success);

    if (success) {
      emit operationCompleted(
          QString("Reworded commit %1").arg(commitHash.left(7)));
      emit statusChanged();
    } else {
      emit errorOccurred("Failed to reword commit message");
    }
    return success;
  }

  emit errorOccurred("Can only reword the most recent (HEAD) commit. Use "
                     "interactive rebase to reword older commits.");
  return false;
}

bool GitIntegration::dropCommit(const QString &commitHash) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (commitHash.isEmpty()) {
    emit errorOccurred("Commit hash cannot be empty");
    return false;
  }

  bool success;
  QString headHash =
      executeGitCommand({"rev-parse", "HEAD"}, &success).trimmed();

  if (!success) {
    emit errorOccurred("Failed to determine HEAD commit");
    return false;
  }

  if (headHash.startsWith(commitHash) || commitHash.startsWith(headHash)) {
    executeGitCommand({"reset", "--hard", "HEAD~1"}, &success);
    if (success) {
      updateCurrentBranch();
      emit operationCompleted(
          QString("Dropped commit %1").arg(commitHash.left(7)));
      emit statusChanged();
    } else {
      emit errorOccurred("Failed to drop HEAD commit");
    }
    return success;
  }

  QString fullHash =
      executeGitCommand({"rev-parse", commitHash}, &success).trimmed();
  if (!success) {
    emit errorOccurred(QString("Failed to resolve commit %1").arg(commitHash));
    return false;
  }

  QProcess proc;
  proc.setWorkingDirectory(m_repositoryPath);
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

  QTemporaryFile dropScript;
  dropScript.setAutoRemove(false);
  dropScript.setFileTemplate(QDir::tempPath() + "/lightpad-drop-XXXXXX.sh");
  if (!dropScript.open()) {
    emit errorOccurred("Failed to create drop script");
    return false;
  }
  QString shortHashForDrop = fullHash.left(7);
  QTextStream scriptOut(&dropScript);
  scriptOut << "#!/bin/sh\n"
            << "grep -v '^pick " << shortHashForDrop << "' \"$1\" > \"$1.tmp\""
            << " && mv \"$1.tmp\" \"$1\"\n";
  dropScript.close();
  QFile::setPermissions(dropScript.fileName(), QFileDevice::ReadOwner |
                                                   QFileDevice::WriteOwner |
                                                   QFileDevice::ExeOwner);

  env.insert("GIT_SEQUENCE_EDITOR", dropScript.fileName());
  proc.setProcessEnvironment(env);
  proc.start("git", {"rebase", "-i", fullHash + "~1"});
  proc.waitForFinished(10000);

  QFile::remove(dropScript.fileName());

  if (proc.exitCode() == 0) {
    updateCurrentBranch();
    emit operationCompleted(
        QString("Dropped commit %1").arg(commitHash.left(7)));
    emit statusChanged();
    return true;
  }

  executeGitCommand({"rebase", "--abort"}, nullptr);
  emit errorOccurred(
      QString("Failed to drop commit %1: %2")
          .arg(commitHash.left(7), proc.readAllStandardError().trimmed()));
  return false;
}

bool GitIntegration::squashCommits(const QStringList &commitHashes,
                                   const QString &newMessage) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (commitHashes.size() < 2) {
    emit errorOccurred("Need at least two commits to squash");
    return false;
  }

  if (newMessage.isEmpty()) {
    emit errorOccurred("Squash message cannot be empty");
    return false;
  }

  bool success;
  QString headHash =
      executeGitCommand({"rev-parse", "HEAD"}, &success).trimmed();
  if (!success) {
    emit errorOccurred("Failed to determine HEAD commit");
    return false;
  }

  QStringList fullHashes;
  for (const QString &h : commitHashes) {
    QString full = executeGitCommand({"rev-parse", h}, &success).trimmed();
    if (!success) {
      emit errorOccurred(QString("Failed to resolve commit %1").arg(h));
      return false;
    }
    fullHashes.append(full);
  }

  QStringList logOutput =
      executeGitCommand({"log", "--format=%H", "HEAD"}, &success)
          .trimmed()
          .split('\n');

  int earliestIdx = -1;
  for (int i = 0; i < logOutput.size(); ++i) {
    for (const QString &fh : fullHashes) {
      if (logOutput[i].startsWith(fh) || fh.startsWith(logOutput[i])) {
        earliestIdx = i;
      }
    }
  }

  if (earliestIdx < 0) {
    emit errorOccurred("Could not find commits in history");
    return false;
  }

  QString resetTarget =
      (earliestIdx + 1 < logOutput.size()) ? logOutput[earliestIdx + 1] : "";

  if (resetTarget.isEmpty()) {
    executeGitCommand({"reset", "--soft", "--root"}, &success);
  } else {
    executeGitCommand({"reset", "--soft", resetTarget}, &success);
  }

  if (!success) {
    emit errorOccurred("Failed to soft reset for squash");
    return false;
  }

  executeGitCommand({"commit", "-m", newMessage}, &success);
  if (!success) {
    emit errorOccurred("Failed to create squashed commit");
    return false;
  }

  updateCurrentBranch();
  emit operationCompleted(
      QString("Squashed %1 commits").arg(commitHashes.size()));
  emit statusChanged();
  return true;
}

bool GitIntegration::moveCommitToBranch(const QString &commitHash,
                                        const QString &targetBranch) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  if (commitHash.isEmpty() || targetBranch.isEmpty()) {
    emit errorOccurred("Commit hash and target branch cannot be empty");
    return false;
  }

  bool success;
  QString currentBranch =
      executeGitCommand({"rev-parse", "--abbrev-ref", "HEAD"}, &success)
          .trimmed();
  if (!success || currentBranch.isEmpty()) {
    emit errorOccurred("Failed to determine current branch");
    return false;
  }

  if (currentBranch == targetBranch) {
    emit errorOccurred("Commit is already on the target branch");
    return false;
  }

  if (isDirty()) {
    emit errorOccurred(
        "Working tree has uncommitted changes. Please commit or stash "
        "them before moving commits between branches.");
    return false;
  }

  executeGitCommand({"checkout", targetBranch}, &success);
  if (!success) {
    emit errorOccurred(
        QString("Failed to checkout branch %1").arg(targetBranch));
    return false;
  }

  executeGitCommand({"cherry-pick", commitHash}, &success);
  if (!success) {
    executeGitCommand({"cherry-pick", "--abort"}, nullptr);
    executeGitCommand({"checkout", currentBranch}, nullptr);
    emit errorOccurred(QString("Failed to cherry-pick %1 onto %2")
                           .arg(commitHash.left(7), targetBranch));
    return false;
  }

  executeGitCommand({"checkout", currentBranch}, &success);
  if (!success) {
    emit errorOccurred("Failed to return to original branch");
    return false;
  }

  if (!dropCommit(commitHash)) {
    return false;
  }

  updateCurrentBranch();
  emit operationCompleted(
      QString("Moved commit %1 to %2").arg(commitHash.left(7), targetBranch));
  emit statusChanged();
  return true;
}

QList<QPair<QString, QString>> GitIntegration::listWorktrees() const {
  QList<QPair<QString, QString>> result;
  if (!m_isValid)
    return result;

  bool success;
  QString output =
      executeGitCommand({"worktree", "list", "--porcelain"}, &success);
  if (!success)
    return result;

  QString currentPath;
  for (const QString &line : output.split('\n')) {
    if (line.startsWith("worktree ")) {
      currentPath = line.mid(9);
    } else if (line.startsWith("branch ")) {
      QString branch = line.mid(7);
      if (branch.startsWith("refs/heads/"))
        branch = branch.mid(11);
      result.append({currentPath, branch});
    } else if (line.trimmed().isEmpty()) {
      if (!currentPath.isEmpty() &&
          (result.isEmpty() || result.last().first != currentPath)) {
        result.append({currentPath, "(detached)"});
      }
      currentPath.clear();
    }
  }

  return result;
}

bool GitIntegration::addWorktree(const QString &path, const QString &branch,
                                 bool createBranch) {
  if (!m_isValid)
    return false;

  QStringList args = {"worktree", "add"};
  if (createBranch)
    args << "-b";
  args << path << branch;

  bool success;
  QString output = executeGitCommand(args, &success);

  if (success) {
    emit operationCompleted(QString("Added worktree at %1").arg(path));
  } else {
    emit errorOccurred(
        QString("Failed to add worktree: %1").arg(output.trimmed()));
  }

  return success;
}

bool GitIntegration::removeWorktree(const QString &path) {
  if (!m_isValid)
    return false;

  bool success;
  QString output = executeGitCommand({"worktree", "remove", path}, &success);

  if (success) {
    emit operationCompleted(QString("Removed worktree at %1").arg(path));
  } else {
    emit errorOccurred(
        QString("Failed to remove worktree: %1").arg(output.trimmed()));
  }

  return success;
}

QMap<int, qint64>
GitIntegration::getBlameTimestamps(const QString &filePath) const {
  QMap<int, qint64> result;
  if (!m_isValid)
    return result;

  bool success;
  QString output =
      executeGitCommand({"blame", "--line-porcelain", filePath}, &success);
  if (!success)
    return result;

  int currentLine = 0;
  qint64 currentTimestamp = 0;
  static const QRegularExpression headerRe(
      "^([0-9a-f]{7,40})\\s+\\d+\\s+(\\d+)");

  for (const QString &line : output.split('\n')) {
    QRegularExpressionMatch match = headerRe.match(line);
    if (match.hasMatch()) {
      currentLine = match.captured(2).toInt();
    } else if (line.startsWith("author-time ")) {
      currentTimestamp = line.mid(12).toLongLong();
      if (currentLine > 0)
        result[currentLine] = currentTimestamp;
    }
  }

  return result;
}

QList<GitTagInfo> GitIntegration::getTags() const {
  QList<GitTagInfo> result;
  if (!m_isValid)
    return result;

  bool success;
  QString output = executeGitCommand(
      {"tag",
       "--format=%(refname:short)%09%(objectname:short)%09%(*objectname:short)"
       "%09%(objecttype)"},
      &success);

  if (!success)
    return result;

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);

  for (const QString &line : lines) {
    QStringList parts = line.split('\t');
    if (parts.size() < 4)
      continue;

    GitTagInfo info;
    info.name = parts.value(0).trimmed();
    QString objectHash = parts.value(1).trimmed();
    QString derefHash = parts.value(2).trimmed();
    QString objectType = parts.value(3).trimmed();

    info.isAnnotated = (objectType == "tag");
    info.hash =
        info.isAnnotated && !derefHash.isEmpty() ? derefHash : objectHash;

    result.append(info);
  }

  return result;
}

bool GitIntegration::renameBranch(const QString &oldName,
                                  const QString &newName) {
  if (!m_isValid)
    return false;

  bool success;
  QString output =
      executeGitCommand({"branch", "-m", oldName, newName}, &success);

  if (success) {
    emit operationCompleted(
        QString("Renamed branch '%1' to '%2'").arg(oldName, newName));
  } else {
    emit errorOccurred(
        QString("Failed to rename branch: %1").arg(output.trimmed()));
  }

  return success;
}

bool GitIntegration::rebaseBranch(const QString &ontoBranch) {
  if (!m_isValid)
    return false;

  bool success;
  QString output = executeGitCommand({"rebase", ontoBranch}, &success);

  if (success) {
    emit operationCompleted(QString("Rebased onto '%1'").arg(ontoBranch));
  } else {
    emit errorOccurred(QString("Failed to rebase onto '%1': %2")
                           .arg(ontoBranch, output.trimmed()));
  }

  return success;
}

QList<GitReflogEntry> GitIntegration::getReflog(int count) const {
  QList<GitReflogEntry> result;
  if (!m_isValid)
    return result;

  bool success;
  QString output = executeGitCommand(
      {"reflog", QString("--format=%h%x09%gd%x09%gs%x09%s%x09%cr"),
       QString("-n"), QString::number(count)},
      &success);

  if (!success || output.isEmpty())
    return result;

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);

  for (const QString &line : lines) {
    QStringList parts = line.split('\t');
    if (parts.size() < 5)
      continue;

    GitReflogEntry entry;
    entry.shortHash = parts.value(0).trimmed();
    entry.hash = entry.shortHash;
    entry.action = parts.value(2).trimmed();
    entry.subject = parts.value(3).trimmed();
    entry.relativeDate = parts.value(4).trimmed();

    result.append(entry);
  }

  return result;
}

QStringList GitIntegration::getBackupRefs() const {
  QStringList result;
  if (!m_isValid)
    return result;

  bool success;
  QString output = executeGitCommand(
      {"for-each-ref", "--format=%(refname)", "refs/backup/"}, &success);

  if (!success || output.isEmpty())
    return result;

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    QString trimmed = line.trimmed();
    if (!trimmed.isEmpty())
      result.append(trimmed);
  }

  return result;
}

QStringList GitIntegration::getCommitRefs(const QString &hash) const {
  QStringList result;
  if (!m_isValid)
    return result;

  bool success;
  QString branchOutput = executeGitCommand(
      {"branch", "--all", "--points-at", hash, "--format=%(refname:short)"},
      &success);

  if (success && !branchOutput.isEmpty()) {
    QStringList lines = branchOutput.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
      QString trimmed = line.trimmed();
      if (!trimmed.isEmpty())
        result.append(trimmed);
    }
  }

  QString tagOutput = executeGitCommand({"tag", "--points-at", hash}, &success);

  if (success && !tagOutput.isEmpty()) {
    QStringList lines = tagOutput.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
      QString trimmed = line.trimmed();
      if (!trimmed.isEmpty())
        result.append(QString("tag: %1").arg(trimmed));
    }
  }

  return result;
}

namespace {

QString readTrimmedFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }
  return QString::fromUtf8(file.readAll()).trimmed();
}

QString sequencerProgress(const QString &dir, const QString &currentFile,
                          const QString &totalFile) {
  const QString current = readTrimmedFile(dir + "/" + currentFile);
  const QString total = readTrimmedFile(dir + "/" + totalFile);
  if (current.isEmpty() || total.isEmpty()) {
    return QString();
  }
  return QObject::tr("%1 of %2").arg(current, total);
}

GitOperation detectGitOperation(const QString &gitDir, QString *detail) {
  if (detail) {
    detail->clear();
  }
  if (gitDir.isEmpty()) {
    return GitOperation::None;
  }

  const QString rebaseMerge = gitDir + "/rebase-merge";
  if (QFileInfo::exists(rebaseMerge)) {
    if (detail) {
      *detail = sequencerProgress(rebaseMerge, "msgnum", "end");
    }
    return GitOperation::Rebase;
  }

  const QString rebaseApply = gitDir + "/rebase-apply";
  if (QFileInfo::exists(rebaseApply)) {

    const bool isAm = QFileInfo::exists(rebaseApply + "/applying");
    if (detail) {
      *detail = sequencerProgress(rebaseApply, "next", "last");
    }
    return isAm ? GitOperation::ApplyMailbox : GitOperation::Rebase;
  }

  if (QFileInfo::exists(gitDir + "/CHERRY_PICK_HEAD")) {
    return GitOperation::CherryPick;
  }
  if (QFileInfo::exists(gitDir + "/REVERT_HEAD")) {
    return GitOperation::Revert;
  }
  if (QFileInfo::exists(gitDir + "/MERGE_HEAD")) {
    return GitOperation::Merge;
  }
  if (QFileInfo::exists(gitDir + "/BISECT_LOG")) {
    return GitOperation::Bisect;
  }
  return GitOperation::None;
}

} // namespace

QString GitIntegration::gitDirPath() const {
  if (!m_isValid) {
    return QString();
  }

  bool success = false;
  const QString dir =
      executeGitCommand({"rev-parse", "--absolute-git-dir"}, &success);
  if (success && !dir.isEmpty()) {
    return dir;
  }
  return m_repositoryPath + "/.git";
}

GitRepositoryState GitIntegration::repositoryState() const {
  GitRepositoryState state;
  if (!m_isValid) {
    return state;
  }

  state.valid = true;
  state.repositoryRoot = m_repositoryPath;

  bool success = false;
  const QString status = executeGitCommand(
      {"status", "--porcelain=v2", "--branch", "--untracked-files=all"},
      &success);
  if (success) {
    parseGitStatusPorcelainV2(status, state);
  }

  if (!state.unbornBranch) {
    const QString subject =
        executeGitCommand({"log", "-1", "--format=%s"}, &success);
    if (success) {
      state.headSubject = subject;
    }
  }

  const QString stashes = executeGitCommand({"stash", "list"}, &success);
  if (success) {
    state.stashCount = stashes.split('\n', Qt::SkipEmptyParts).size();
  }

  state.operation = detectGitOperation(gitDirPath(), &state.operationDetail);

  return state;
}

QString GitIntegration::executeGitCommandWithInput(const QStringList &args,
                                                   const QByteArray &input,
                                                   bool *success,
                                                   QString *errorOutput) const {
  if (!m_isValid) {
    if (success) {
      *success = false;
    }
    return QString();
  }

  QProcess process;
  process.setWorkingDirectory(m_repositoryPath.isEmpty() ? QDir::currentPath()
                                                         : m_repositoryPath);
  process.start("git", args);
  if (!process.waitForStarted(GIT_COMMAND_TIMEOUT_MS)) {
    LOG_WARNING("Git command failed to start: git " + args.join(" "));
    if (success) {
      *success = false;
    }
    return QString();
  }

  process.write(input);
  process.closeWriteChannel();

  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
    LOG_WARNING("Git command timed out: git " + args.join(" "));
    process.kill();
    process.waitForFinished(1000);
    if (success) {
      *success = false;
    }
    return QString();
  }

  const QString error = QString::fromUtf8(process.readAllStandardError());
  if (errorOutput) {
    *errorOutput = error.trimmed();
  }
  if (success) {
    *success = process.exitCode() == 0;
  }
  if (process.exitCode() != 0) {
    LOG_DEBUG("Git command failed: git " + args.join(" ") + " - " + error);
  }

  const QString output = QString::fromUtf8(process.readAllStandardOutput());
  recordCommand(args, process.workingDirectory(), output, error,
                process.exitCode());
  return output;
}

QString GitIntegration::getWorkingVsHeadDiff(const QString &filePath) const {
  if (!m_isValid) {
    return QString();
  }

  bool success = false;
  const QString output =
      executeGitCommand({"diff", "HEAD", "--", filePath}, &success);
  return success ? output : QString();
}

bool GitIntegration::applyPatch(const QString &patch, bool cached,
                                bool reverse) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }
  if (patch.trimmed().isEmpty()) {
    return false;
  }

  QStringList args{"apply"};
  if (cached) {
    args << "--cached";
  }
  if (reverse) {
    args << "--reverse";
  }

  args << "--recount" << "--unidiff-zero" << "--whitespace=nowarn" << "-";

  bool success = false;
  QString error;
  executeGitCommandWithInput(args, patch.toUtf8(), &success, &error);

  if (!success) {
    emit errorOccurred(error.isEmpty() ? QStringLiteral("git apply failed")
                                       : error);
    return false;
  }

  emit statusChanged();
  return true;
}

QList<GitCommitInfo> GitIntegration::getLogPage(const GitLogOptions &options,
                                                int skip, int limit) const {
  if (!m_isValid || limit <= 0 || skip < 0) {
    return {};
  }

  const QString format = "%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P";

  QStringList args = {"log", "--date-order", QString("--skip=%1").arg(skip),
                      QString("--max-count=%1").arg(limit),
                      QString("--pretty=format:%1").arg(format)};

  if (options.firstParentOnly) {
    args << "--first-parent";
  }
  if (options.allRefs && options.revisionRange.isEmpty()) {

    args << "--branches" << "--tags" << "--remotes";
  }
  if (!options.revisionRange.isEmpty()) {
    args << options.revisionRange;
  }
  if (!options.pathFilter.isEmpty()) {

    args << "--" << options.pathFilter;
  }

  bool success = false;
  const QString output = executeGitCommand(args, &success);
  if (!success) {
    return {};
  }

  return parseCommitLogOutput(output);
}

void GitIntegration::getLogPageAsync(
    const GitLogOptions &options, int skip, int limit,
    const std::function<void(QList<GitCommitInfo>)> &callback) {
  QPointer<GitIntegration> self(this);
  QThreadPool::globalInstance()->start([self, options, skip, limit,
                                        callback]() {
    if (!self) {
      return;
    }
    const QList<GitCommitInfo> page = self->getLogPage(options, skip, limit);
    QMetaObject::invokeMethod(
        self, [callback, page]() { callback(page); }, Qt::QueuedConnection);
  });
}

QMap<QString, QStringList> GitIntegration::getWorktreeAnchors() const {
  QMap<QString, QStringList> anchors;
  if (!m_isValid) {
    return anchors;
  }

  bool success = false;
  const QString output =
      executeGitCommand({"worktree", "list", "--porcelain"}, &success);
  if (!success) {
    return anchors;
  }

  QString path;
  QString head;
  const auto flush = [&]() {
    if (!path.isEmpty() && !head.isEmpty() &&
        QDir(path).absolutePath() != QDir(m_repositoryPath).absolutePath()) {
      anchors[head] << QDir(path).dirName();
    }
    path.clear();
    head.clear();
  };

  const QStringList lines = output.split('\n');
  for (const QString &line : lines) {
    if (line.startsWith(QLatin1String("worktree "))) {
      flush();
      path = line.mid(9).trimmed();
    } else if (line.startsWith(QLatin1String("HEAD "))) {
      head = line.mid(5).trimmed();
    }
  }
  flush();

  return anchors;
}

QMap<QString, QStringList> GitIntegration::getStashAnchors() const {
  QMap<QString, QStringList> anchors;
  if (!m_isValid) {
    return anchors;
  }

  bool success = false;
  const QString output = executeGitCommand(
      {"reflog", "show", "--format=%gd%x00%P", "refs/stash"}, &success);
  if (!success) {
    return anchors;
  }

  const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    const QStringList parts = line.split(QChar('\0'));
    if (parts.size() < 2) {
      continue;
    }
    const QStringList parents = parts[1].split(' ', Qt::SkipEmptyParts);
    if (parents.isEmpty()) {
      continue;
    }
    anchors[parents.first()] << parts[0];
  }

  return anchors;
}

bool GitIntegration::createTag(const QString &name, const QString &commitHash,
                               const QString &message) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QStringList args{"tag"};
  if (!message.isEmpty()) {
    args << "-a" << name << "-m" << message;
  } else {
    args << name;
  }
  if (!commitHash.isEmpty()) {
    args << commitHash;
  }

  bool success = false;
  executeGitCommand(args, &success);
  if (success) {
    emit operationCompleted("Created tag: " + name);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to create tag: " + name);
  }
  return success;
}

bool GitIntegration::deleteTag(const QString &name) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success = false;
  executeGitCommand({"tag", "-d", name}, &success);
  if (success) {
    emit operationCompleted("Deleted tag: " + name);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to delete tag: " + name);
  }
  return success;
}

QString GitIntegration::getMergeBase(const QString &refA,
                                     const QString &refB) const {
  if (!m_isValid || refA.isEmpty() || refB.isEmpty()) {
    return QString();
  }

  bool success = false;
  const QString output =
      executeGitCommand({"merge-base", refA, refB}, &success);
  return success ? output.trimmed() : QString();
}

GitSyncState GitIntegration::syncState(int maxCommits) const {
  GitSyncState state;
  if (!m_isValid) {
    return state;
  }

  state.valid = true;

  bool success = false;
  const QString head =
      executeGitCommand({"rev-parse", "--abbrev-ref", "HEAD"}, &success);
  if (!success) {
    return state;
  }
  if (head == QLatin1String("HEAD")) {
    state.detachedHead = true;
    return state;
  }
  state.branch = head;

  const QString upstream = executeGitCommand(
      {"rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{upstream}"},
      &success);
  if (!success || upstream.isEmpty()) {
    return state;
  }
  state.upstream = upstream;
  state.hasUpstream = true;

  state.mergeBase = getMergeBase(QStringLiteral("HEAD"), upstream);

  state.incoming =
      getCommitLogPage(QStringLiteral("HEAD..%1").arg(upstream), 0, maxCommits);
  state.outgoing =
      getCommitLogPage(QStringLiteral("%1..HEAD").arg(upstream), 0, maxCommits);

  return state;
}

GitPullStrategy GitIntegration::configuredPullStrategy() const {
  if (!m_isValid) {
    return GitPullStrategy::Merge;
  }

  bool success = false;
  const QString rebase =
      executeGitCommand({"config", "--get", "pull.rebase"}, &success);
  const QString ff =
      executeGitCommand({"config", "--get", "pull.ff"}, &success);
  return parsePullStrategyConfig(rebase, ff);
}

bool GitIntegration::pullWithStrategy(const QString &remoteName,
                                      const QString &branchName,
                                      GitPullStrategy strategy) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QStringList args{"pull"};
  switch (strategy) {
  case GitPullStrategy::Merge:
    args << "--no-rebase";
    break;
  case GitPullStrategy::Rebase:
    args << "--rebase";
    break;
  case GitPullStrategy::FastForwardOnly:
    args << "--ff-only";
    break;
  }
  if (!remoteName.isEmpty()) {
    args << remoteName;
    if (!branchName.isEmpty()) {
      args << branchName;
    }
  }

  bool success = false;
  executeGitCommand(args, &success);

  if (success) {
    updateCurrentBranch();
    emit operationCompleted("Pulled from " + remoteName);
    emit pullCompleted(remoteName, branchName);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to pull from " + remoteName);
  }
  return success;
}

bool GitIntegration::pushWithForce(const QString &remoteName,
                                   const QString &branchName, bool setUpstream,
                                   GitPushForce force) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  QStringList args{"push"};
  switch (force) {
  case GitPushForce::None:
    break;
  case GitPushForce::WithLease:
    args << "--force-with-lease";
    break;
  case GitPushForce::Force:
    args << "--force";
    break;
  }
  if (setUpstream) {
    args << "--set-upstream";
  }
  if (!remoteName.isEmpty()) {
    args << remoteName;
    if (!branchName.isEmpty()) {
      args << branchName;
    }
  }

  bool success = false;
  executeGitCommand(args, &success);

  if (success) {
    emit operationCompleted("Pushed to " + remoteName);
    emit pushCompleted(remoteName, branchName);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to push to " + remoteName);
  }
  return success;
}

bool GitIntegration::setUpstreamBranch(const QString &remoteName,
                                       const QString &branchName) {
  if (!m_isValid || remoteName.isEmpty() || branchName.isEmpty()) {
    return false;
  }

  bool success = false;
  executeGitCommand(
      {"branch", "--set-upstream-to", remoteName + "/" + branchName}, &success);

  if (success) {
    emit operationCompleted("Upstream set to " + remoteName + "/" + branchName);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to set upstream to " + remoteName + "/" +
                       branchName);
  }
  return success;
}

bool GitIntegration::unstageAll() {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success = false;
  executeGitCommand({"reset", "--mixed", "HEAD"}, &success);
  if (success) {
    emit statusChanged();
  }
  return success;
}

QList<GitFileRevision>
GitIntegration::getFileTimeline(const QString &filePath,
                                const GitFileTimelineOptions &options, int skip,
                                int limit) const {
  if (!m_isValid || filePath.isEmpty() || limit <= 0 || skip < 0) {
    return {};
  }

  const QString format = "%x01%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P";

  QStringList base{"log", QString("--skip=%1").arg(skip),
                   QString("--max-count=%1").arg(limit),
                   QString("--pretty=format:%1").arg(format)};
  if (options.followRenames) {

    base << "--follow";
  }
  if (options.firstParentOnly) {
    base << "--first-parent";
  }
  if (options.allBranches) {
    base << "--branches" << "--tags" << "--remotes";
  }
  if (!options.author.isEmpty()) {
    base << QString("--author=%1").arg(options.author);
  }
  if (!options.since.isEmpty()) {
    base << QString("--since=%1").arg(options.since);
  }
  if (!options.until.isEmpty()) {
    base << QString("--until=%1").arg(options.until);
  }

  QStringList nameArgs = base;
  nameArgs << "--name-status" << "--" << filePath;
  QStringList statArgs = base;
  statArgs << "--numstat" << "--" << filePath;

  bool success = false;
  const QString nameStatus = executeGitCommand(nameArgs, &success);
  if (!success) {
    return {};
  }
  const QString numstat = executeGitCommand(statArgs, &success);

  QList<GitFileRevision> revisions =
      parseFileTimeline(nameStatus, success ? numstat : QString());

  if (options.hasLineRange()) {

    QSet<QString> touching;
    for (const GitCommitInfo &commit : getLineHistory(
             filePath, options.lineRangeStart, options.lineRangeEnd)) {
      touching.insert(commit.hash);
    }
    QList<GitFileRevision> filtered;
    for (const GitFileRevision &revision : revisions) {
      if (touching.contains(revision.commit.hash)) {
        filtered.append(revision);
      }
    }
    revisions = filtered;
  }

  return revisions;
}

QStringList GitIntegration::predictMergeConflicts(const QString &refA,
                                                  const QString &refB) const {
  if (!m_isValid || refA.isEmpty() || refB.isEmpty()) {
    return {};
  }

  bool success = false;
  const QString output = executeGitCommand(
      {"merge-tree", "--write-tree", "--name-only", refA, refB}, &success);
  if (success) {

    return {};
  }
  if (output.trimmed().isEmpty()) {

    return {};
  }
  return parseMergeTreeConflicts(output);
}

QList<GitCommitInfo>
GitIntegration::getUnreachableAfterDelete(const QString &branchName,
                                          int maxCount) const {
  if (!m_isValid || branchName.isEmpty()) {
    return {};
  }

  const QString format = "%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P";
  QStringList args = {"log", QString("--max-count=%1").arg(maxCount),
                      QString("--pretty=format:%1").arg(format), branchName,
                      "--not"};

  bool success = false;
  const QString refs =
      executeGitCommand({"for-each-ref", "--format=%(refname)", "refs/heads",
                         "refs/remotes", "refs/tags"},
                        &success);
  if (!success) {
    return {};
  }

  const QString selfRef = QStringLiteral("refs/heads/%1").arg(branchName);
  bool haveOther = false;
  for (const QString &ref : refs.split('\n', Qt::SkipEmptyParts)) {
    if (ref == selfRef) {
      continue;
    }
    args << ref;
    haveOther = true;
  }
  if (!haveOther) {

    args.removeAll(QStringLiteral("--not"));
  }

  const QString output = executeGitCommand(args, &success);
  if (!success) {
    return {};
  }
  return parseCommitLogOutput(output);
}

bool GitIntegration::cherryPickNoCommit(const QString &commitHash) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }

  bool success = false;
  executeGitCommand({"cherry-pick", "-n", commitHash}, &success);
  if (success) {
    emit operationCompleted("Applied " + commitHash.left(7) +
                            " without committing");
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to apply " + commitHash.left(7));
  }
  return success;
}

QList<GitCommitInfo> GitIntegration::getCommitsWithoutRef(int maxCount) const {
  if (!m_isValid) {
    return {};
  }

  const QString format = "%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P";
  bool success = false;
  const QString output =
      executeGitCommand({"log", QString("--max-count=%1").arg(maxCount),
                         QString("--pretty=format:%1").arg(format), "HEAD",
                         "--not", "--branches", "--tags", "--remotes"},
                        &success);
  if (!success) {
    return {};
  }
  return parseCommitLogOutput(output);
}

QString
GitIntegration::worktreeDirtySummary(const QString &worktreePath) const {
  if (!m_isValid || worktreePath.isEmpty()) {
    return QString();
  }

  bool success = false;
  const QString output = executeGitCommandAtPath(
      worktreePath, {"status", "--porcelain", "-uall"}, &success);
  if (!success) {
    return QString();
  }

  QStringList paths;
  for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
    const QString entry = line.trimmed();
    const int space = entry.indexOf(QLatin1Char(' '));
    if (space > 0) {
      paths << entry.mid(space + 1).trimmed();
    }
  }
  return paths.join(QStringLiteral(", "));
}

QString GitIntegration::executeGitCommandWithEnv(
    const QStringList &args, const QMap<QString, QString> &extraEnv,
    bool *success, QString *errorOutput) const {
  if (!m_isValid) {
    if (success) {
      *success = false;
    }
    return QString();
  }

  QProcess process;
  process.setWorkingDirectory(m_repositoryPath.isEmpty() ? QDir::currentPath()
                                                         : m_repositoryPath);

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  for (auto it = extraEnv.constBegin(); it != extraEnv.constEnd(); ++it) {
    env.insert(it.key(), it.value());
  }
  process.setProcessEnvironment(env);

  process.start("git", args);

  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS * 6)) {
    LOG_WARNING("Git command timed out: git " + args.join(" "));
    process.kill();
    process.waitForFinished(1000);
    if (success) {
      *success = false;
    }
    return QString();
  }

  const QString error = QString::fromUtf8(process.readAllStandardError());
  if (errorOutput) {
    *errorOutput = error.trimmed();
  }
  if (success) {
    *success = process.exitCode() == 0;
  }

  const QString output = QString::fromUtf8(process.readAllStandardOutput());
  recordCommand(args, process.workingDirectory(), output, error,
                process.exitCode());
  return output;
}

QString
GitIntegration::prepareRebaseHelpers(const QString &todoText,
                                     const QStringList &messages) const {
  const QString gitDir = gitDirPath();
  if (gitDir.isEmpty()) {
    return QString();
  }

  const QString dir = gitDir + "/lightpad-rebase";
  QDir().mkpath(dir);

  const auto writeFile = [](const QString &path, const QString &content,
                            bool executable) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return false;
    }
    file.write(content.toUtf8());
    file.close();
    if (executable) {
      file.setPermissions(file.permissions() | QFileDevice::ExeOwner |
                          QFileDevice::ExeGroup);
    }
    return true;
  };

  if (!writeFile(dir + "/todo", todoText, false)) {
    return QString();
  }

  QFile::remove(dir + "/counter");
  for (int i = 0; i < messages.size(); ++i) {
    writeFile(dir + QStringLiteral("/msg-%1").arg(i + 1), messages.at(i),
              false);
  }

  writeFile(dir + "/seq-editor.sh",
            QStringLiteral("#!/bin/sh\ncat \"%1/todo\" > \"$1\"\n").arg(dir),
            true);

  writeFile(dir + "/msg-editor.sh",
            QStringLiteral("#!/bin/sh\n"
                           "d=\"%1\"\n"
                           "n=`cat \"$d/counter\" 2>/dev/null || echo 0`\n"
                           "n=`expr $n + 1`\n"
                           "echo $n > \"$d/counter\"\n"
                           "if [ -f \"$d/msg-$n\" ]; then\n"
                           "  cat \"$d/msg-$n\" > \"$1\"\n"
                           "fi\n"
                           "exit 0\n")
                .arg(dir),
            true);

  return dir;
}

QMap<QString, QString> GitIntegration::rebaseHelperEnvironment() const {
  const QString dir = gitDirPath() + "/lightpad-rebase";
  QMap<QString, QString> env;
  if (QFileInfo::exists(dir + "/seq-editor.sh")) {
    env.insert("GIT_SEQUENCE_EDITOR", dir + "/seq-editor.sh");
    env.insert("GIT_EDITOR", dir + "/msg-editor.sh");
  }
  return env;
}

bool GitIntegration::startInteractiveRebase(const QString &onto,
                                            const GitRebasePlan &plan) {
  if (!m_isValid) {
    emit errorOccurred("Not in a git repository");
    return false;
  }
  if (plan.isEmpty()) {
    return false;
  }

  const QString dir =
      prepareRebaseHelpers(plan.todoText(), plan.pendingMessages());
  if (dir.isEmpty()) {
    emit errorOccurred("Could not prepare the rebase plan");
    return false;
  }

  bool success = false;
  QString error;
  executeGitCommandWithEnv({"rebase", "-i", onto}, rebaseHelperEnvironment(),
                           &success, &error);

  if (success) {
    updateCurrentBranch();
    emit operationCompleted("Rebase finished");
  } else if (isRebaseInProgress()) {

    emit operationCompleted("Rebase stopped — resolve and continue");
  } else {
    emit errorOccurred(error.isEmpty() ? QStringLiteral("Rebase failed")
                                       : error);
  }

  emit statusChanged();
  return success;
}

bool GitIntegration::rebaseContinue() {
  if (!m_isValid) {
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({"rebase", "--continue"}, rebaseHelperEnvironment(),
                           &success, &error);
  if (!success && !isRebaseInProgress()) {
    emit errorOccurred(error.isEmpty() ? QStringLiteral("Could not continue")
                                       : error);
  }
  updateCurrentBranch();
  emit statusChanged();
  return success;
}

bool GitIntegration::rebaseSkip() {
  if (!m_isValid) {
    return false;
  }
  bool success = false;
  executeGitCommandWithEnv({"rebase", "--skip"}, rebaseHelperEnvironment(),
                           &success);
  updateCurrentBranch();
  emit statusChanged();
  return success;
}

bool GitIntegration::rebaseAbort() {
  if (!m_isValid) {
    return false;
  }
  bool success = false;
  executeGitCommand({"rebase", "--abort"}, &success);
  updateCurrentBranch();
  emit statusChanged();
  return success;
}

bool GitIntegration::isRebaseInProgress() const {
  const QString gitDir = gitDirPath();
  if (gitDir.isEmpty()) {
    return false;
  }
  return QFileInfo::exists(gitDir + "/rebase-merge") ||
         QFileInfo::exists(gitDir + "/rebase-apply");
}

bool GitIntegration::rebaseProgress(int &current, int &total) const {
  current = 0;
  total = 0;

  const QString gitDir = gitDirPath();
  if (gitDir.isEmpty()) {
    return false;
  }

  const auto readInt = [](const QString &path, int *value) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return false;
    }
    bool ok = false;
    const int parsed = QString::fromUtf8(file.readAll()).trimmed().toInt(&ok);
    if (ok) {
      *value = parsed;
    }
    return ok;
  };

  const QString merge = gitDir + "/rebase-merge";
  if (QFileInfo::exists(merge)) {
    return readInt(merge + "/msgnum", &current) &&
           readInt(merge + "/end", &total);
  }
  const QString apply = gitDir + "/rebase-apply";
  if (QFileInfo::exists(apply)) {
    return readInt(apply + "/next", &current) &&
           readInt(apply + "/last", &total);
  }
  return false;
}

QList<QPair<QString, QString>> GitIntegration::unmergedEntries() const {
  if (!m_isValid) {
    return {};
  }

  bool success = false;
  const QString output = executeGitCommand(
      {"status", "--porcelain=v2", "--untracked-files=no"}, &success);
  if (!success) {
    return {};
  }
  return parseUnmergedEntries(output);
}

QString GitIntegration::stageContent(const QString &filePath, int stage) const {
  if (!m_isValid || filePath.isEmpty() || stage < 1 || stage > 3) {
    return QString();
  }

  bool success = false;
  const QString content = executeGitCommand(
      {"show", QStringLiteral(":%1:%2").arg(stage).arg(filePath)}, &success);

  return success ? content : QString();
}

QString GitIntegration::workingFileContent(const QString &filePath) const {
  if (!m_isValid || filePath.isEmpty()) {
    return QString();
  }

  QFile file(QDir(m_repositoryPath).filePath(filePath));
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }
  return QString::fromUtf8(file.readAll());
}

GitConflictIdentities GitIntegration::conflictIdentities() const {
  GitConflictIdentities identities;
  if (!m_isValid) {
    return identities;
  }

  const QString gitDir = gitDirPath();
  const auto readRef = [&](const QString &relative) {
    QFile file(gitDir + "/" + relative);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return QString();
    }
    return QString::fromUtf8(file.readAll()).trimmed();
  };
  const auto describe = [&](const QString &hash, QString *author,
                            QString *subject) {
    if (hash.isEmpty()) {
      return;
    }
    const GitCommitInfo info = getCommitDetails(hash);
    *author = info.author;
    *subject = info.subject;
  };

  QString oursRef = QStringLiteral("HEAD");
  QString theirsRef;

  if (QFileInfo::exists(gitDir + "/rebase-merge")) {

    const QString onto = readRef("rebase-merge/onto");
    const QString headName = readRef("rebase-merge/head-name");
    identities.oursLabel =
        headName.isEmpty() ? tr("the branch being replayed onto")
                           : tr("%1 (replaying onto)")
                                 .arg(headName.section(QLatin1Char('/'), 2));
    oursRef = onto.isEmpty() ? QStringLiteral("HEAD") : onto;
    theirsRef = readRef("REBASE_HEAD");
    identities.theirsLabel = tr("the commit being replayed");
  } else if (QFileInfo::exists(gitDir + "/CHERRY_PICK_HEAD")) {
    theirsRef = readRef("CHERRY_PICK_HEAD");
    identities.oursLabel = tr("%1 (current branch)").arg(m_currentBranch);
    identities.theirsLabel = tr("the commit being copied");
  } else if (QFileInfo::exists(gitDir + "/REVERT_HEAD")) {
    theirsRef = readRef("REVERT_HEAD");
    identities.oursLabel = tr("%1 (current branch)").arg(m_currentBranch);
    identities.theirsLabel = tr("the commit being undone");
  } else {
    theirsRef = readRef("MERGE_HEAD");
    identities.oursLabel = tr("%1 (current branch)").arg(m_currentBranch);
    identities.theirsLabel = tr("the branch being merged in");
  }

  identities.oursHash = getCommitDetails(oursRef).hash;
  identities.theirsHash =
      theirsRef.isEmpty() ? QString() : getCommitDetails(theirsRef).hash;
  describe(identities.oursHash, &identities.oursAuthor,
           &identities.oursSubject);
  describe(identities.theirsHash, &identities.theirsAuthor,
           &identities.theirsSubject);

  if (!identities.oursHash.isEmpty() && !identities.theirsHash.isEmpty()) {
    identities.mergeBase =
        getMergeBase(identities.oursHash, identities.theirsHash);
    if (!identities.mergeBase.isEmpty()) {
      identities.mergeBaseSubject =
          getCommitDetails(identities.mergeBase).subject;
    }
  }

  if (!identities.theirsLabel.isEmpty() && !identities.theirsHash.isEmpty()) {
    const QStringList refs = getCommitRefs(identities.theirsHash);
    if (!refs.isEmpty()) {
      identities.theirsLabel =
          tr("%1 (%2)").arg(refs.first(), identities.theirsLabel);
    }
  }

  return identities;
}

bool GitIntegration::resolveConflictWith(const QString &filePath,
                                         const QString &content) {
  if (!m_isValid || filePath.isEmpty()) {
    return false;
  }

  QFile file(QDir(m_repositoryPath).filePath(filePath));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    emit errorOccurred("Could not write " + filePath);
    return false;
  }
  file.write(content.toUtf8());
  file.close();

  return markConflictResolved(filePath);
}

QList<GitCommitInfo> GitIntegration::getFileLogRange(const QString &filePath,
                                                     const QString &range,
                                                     int maxCount) const {
  if (!m_isValid || filePath.isEmpty() || range.isEmpty()) {
    return {};
  }

  const QString format = "%H%x00%h%x00%an%x00%ae%x00%aI%x00%ar%x00%s%x00%P";
  bool success = false;
  const QString output = executeGitCommand(
      {"log", QString("--max-count=%1").arg(maxCount),
       QString("--pretty=format:%1").arg(format), range, "--", filePath},
      &success);
  if (!success) {
    return {};
  }
  return parseCommitLogOutput(output);
}

QString GitIntegration::reflogRaw(int count) const {
  if (!m_isValid) {
    return QString();
  }

  bool success = false;
  const QString output =
      executeGitCommand({"reflog", QString("--max-count=%1").arg(count),
                         "--format=%gd%x00%gs%x00%H%x00%s%x00%ar"},
                        &success);
  return success ? output : QString();
}

bool GitIntegration::isCommitReachable(const QString &commitHash) const {
  if (!m_isValid || commitHash.isEmpty()) {
    return false;
  }

  bool success = false;
  const QString branches = executeGitCommand(
      {"branch", "--all", "--contains", commitHash}, &success);
  if (success && !branches.trimmed().isEmpty()) {
    return true;
  }

  const QString tags =
      executeGitCommand({"tag", "--contains", commitHash}, &success);
  return success && !tags.trimmed().isEmpty();
}

QList<GitStashDetail> GitIntegration::stashDetails() const {
  QList<GitStashDetail> details;
  if (!m_isValid) {
    return details;
  }

  bool success = false;
  const QString output = executeGitCommand(
      {"reflog", "show", "--format=%gd%x00%gs%x00%H%x00%P%x00%ar",
       "refs/stash"},
      &success);
  if (!success) {
    return details;
  }

  int index = 0;
  for (const QString &line :
       output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
    const QStringList fields = line.split(QChar(0));
    if (fields.size() < 5) {
      continue;
    }

    GitStashDetail detail;
    detail.index = index++;
    detail.commitHash = fields.at(2);
    detail.relativeDate = fields.at(4);

    const QStringList parents =
        fields.at(3).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    detail.parentCount = parents.size();
    if (!parents.isEmpty()) {
      detail.baseHash = parents.first();
      detail.baseSubject = getCommitDetails(detail.baseHash).subject;
    }

    const QString subject = fields.at(1);
    const int colon = subject.indexOf(QLatin1Char(':'));
    if (colon > 0) {
      QString prefix = subject.left(colon).trimmed();
      detail.message = subject.mid(colon + 1).trimmed();
      if (prefix.startsWith(QLatin1String("WIP on "), Qt::CaseInsensitive)) {
        prefix = prefix.mid(7);
      } else if (prefix.startsWith(QLatin1String("On "), Qt::CaseInsensitive)) {
        prefix = prefix.mid(3);
      }
      detail.branch = prefix.trimmed();
    } else {
      detail.message = subject;
    }

    details.append(detail);
  }

  return details;
}

QString GitIntegration::stashNumstat(int index) const {
  if (!m_isValid) {
    return QString();
  }
  bool success = false;
  const QString output = executeGitCommand(
      {"stash", "show", "--numstat", QStringLiteral("stash@{%1}").arg(index)},
      &success);
  return success ? output : QString();
}

QString GitIntegration::stashNameStatus(int index) const {
  if (!m_isValid) {
    return QString();
  }
  bool success = false;
  const QString output =
      executeGitCommand({"stash", "show", "--name-status",
                         QStringLiteral("stash@{%1}").arg(index)},
                        &success);
  return success ? output : QString();
}

QString GitIntegration::stashDiff(int index) const {
  if (!m_isValid) {
    return QString();
  }
  bool success = false;
  const QString output = executeGitCommand(
      {"stash", "show", "-p", QStringLiteral("stash@{%1}").arg(index)},
      &success);
  return success ? output : QString();
}

bool GitIntegration::stashPaths(const QStringList &paths,
                                const QString &message, bool includeUntracked) {
  if (!m_isValid || paths.isEmpty()) {
    return false;
  }

  QStringList args{"stash", "push"};
  if (includeUntracked) {
    args << "--include-untracked";
  }
  if (!message.isEmpty()) {
    args << "-m" << message;
  }
  args << "--";
  args += paths;

  bool success = false;
  executeGitCommand(args, &success);
  if (success) {
    emit operationCompleted("Stashed " + QString::number(paths.size()) +
                            " path(s)");
    emit statusChanged();
  } else {
    emit errorOccurred("Could not stash the selected paths");
  }
  return success;
}

bool GitIntegration::restoreStashPaths(int index, const QStringList &paths) {
  if (!m_isValid || paths.isEmpty()) {
    return false;
  }

  QStringList args{"checkout", QStringLiteral("stash@{%1}").arg(index), "--"};
  args += paths;

  bool success = false;
  executeGitCommand(args, &success);
  if (success) {
    emit operationCompleted("Restored " + QString::number(paths.size()) +
                            " path(s) from the stash");
    emit statusChanged();
  } else {
    emit errorOccurred("Could not restore those paths from the stash");
  }
  return success;
}

bool GitIntegration::stashBranch(const QString &branchName, int index) {
  if (!m_isValid || branchName.isEmpty()) {
    return false;
  }

  bool success = false;
  executeGitCommand(
      {"stash", "branch", branchName, QStringLiteral("stash@{%1}").arg(index)},
      &success);
  if (success) {
    updateCurrentBranch();
    emit operationCompleted("Created " + branchName + " from the stash");
    emit statusChanged();
  } else {
    emit errorOccurred("Could not create a branch from the stash");
  }
  return success;
}

namespace {

constexpr int MAX_COMMAND_HISTORY = 500;
const char *MIRROR_MODE_KEY = "git/commandMirrorMode";
} // namespace

void GitIntegration::recordCommand(const QStringList &args,
                                   const QString &workingDirectory,
                                   const QString &output, const QString &error,
                                   int exitCode) const {
  GitCommandRecord record;
  record.args = args;
  record.workingDirectory = workingDirectory;

  record.output = redactSecrets(output);
  record.error = redactSecrets(error);
  record.exitCode = exitCode;
  record.succeeded = exitCode == 0;
  record.when = QDateTime::currentDateTime();

  m_commandHistory.append(record);
  while (m_commandHistory.size() > MAX_COMMAND_HISTORY) {
    m_commandHistory.removeFirst();
  }

  GitIntegration *self = const_cast<GitIntegration *>(this);
  emit self->commandExecuted(record);
}

void GitIntegration::clearCommandHistory() { m_commandHistory.clear(); }

GitCommandMirrorMode GitIntegration::mirrorMode() {
  QSettings settings("Lightpad", "Lightpad");
  return static_cast<GitCommandMirrorMode>(
      settings
          .value(QString::fromLatin1(MIRROR_MODE_KEY),
                 static_cast<int>(GitCommandMirrorMode::Hidden))
          .toInt());
}

void GitIntegration::setMirrorMode(GitCommandMirrorMode mode) {
  QSettings settings("Lightpad", "Lightpad");
  settings.setValue(QString::fromLatin1(MIRROR_MODE_KEY),
                    static_cast<int>(mode));
}

QString GitIntegration::branchRefDetails() const {
  if (!m_isValid) {
    return QString();
  }

  bool success = false;
  const QString output = executeGitCommand(
      {"for-each-ref",
       "--format=%(refname)%00%(objectname)%00%(committerdate:relative)%00"
       "%(upstream:short)%00%(upstream:track)%00%(HEAD)",
       "refs/heads", "refs/remotes"},
      &success);
  return success ? output : QString();
}

QSet<QString> GitIntegration::mergedBranchNames(const QString &baseRef) const {
  QSet<QString> merged;
  if (!m_isValid || baseRef.isEmpty()) {
    return merged;
  }

  bool success = false;
  const QString output = executeGitCommand(
      {"branch", "--all", "--merged", baseRef, "--format=%(refname:short)"},
      &success);
  if (!success) {
    return merged;
  }

  for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
    merged.insert(line.trimmed());
  }
  return merged;
}

QMap<QString, QString> GitIntegration::branchWorktreePaths() const {
  QMap<QString, QString> paths;
  if (!m_isValid) {
    return paths;
  }

  bool success = false;
  const QString output =
      executeGitCommand({"worktree", "list", "--porcelain"}, &success);
  if (!success) {
    return paths;
  }

  QString path;
  for (const QString &line : output.split('\n')) {
    if (line.startsWith(QLatin1String("worktree "))) {
      path = line.mid(9).trimmed();
    } else if (line.startsWith(QLatin1String("branch "))) {
      const QString ref = line.mid(7).trimmed();
      if (ref.startsWith(QLatin1String("refs/heads/"))) {
        paths.insert(ref.mid(11), path);
      }
    }
  }
  return paths;
}

int GitIntegration::countCommitsNotIn(const QString &ref,
                                      const QString &baseRef) const {
  if (!m_isValid || ref.isEmpty() || baseRef.isEmpty()) {
    return 0;
  }

  bool success = false;
  const QString output = executeGitCommand(
      {"rev-list", "--count", QStringLiteral("%1..%2").arg(baseRef, ref)},
      &success);
  return success ? output.trimmed().toInt() : 0;
}

bool GitIntegration::unsetUpstream(const QString &branchName) {
  if (!m_isValid || branchName.isEmpty()) {
    return false;
  }

  bool success = false;
  executeGitCommand({"branch", "--unset-upstream", branchName}, &success);
  if (success) {
    emit operationCompleted(branchName + " no longer tracks anything");
    emit statusChanged();
  } else {
    emit errorOccurred("Could not clear the upstream of " + branchName);
  }
  return success;
}

QString GitIntegration::worktreeListRaw() const {
  if (!m_isValid) {
    return QString();
  }
  bool success = false;
  const QString output =
      executeGitCommand({"worktree", "list", "--porcelain"}, &success);
  return success ? output : QString();
}

GitRepositoryState
GitIntegration::worktreeState(const QString &worktreePath) const {
  GitRepositoryState state;
  if (!m_isValid || worktreePath.isEmpty()) {
    return state;
  }

  bool success = false;
  const QString status = executeGitCommandAtPath(
      worktreePath,
      {"status", "--porcelain=v2", "--branch", "--untracked-files=all"},
      &success);
  if (!success) {
    return state;
  }

  state.valid = true;
  state.repositoryRoot = worktreePath;
  parseGitStatusPorcelainV2(status, state);

  const QString subject = executeGitCommandAtPath(
      worktreePath, {"log", "-1", "--format=%s"}, &success);
  if (success) {
    state.headSubject = subject;
  }
  return state;
}

bool GitIntegration::pruneWorktrees() {
  if (!m_isValid) {
    return false;
  }
  bool success = false;
  executeGitCommand({"worktree", "prune"}, &success);
  if (success) {
    emit operationCompleted("Pruned stale worktree entries");
    emit statusChanged();
  }
  return success;
}

bool GitIntegration::isBisecting() const {
  const QString gitDir = gitDirPath();
  return !gitDir.isEmpty() && QFileInfo::exists(gitDir + "/BISECT_LOG");
}

QString GitIntegration::bisectStart(const QString &badRef,
                                    const QString &goodRef) {
  if (!m_isValid || badRef.isEmpty() || goodRef.isEmpty()) {
    return QString();
  }

  bool success = false;

  const QString output =
      executeGitCommand({"bisect", "start", badRef, goodRef}, &success);
  if (success) {
    emit operationCompleted("Bisect started");
  } else {
    emit errorOccurred("Could not start the bisect");
  }
  emit statusChanged();
  return output;
}

QString GitIntegration::bisectMark(const QString &verdict) {
  if (!m_isValid || verdict.isEmpty()) {
    return QString();
  }

  bool success = false;
  const QString output = executeGitCommand({"bisect", verdict}, &success);
  updateCurrentBranch();
  emit statusChanged();
  return output;
}

QString GitIntegration::bisectLog() const {
  if (!m_isValid) {
    return QString();
  }
  bool success = false;
  const QString output = executeGitCommand({"bisect", "log"}, &success);
  return success ? output : QString();
}

bool GitIntegration::bisectReset() {
  if (!m_isValid) {
    return false;
  }
  bool success = false;
  executeGitCommand({"bisect", "reset"}, &success);
  updateCurrentBranch();
  emit statusChanged();
  return success;
}

QString GitIntegration::bisectRun(const QString &command, int *exitCode) {
  if (!m_isValid || command.trimmed().isEmpty()) {
    return QString();
  }

  QProcess process;
  process.setWorkingDirectory(m_repositoryPath);
  process.setProcessChannelMode(QProcess::MergedChannels);

  process.start("git", {"bisect", "run", "sh", "-c", command});

  if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS * 120)) {
    process.kill();
    process.waitForFinished(1000);
    emit errorOccurred("The bisect run took too long and was stopped");
    if (exitCode) {
      *exitCode = -1;
    }
    return QString();
  }

  if (exitCode) {
    *exitCode = process.exitCode();
  }
  const QString output = QString::fromUtf8(process.readAll());
  recordCommand({"bisect", "run", "sh", "-c", command}, m_repositoryPath,
                output, QString(), process.exitCode());
  emit statusChanged();
  return output;
}

namespace {

QString sliceLines(const QString &content, int startLine, int endLine) {
  const QStringList lines = content.split(QLatin1Char('\n'));
  QStringList slice;
  for (int i = startLine; i <= endLine && i <= lines.size(); ++i) {
    slice << lines.at(i - 1);
  }
  return slice.join(QLatin1Char('\n'));
}

} // namespace

QString GitIntegration::blameCommitForLine(const QString &filePath, int line,
                                           bool detectMoves) const {
  if (!m_isValid || filePath.isEmpty() || line <= 0) {
    return QString();
  }

  QStringList args{"blame", "--porcelain", "-L",
                   QStringLiteral("%1,%1").arg(line)};
  if (detectMoves) {
    args << "-M" << "-C";
  }
  args << "--" << filePath;

  bool success = false;
  const QString output = executeGitCommand(args, &success);
  if (!success || output.isEmpty()) {
    return QString();
  }

  return output.section(QLatin1Char('\n'), 0, 0)
      .section(QLatin1Char(' '), 0, 0);
}

QString GitIntegration::blameOriginalPath(const QString &filePath,
                                          int line) const {
  if (!m_isValid || filePath.isEmpty() || line <= 0) {
    return QString();
  }

  bool success = false;
  const QString output =
      executeGitCommand({"blame", "--porcelain", "-M", "-C", "-L",
                         QStringLiteral("%1,%1").arg(line), "--", filePath},
                        &success);
  if (!success) {
    return QString();
  }

  for (const QString &outputLine : output.split(QLatin1Char('\n'))) {
    if (outputLine.startsWith(QLatin1String("filename "))) {
      return outputLine.mid(9).trimmed();
    }
  }
  return QString();
}

QString GitIntegration::fileLines(const QString &filePath, int startLine,
                                  int endLine) const {
  if (!m_isValid || filePath.isEmpty()) {
    return QString();
  }

  QFile file(QDir(m_repositoryPath).filePath(filePath));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }
  return sliceLines(QString::fromUtf8(file.readAll()), startLine, endLine);
}

QString GitIntegration::fileLinesAtRevision(const QString &filePath,
                                            const QString &revision,
                                            int startLine, int endLine) const {
  const QString content = getFileAtRevision(filePath, revision);
  if (content.isEmpty()) {
    return QString();
  }
  return sliceLines(content, startLine, endLine);
}
