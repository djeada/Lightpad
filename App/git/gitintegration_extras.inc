// Remote management, sequencer control, fixup/autosquash, clean, submodules.
// Included at the end of gitintegration.cpp (member definitions of
// GitIntegration).

bool GitIntegration::renameRemote(const QString &oldName,
                                  const QString &newName) {
  if (!m_isValid || oldName.isEmpty() || newName.isEmpty()) {
    emit errorOccurred("Remote names cannot be empty");
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({"remote", "rename", oldName, newName}, {}, &success,
                           &error);
  if (success) {
    emit operationCompleted(
        QString("Remote renamed: %1 -> %2").arg(oldName, newName));
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to rename remote: " + error.trimmed());
  }
  return success;
}

bool GitIntegration::setRemoteUrl(const QString &name, const QString &url,
                                  bool pushUrl) {
  if (!m_isValid || name.isEmpty() || url.isEmpty()) {
    emit errorOccurred("Remote name and URL cannot be empty");
    return false;
  }
  QStringList args{"remote", "set-url"};
  if (pushUrl) {
    args << "--push";
  }
  args << name << url;
  bool success = false;
  QString error;
  executeGitCommandWithEnv(args, {}, &success, &error);
  if (success) {
    emit operationCompleted("Remote URL updated: " + name);
  } else {
    emit errorOccurred("Failed to update remote URL: " + error.trimmed());
  }
  return success;
}

bool GitIntegration::pruneRemote(const QString &name) {
  if (!m_isValid || name.isEmpty()) {
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({"remote", "prune", name}, {}, &success, &error);
  if (success) {
    emit operationCompleted("Pruned stale branches of " + name);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to prune remote: " + error.trimmed());
  }
  return success;
}

bool GitIntegration::pushTag(const QString &remoteName,
                             const QString &tagName) {
  if (!m_isValid || remoteName.isEmpty() || tagName.isEmpty()) {
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({"push", remoteName, "refs/tags/" + tagName}, {},
                           &success, &error);
  if (success) {
    emit operationCompleted(
        QString("Pushed tag %1 to %2").arg(tagName, remoteName));
  } else {
    emit errorOccurred("Failed to push tag: " + error.trimmed());
  }
  return success;
}

bool GitIntegration::deleteRemoteTag(const QString &remoteName,
                                     const QString &tagName) {
  if (!m_isValid || remoteName.isEmpty() || tagName.isEmpty()) {
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv(
      {"push", remoteName, "--delete", "refs/tags/" + tagName}, {}, &success,
      &error);
  if (success) {
    emit operationCompleted(
        QString("Deleted tag %1 on %2").arg(tagName, remoteName));
  } else {
    emit errorOccurred("Failed to delete remote tag: " + error.trimmed());
  }
  return success;
}

GitIntegration::SequencerKind GitIntegration::sequencerKind() const {
  const QString gitDir = gitDirPath();
  if (gitDir.isEmpty()) {
    return SequencerKind::None;
  }
  if (QFileInfo::exists(gitDir + "/CHERRY_PICK_HEAD")) {
    return SequencerKind::CherryPick;
  }
  if (QFileInfo::exists(gitDir + "/REVERT_HEAD")) {
    return SequencerKind::Revert;
  }
  QFile todo(gitDir + "/sequencer/todo");
  if (todo.open(QIODevice::ReadOnly)) {
    const QString text = QString::fromUtf8(todo.readAll()).trimmed();
    if (text.startsWith("revert") || text.startsWith("r ")) {
      return SequencerKind::Revert;
    }
    if (!text.isEmpty()) {
      return SequencerKind::CherryPick;
    }
  }
  return SequencerKind::None;
}

bool GitIntegration::cherryPickCommits(const QStringList &commitHashes) {
  if (!m_isValid || commitHashes.isEmpty()) {
    return false;
  }
  QStringList args{"cherry-pick"};
  args << commitHashes;
  bool success = false;
  QString error;
  executeGitCommandWithEnv(args, {{"GIT_EDITOR", "true"}}, &success, &error);
  if (success) {
    emit operationCompleted(
        QString("Cherry-picked %1 commit(s)").arg(commitHashes.size()));
  } else {
    emit errorOccurred("Cherry-pick failed: " + error.trimmed());
  }
  emit statusChanged();
  return success;
}

static QString sequencerVerb(GitIntegration::SequencerKind kind) {
  return kind == GitIntegration::SequencerKind::Revert
             ? QStringLiteral("revert")
             : QStringLiteral("cherry-pick");
}

bool GitIntegration::sequencerContinue() {
  const SequencerKind kind = sequencerKind();
  if (!m_isValid || kind == SequencerKind::None) {
    emit errorOccurred("No cherry-pick or revert is in progress");
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({sequencerVerb(kind), "--continue"},
                           {{"GIT_EDITOR", "true"}}, &success, &error);
  if (!success) {
    emit errorOccurred("Could not continue: " + error.trimmed());
  } else {
    emit operationCompleted("Sequence continued");
  }
  updateCurrentBranch();
  emit statusChanged();
  return success;
}

bool GitIntegration::sequencerSkip() {
  const SequencerKind kind = sequencerKind();
  if (!m_isValid || kind == SequencerKind::None) {
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({sequencerVerb(kind), "--skip"},
                           {{"GIT_EDITOR", "true"}}, &success, &error);
  if (!success) {
    emit errorOccurred("Could not skip: " + error.trimmed());
  }
  updateCurrentBranch();
  emit statusChanged();
  return success;
}

bool GitIntegration::sequencerAbort() {
  const SequencerKind kind = sequencerKind();
  if (!m_isValid || kind == SequencerKind::None) {
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({sequencerVerb(kind), "--abort"}, {}, &success,
                           &error);
  if (!success) {
    emit errorOccurred("Could not abort: " + error.trimmed());
  } else {
    emit operationCompleted("Sequence aborted");
  }
  updateCurrentBranch();
  emit statusChanged();
  return success;
}

bool GitIntegration::commitFixup(const QString &targetHash, bool squashStyle) {
  if (!m_isValid || targetHash.isEmpty()) {
    emit errorOccurred("A target commit is required");
    return false;
  }
  bool noStagedChanges = false;
  executeGitCommand({"diff", "--cached", "--quiet"}, &noStagedChanges);
  if (noStagedChanges) {
    emit errorOccurred("Stage some changes before creating a fixup commit");
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({"commit", squashStyle ? "--squash=" + targetHash
                                                  : "--fixup=" + targetHash},
                           {{"GIT_EDITOR", "true"}}, &success, &error);
  if (success) {
    emit operationCompleted(
        QString("Created fixup for %1").arg(targetHash.left(7)));
    emit statusChanged();
  } else {
    emit errorOccurred("Fixup failed: " + error.trimmed());
  }
  return success;
}

QStringList GitIntegration::pendingFixupSubjects(const QString &onto) const {
  QStringList result;
  if (!m_isValid || onto.isEmpty()) {
    return result;
  }
  bool success = false;
  const QString out =
      executeGitCommand({"log", "--format=%s", onto + "..HEAD"}, &success);
  if (!success) {
    return result;
  }
  for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
    if (line.startsWith("fixup! ") || line.startsWith("squash! ") ||
        line.startsWith("amend! ")) {
      result << line;
    }
  }
  return result;
}

bool GitIntegration::autosquashRebase(const QString &onto) {
  if (!m_isValid || onto.isEmpty()) {
    emit errorOccurred("A base revision is required");
    return false;
  }
  if (pendingFixupSubjects(onto).isEmpty()) {
    emit errorOccurred("No fixup! or squash! commits to autosquash");
    return false;
  }
  bool success = false;
  QString error;
  bool timedOut = false;
  executeGitCommandWithEnv(
      {"rebase", "-i", "--autosquash", "--autostash", onto},
      {{"GIT_SEQUENCE_EDITOR", "true"}, {"GIT_EDITOR", "true"}}, &success,
      &error, &timedOut);
  if (timedOut && isRebaseInProgress()) {
    executeGitCommand({"rebase", "--abort"}, nullptr);
    success = false;
    error += "; the rebase was aborted";
  }
  if (success) {
    emit operationCompleted("Autosquashed fixup commits");
  } else {
    emit errorOccurred("Autosquash failed: " + error.trimmed());
  }
  updateCurrentBranch();
  emit statusChanged();
  return success;
}

QStringList GitIntegration::cleanPreview(bool includeIgnored) const {
  QStringList result;
  if (!m_isValid) {
    return result;
  }
  QStringList args{"clean", "-nd"};
  if (includeIgnored) {
    args << "-x";
  }
  bool success = false;
  const QString out = executeGitCommand(args, &success);
  if (!success) {
    return result;
  }
  const QString prefix = QStringLiteral("Would remove ");
  for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
    if (line.startsWith(prefix)) {
      result << line.mid(prefix.size()).trimmed();
    }
  }
  return result;
}

bool GitIntegration::cleanPaths(const QStringList &paths,
                                bool includeIgnored) {
  if (!m_isValid || paths.isEmpty()) {
    return false;
  }
  QStringList args{"clean", "-fd"};
  if (includeIgnored) {
    args << "-x";
  }
  args << "--" << paths;
  bool success = false;
  QString error;
  executeGitCommandWithEnv(args, {}, &success, &error);
  if (success) {
    emit operationCompleted(
        QString("Removed %1 untracked item(s)").arg(paths.size()));
    emit statusChanged();
  } else {
    emit errorOccurred("Clean failed: " + error.trimmed());
  }
  return success;
}

QList<GitSubmoduleInfo> GitIntegration::submodules() const {
  if (!m_isValid) {
    return {};
  }
  bool success = false;
  const QString out =
      executeGitCommandRaw({"submodule", "status", "--recursive"}, &success);
  return success ? parseSubmoduleStatus(out) : QList<GitSubmoduleInfo>();
}

bool GitIntegration::addSubmodule(const QString &url, const QString &path) {
  if (!m_isValid || url.isEmpty() || path.isEmpty()) {
    emit errorOccurred("Submodule URL and path are required");
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({"submodule", "add", "--", url, path}, {}, &success,
                           &error);
  if (success) {
    emit operationCompleted("Submodule added: " + path);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to add submodule: " + error.trimmed());
  }
  return success;
}

bool GitIntegration::initUpdateSubmodules(const QStringList &paths,
                                          bool recursive) {
  if (!m_isValid) {
    return false;
  }
  QStringList args{"submodule", "update", "--init"};
  if (recursive) {
    args << "--recursive";
  }
  if (!paths.isEmpty()) {
    args << "--" << paths;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv(args, {}, &success, &error);
  if (success) {
    emit operationCompleted("Submodules updated");
    emit statusChanged();
  } else {
    emit errorOccurred("Submodule update failed: " + error.trimmed());
  }
  return success;
}

bool GitIntegration::syncSubmodules() {
  if (!m_isValid) {
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({"submodule", "sync", "--recursive"}, {}, &success,
                           &error);
  if (!success) {
    emit errorOccurred("Submodule sync failed: " + error.trimmed());
  }
  return success;
}

bool GitIntegration::removeSubmodule(const QString &path) {
  if (!m_isValid || path.isEmpty()) {
    return false;
  }
  bool success = false;
  QString error;
  executeGitCommandWithEnv({"submodule", "deinit", "-f", "--", path}, {},
                           &success, &error);
  if (success) {
    executeGitCommandWithEnv({"rm", "-f", "--", path}, {}, &success, &error);
  }
  if (success) {
    emit operationCompleted("Submodule removed: " + path);
    emit statusChanged();
  } else {
    emit errorOccurred("Failed to remove submodule: " + error.trimmed());
  }
  return success;
}
