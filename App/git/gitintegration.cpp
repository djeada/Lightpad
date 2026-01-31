#include "gitintegration.h"
#include "../core/logging/logger.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

GitIntegration::GitIntegration(QObject* parent)
    : QObject(parent)
    , m_isValid(false)
{
}

GitIntegration::~GitIntegration()
{
}

bool GitIntegration::setRepositoryPath(const QString& path)
{
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

QString GitIntegration::repositoryPath() const
{
    return m_repositoryPath;
}

bool GitIntegration::isValidRepository() const
{
    return m_isValid;
}

QString GitIntegration::currentBranch() const
{
    return m_currentBranch;
}

QString GitIntegration::findRepositoryRoot(const QString& path) const
{
    QDir dir(path);
    
    // If path is a file, start from its directory
    QFileInfo info(path);
    if (info.isFile()) {
        dir = info.dir();
    }
    
    // Walk up the directory tree looking for .git
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

QString GitIntegration::executeGitCommand(const QStringList& args, bool* success) const
{
    if (!m_isValid && !args.contains("rev-parse")) {
        if (success) *success = false;
        return QString();
    }
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath.isEmpty() ? QDir::currentPath() : m_repositoryPath);
    process.start("git", args);
    
    if (!process.waitForFinished(GIT_COMMAND_TIMEOUT_MS)) {
        LOG_WARNING("Git command timed out: git " + args.join(" "));
        if (success) *success = false;
        return QString();
    }
    
    if (success) {
        *success = (process.exitCode() == 0);
    }
    
    if (process.exitCode() != 0) {
        QString error = process.readAllStandardError();
        LOG_DEBUG("Git command failed: git " + args.join(" ") + " - " + error);
        return QString();
    }
    
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

void GitIntegration::updateCurrentBranch()
{
    bool success;
    QString branch = executeGitCommand({"rev-parse", "--abbrev-ref", "HEAD"}, &success);
    
    if (success) {
        if (m_currentBranch != branch) {
            m_currentBranch = branch;
            emit branchChanged(m_currentBranch);
        }
    }
}

QList<GitFileInfo> GitIntegration::getStatus() const
{
    bool success;
    QString output = executeGitCommand({"status", "--porcelain", "-uall"}, &success);
    
    if (!success) {
        return QList<GitFileInfo>();
    }
    
    return parseStatusOutput(output);
}

GitFileInfo GitIntegration::getFileStatus(const QString& filePath) const
{
    GitFileInfo info;
    info.filePath = filePath;
    info.indexStatus = GitFileStatus::Clean;
    info.workTreeStatus = GitFileStatus::Clean;
    
    if (!m_isValid) {
        return info;
    }
    
    // Get relative path from repository root
    QString relativePath = filePath;
    if (filePath.startsWith(m_repositoryPath)) {
        relativePath = filePath.mid(m_repositoryPath.length() + 1);
    }
    
    bool success;
    QString output = executeGitCommand({"status", "--porcelain", "-uall", "--", relativePath}, &success);
    
    if (!success || output.isEmpty()) {
        return info;
    }
    
    QList<GitFileInfo> parsed = parseStatusOutput(output);
    if (!parsed.isEmpty()) {
        return parsed.first();
    }
    
    return info;
}

QList<GitFileInfo> GitIntegration::parseStatusOutput(const QString& output) const
{
    QList<GitFileInfo> result;
    
    if (output.isEmpty()) {
        return result;
    }
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString& line : lines) {
        if (line.length() < 4) {
            continue;
        }
        
        GitFileInfo info;
        
        QChar indexChar = line[0];
        QChar workTreeChar = line[1];
        QString path = line.mid(3);
        
        // Handle renamed files (format: "R  old -> new")
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

GitFileStatus GitIntegration::parseStatusChar(QChar c) const
{
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

QList<GitDiffLineInfo> GitIntegration::getDiffLines(const QString& filePath) const
{
    QList<GitDiffLineInfo> result;
    
    if (!m_isValid) {
        return result;
    }
    
    // Get relative path from repository root
    QString relativePath = filePath;
    if (filePath.startsWith(m_repositoryPath)) {
        relativePath = filePath.mid(m_repositoryPath.length() + 1);
    }
    
    bool success;
    // Get unified diff with 0 context lines
    QString output = executeGitCommand({"diff", "-U0", "--", relativePath}, &success);
    
    if (!success || output.isEmpty()) {
        // Also check for staged changes
        output = executeGitCommand({"diff", "-U0", "--cached", "--", relativePath}, &success);
        if (!success || output.isEmpty()) {
            return result;
        }
    }
    
    // Parse the diff output to find added/deleted lines
    // Format: @@ -oldStart,oldCount +newStart,newCount @@
    QRegularExpression hunkHeader(R"(@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@)");
    QStringList lines = output.split('\n');
    
    int currentNewLine = 0;
    bool inHunk = false;
    
    for (const QString& line : lines) {
        QRegularExpressionMatch match = hunkHeader.match(line);
        if (match.hasMatch()) {
            currentNewLine = match.captured(3).toInt();
            int oldCount = match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
            int newCount = match.captured(4).isEmpty() ? 1 : match.captured(4).toInt();
            
            // If old count is 0, it's a pure addition
            // If new count is 0, it's a pure deletion
            if (oldCount == 0 && newCount > 0) {
                for (int i = 0; i < newCount; ++i) {
                    GitDiffLineInfo info;
                    info.lineNumber = currentNewLine + i;
                    info.type = GitDiffLineInfo::Type::Added;
                    result.append(info);
                }
            } else if (newCount == 0 && oldCount > 0) {
                // Deletion - mark the line before as having a deletion
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
                // Check if this line is already marked from hunk header
                bool alreadyMarked = false;
                for (const auto& existing : result) {
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
                // Deletion, don't increment new line counter
            } else if (line[0] != '\\') {
                // Context line
                currentNewLine++;
            }
        }
    }
    
    return result;
}

QList<GitBranchInfo> GitIntegration::getBranches() const
{
    QList<GitBranchInfo> result;
    
    if (!m_isValid) {
        return result;
    }
    
    bool success;
    QString output = executeGitCommand({"branch", "-a", "--format=%(refname:short)%(HEAD)%(upstream:short)"}, &success);
    
    if (!success) {
        return result;
    }
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString& line : lines) {
        GitBranchInfo info;
        
        // Parse branch info
        // Format with HEAD marker: "branch*upstream" or "branch upstream"
        QString trimmedLine = line.trimmed();
        
        if (trimmedLine.contains('*')) {
            info.isCurrent = true;
            trimmedLine = trimmedLine.remove('*');
        } else {
            info.isCurrent = false;
        }
        
        info.isRemote = trimmedLine.startsWith("remotes/") || trimmedLine.startsWith("origin/");
        info.name = trimmedLine.section(' ', 0, 0);
        info.trackingBranch = trimmedLine.section(' ', 1, 1);
        info.aheadCount = 0;
        info.behindCount = 0;
        
        result.append(info);
    }
    
    return result;
}

bool GitIntegration::stageFile(const QString& filePath)
{
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

bool GitIntegration::stageAll()
{
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

bool GitIntegration::unstageFile(const QString& filePath)
{
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

bool GitIntegration::commit(const QString& message)
{
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

bool GitIntegration::checkoutBranch(const QString& branchName)
{
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

bool GitIntegration::createBranch(const QString& branchName, bool checkout)
{
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

bool GitIntegration::deleteBranch(const QString& branchName, bool force)
{
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

bool GitIntegration::mergeBranch(const QString& branchName)
{
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
        emit errorOccurred("Failed to merge branch: " + branchName);
    }
    
    return success;
}

QString GitIntegration::getFileDiff(const QString& filePath) const
{
    if (!m_isValid) {
        return QString();
    }
    
    QString relativePath = filePath;
    if (filePath.startsWith(m_repositoryPath)) {
        relativePath = filePath.mid(m_repositoryPath.length() + 1);
    }
    
    bool success;
    QString diff = executeGitCommand({"diff", "--", relativePath}, &success);
    
    if (!success || diff.isEmpty()) {
        // Try staged diff
        diff = executeGitCommand({"diff", "--cached", "--", relativePath}, &success);
    }
    
    return diff;
}

bool GitIntegration::discardChanges(const QString& filePath)
{
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

void GitIntegration::refresh()
{
    if (!m_isValid) {
        return;
    }
    
    updateCurrentBranch();
    emit statusChanged();
}

// ==================== Remote Operations ====================

bool GitIntegration::fetch(const QString& remote)
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    bool success;
    executeGitCommand({"fetch", remote}, &success);
    
    if (success) {
        emit operationCompleted("Fetched from: " + remote);
        emit statusChanged();
    } else {
        emit errorOccurred("Failed to fetch from: " + remote);
    }
    
    return success;
}

bool GitIntegration::pull(const QString& remote, const QString& branch)
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    bool success;
    QStringList args = {"pull", remote};
    if (!branch.isEmpty()) {
        args << branch;
    }
    
    executeGitCommand(args, &success);
    
    if (success) {
        emit operationCompleted("Pulled from: " + remote);
        updateCurrentBranch();
        emit statusChanged();
    } else {
        emit errorOccurred("Failed to pull from: " + remote);
    }
    
    return success;
}

bool GitIntegration::push(const QString& remote, const QString& branch, bool setUpstream)
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    bool success;
    QStringList args = {"push"};
    
    if (setUpstream) {
        args << "-u";
    }
    
    args << remote;
    
    if (!branch.isEmpty()) {
        args << branch;
    }
    
    executeGitCommand(args, &success);
    
    if (success) {
        emit operationCompleted("Pushed to: " + remote);
        emit statusChanged();
    } else {
        emit errorOccurred("Failed to push to: " + remote);
    }
    
    return success;
}

QStringList GitIntegration::getRemotes() const
{
    if (!m_isValid) {
        return QStringList();
    }
    
    bool success;
    QString output = executeGitCommand({"remote"}, &success);
    
    if (!success) {
        return QStringList();
    }
    
    return output.split('\n', Qt::SkipEmptyParts);
}

// ==================== Stash Operations ====================

bool GitIntegration::stash(const QString& message, bool includeUntracked)
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    bool success;
    QStringList args = {"stash", "push"};
    
    if (includeUntracked) {
        args << "-u";
    }
    
    if (!message.isEmpty()) {
        args << "-m" << message;
    }
    
    executeGitCommand(args, &success);
    
    if (success) {
        emit operationCompleted("Changes stashed");
        emit statusChanged();
    } else {
        emit errorOccurred("Failed to stash changes");
    }
    
    return success;
}

bool GitIntegration::stashPop(int index)
{
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
    } else {
        emit errorOccurred("Failed to pop stash");
    }
    
    return success;
}

bool GitIntegration::stashApply(int index)
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    bool success;
    QString stashRef = QString("stash@{%1}").arg(index);
    executeGitCommand({"stash", "apply", stashRef}, &success);
    
    if (success) {
        emit operationCompleted("Stash applied");
        emit statusChanged();
    } else {
        emit errorOccurred("Failed to apply stash");
    }
    
    return success;
}

bool GitIntegration::stashDrop(int index)
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    bool success;
    QString stashRef = QString("stash@{%1}").arg(index);
    executeGitCommand({"stash", "drop", stashRef}, &success);
    
    if (success) {
        emit operationCompleted("Stash dropped");
        emit statusChanged();
    } else {
        emit errorOccurred("Failed to drop stash");
    }
    
    return success;
}

QList<GitStashEntry> GitIntegration::stashList() const
{
    QList<GitStashEntry> result;
    
    if (!m_isValid) {
        return result;
    }
    
    bool success;
    QString output = executeGitCommand({"stash", "list", "--format=%gd|%s"}, &success);
    
    if (!success || output.isEmpty()) {
        return result;
    }
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString& line : lines) {
        GitStashEntry entry;
        
        // Parse format: "stash@{N}|message"
        int pipePos = line.indexOf('|');
        if (pipePos == -1) {
            continue;
        }
        
        QString stashRef = line.left(pipePos);
        entry.message = line.mid(pipePos + 1);
        
        // Extract index from stash@{N}
        QRegularExpression indexRegex(R"(stash@\{(\d+)\})");
        QRegularExpressionMatch match = indexRegex.match(stashRef);
        if (match.hasMatch()) {
            entry.index = match.captured(1).toInt();
        }
        
        result.append(entry);
    }
    
    return result;
}

bool GitIntegration::stashClear()
{
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
