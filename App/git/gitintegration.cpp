#include "gitintegration.h"
#include "../core/logging/logger.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

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

QString GitIntegration::workingPath() const
{
    return m_workingPath;
}

void GitIntegration::setWorkingPath(const QString& path)
{
    QFileInfo info(path);
    if (info.isFile()) {
        m_workingPath = info.dir().absolutePath();
    } else {
        m_workingPath = QDir(path).absolutePath();
    }
}

QString GitIntegration::executeGitCommandAtPath(const QString& path, const QStringList& args, bool* success) const
{
    QProcess process;
    process.setWorkingDirectory(path);
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
        QString error = QString::fromUtf8(process.readAllStandardError());
        LOG_DEBUG("Git command failed: git " + args.join(" ") + " - " + error);
        return error;
    }
    
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

// ========== Repository Initialization ==========

bool GitIntegration::initRepository(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        emit errorOccurred("Directory does not exist: " + path);
        return false;
    }
    
    bool success;
    executeGitCommandAtPath(path, {"init"}, &success);
    
    if (success) {
        // Set the repository path to the initialized directory
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

// ========== Remote Operations ==========

QList<GitRemoteInfo> GitIntegration::getRemotes() const
{
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
    
    for (const QString& line : lines) {
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
                // Default to both if not specified
                remoteMap[name].fetchUrl = url;
                remoteMap[name].pushUrl = url;
            }
        }
    }
    
    for (const auto& info : remoteMap) {
        result.append(info);
    }
    
    return result;
}

bool GitIntegration::addRemote(const QString& name, const QString& url)
{
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

bool GitIntegration::removeRemote(const QString& name)
{
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

bool GitIntegration::fetch(const QString& remoteName)
{
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

bool GitIntegration::pull(const QString& remoteName, const QString& branchName)
{
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
        emit statusChanged();
        
        // Check for merge conflicts after pull
        if (hasMergeConflicts()) {
            QStringList conflicts = getConflictedFiles();
            emit mergeConflictsDetected(conflicts);
        }
    } else {
        // Check if pull failed due to merge conflicts
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

bool GitIntegration::push(const QString& remoteName, const QString& branchName, bool setUpstream)
{
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
    } else {
        emit errorOccurred("Failed to push to: " + remoteName);
    }
    
    return success;
}

// ========== Merge Conflict Handling ==========

bool GitIntegration::hasMergeConflicts() const
{
    if (!m_isValid) {
        return false;
    }
    
    return !getConflictedFiles().isEmpty();
}

QStringList GitIntegration::getConflictedFiles() const
{
    QStringList result;
    
    if (!m_isValid) {
        return result;
    }
    
    bool success;
    QString output = executeGitCommand({"diff", "--name-only", "--diff-filter=U"}, &success);
    
    if (success && !output.isEmpty()) {
        result = output.split('\n', Qt::SkipEmptyParts);
    }
    
    return result;
}

QList<GitConflictMarker> GitIntegration::getConflictMarkers(const QString& filePath) const
{
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
        const QString& line = lines[i];
        
        if (line.startsWith("<<<<<<<")) {
            currentMarker = GitConflictMarker();
            currentMarker.startLine = i + 1;  // 1-indexed
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

bool GitIntegration::resolveConflictOurs(const QString& filePath)
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
    executeGitCommand({"checkout", "--ours", "--", relativePath}, &success);
    
    if (success) {
        // Stage the resolved file
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

bool GitIntegration::resolveConflictTheirs(const QString& filePath)
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
    executeGitCommand({"checkout", "--theirs", "--", relativePath}, &success);
    
    if (success) {
        // Stage the resolved file
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

bool GitIntegration::markConflictResolved(const QString& filePath)
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
        emit operationCompleted("Marked as resolved: " + relativePath);
        emit statusChanged();
    } else {
        emit errorOccurred("Failed to mark as resolved: " + relativePath);
    }
    
    return success;
}

bool GitIntegration::abortMerge()
{
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

bool GitIntegration::continueMerge()
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    // Check if there are still unresolved conflicts
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

bool GitIntegration::isMergeInProgress() const
{
    if (!m_isValid) {
        return false;
    }
    
    // Check for MERGE_HEAD file
    QFile mergeHead(m_repositoryPath + "/.git/MERGE_HEAD");
    return mergeHead.exists();
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
        // Check if merge failed due to conflicts
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

// ========== Stash Operations ==========

QList<GitStashEntry> GitIntegration::getStashList() const
{
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

QList<GitStashEntry> GitIntegration::parseStashListOutput(const QString& output) const
{
    QList<GitStashEntry> result;
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    // Format: stash@{0}: On branch_name: message
    // or: stash@{0}: WIP on branch_name: commit_hash commit_message
    QRegularExpression stashPattern(R"(stash@\{(\d+)\}: (?:On|WIP on) ([^:]+): (.+))");
    
    // Git abbreviated hash lengths: typically 7-8 chars but can be 4-40
    constexpr int MIN_HASH_LENGTH = 4;
    constexpr int MAX_ABBREV_HASH_LENGTH = 12;
    
    for (const QString& line : lines) {
        QRegularExpressionMatch match = stashPattern.match(line);
        if (match.hasMatch()) {
            GitStashEntry entry;
            entry.index = match.captured(1).toInt();
            entry.branch = match.captured(2).trimmed();
            entry.message = match.captured(3).trimmed();
            
            // Extract hash if present (format: "hash message")
            // Git hashes are hexadecimal, typically abbreviated to 7-8 chars
            QString msgPart = entry.message;
            int spaceIndex = msgPart.indexOf(' ');
            if (spaceIndex >= MIN_HASH_LENGTH && spaceIndex <= MAX_ABBREV_HASH_LENGTH) {
                QString potentialHash = msgPart.left(spaceIndex);
                // Verify it looks like a hex hash
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

bool GitIntegration::stash(const QString& message, bool includeUntracked)
{
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
        emit operationCompleted("Changes stashed" + (message.isEmpty() ? "" : ": " + message));
        emit statusChanged();
    } else {
        emit errorOccurred("Failed to stash changes");
    }
    
    return success;
}

bool GitIntegration::stashPop()
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    bool success;
    executeGitCommand({"stash", "pop"}, &success);
    
    if (success) {
        emit operationCompleted("Stash popped");
        emit statusChanged();
        
        // Check for conflicts after stash pop
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

bool GitIntegration::stashApply(int index)
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    bool success;
    executeGitCommand({"stash", "apply", QString("stash@{%1}").arg(index)}, &success);
    
    if (success) {
        emit operationCompleted(QString("Stash %1 applied").arg(index));
        emit statusChanged();
        
        // Check for conflicts after stash apply
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

bool GitIntegration::stashDrop(int index)
{
    if (!m_isValid) {
        emit errorOccurred("Not in a git repository");
        return false;
    }
    
    bool success;
    executeGitCommand({"stash", "drop", QString("stash@{%1}").arg(index)}, &success);
    
    if (success) {
        emit operationCompleted(QString("Stash %1 dropped").arg(index));
    } else {
        emit errorOccurred(QString("Failed to drop stash %1").arg(index));
    }
    
    return success;
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
    } else {
        emit errorOccurred("Failed to clear stash");
    }
    
    return success;
}
