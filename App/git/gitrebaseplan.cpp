#include "gitrebaseplan.h"
#include <QObject>

QString gitRebaseActionName(GitRebaseAction action) {
  switch (action) {
  case GitRebaseAction::Pick:
    return QObject::tr("Keep");
  case GitRebaseAction::Reword:
    return QObject::tr("Reword");
  case GitRebaseAction::Edit:
    return QObject::tr("Stop to edit");
  case GitRebaseAction::Squash:
    return QObject::tr("Squash into previous");
  case GitRebaseAction::Fixup:
    return QObject::tr("Fixup into previous");
  case GitRebaseAction::Drop:
    return QObject::tr("Drop");
  }
  return QString();
}

QString gitRebaseActionKeyword(GitRebaseAction action) {
  switch (action) {
  case GitRebaseAction::Pick:
    return QStringLiteral("pick");
  case GitRebaseAction::Reword:
    return QStringLiteral("reword");
  case GitRebaseAction::Edit:
    return QStringLiteral("edit");
  case GitRebaseAction::Squash:
    return QStringLiteral("squash");
  case GitRebaseAction::Fixup:
    return QStringLiteral("fixup");
  case GitRebaseAction::Drop:
    return QStringLiteral("drop");
  }
  return QStringLiteral("pick");
}

GitRebaseAction gitRebaseActionFromKeyword(const QString &keyword) {
  const QString value = keyword.trimmed().toLower();
  if (value == QLatin1String("reword") || value == QLatin1String("r")) {
    return GitRebaseAction::Reword;
  }
  if (value == QLatin1String("edit") || value == QLatin1String("e")) {
    return GitRebaseAction::Edit;
  }
  if (value == QLatin1String("squash") || value == QLatin1String("s")) {
    return GitRebaseAction::Squash;
  }
  if (value == QLatin1String("fixup") || value == QLatin1String("f")) {
    return GitRebaseAction::Fixup;
  }
  if (value == QLatin1String("drop") || value == QLatin1String("d")) {
    return GitRebaseAction::Drop;
  }
  return GitRebaseAction::Pick;
}

QString gitRebaseActionExplanation(GitRebaseAction action) {
  switch (action) {
  case GitRebaseAction::Pick:
    return QObject::tr(
        "Replayed as it is. It still gets a new hash, because every commit in "
        "a rebase is recreated.");
  case GitRebaseAction::Reword:
    return QObject::tr(
        "Replayed with a new message. The change is identical; only the "
        "message and the hash differ.");
  case GitRebaseAction::Edit:
    return QObject::tr(
        "The rebase stops here so you can change the commit's content, then "
        "continue.");
  case GitRebaseAction::Squash:
    return QObject::tr(
        "Folded into the commit above it. Both messages are offered so you can "
        "write one for the combined commit.");
  case GitRebaseAction::Fixup:
    return QObject::tr(
        "Folded into the commit above it and its message is thrown away. Use "
        "this for a change that should never have been its own commit.");
  case GitRebaseAction::Drop:
    return QObject::tr(
        "Not replayed at all. Its change disappears from the branch — this "
        "removes work, it does not hide it.");
  }
  return QString();
}

QString autosquashTargetSubject(const QString &subject) {
  for (const QString &prefix :
       {QStringLiteral("fixup! "), QStringLiteral("squash! "),
        QStringLiteral("amend! ")}) {
    if (subject.startsWith(prefix)) {
      return subject.mid(prefix.size()).trimmed();
    }
  }
  return QString();
}

void GitRebasePlan::setEntries(const QList<GitRebaseEntry> &entries) {
  m_entries = entries;
  for (GitRebaseEntry &entry : m_entries) {
    const QString target = autosquashTargetSubject(entry.commit.subject);
    entry.isAutosquash = !target.isEmpty();
    entry.autosquashFor = target;
  }
}

bool GitRebasePlan::moveUp(int index) {
  if (index <= 0 || index >= m_entries.size()) {
    return false;
  }
  m_entries.swapItemsAt(index, index - 1);
  return true;
}

bool GitRebasePlan::moveDown(int index) {
  if (index < 0 || index + 1 >= m_entries.size()) {
    return false;
  }
  m_entries.swapItemsAt(index, index + 1);
  return true;
}

void GitRebasePlan::setAction(int index, GitRebaseAction action) {
  if (index >= 0 && index < m_entries.size()) {
    m_entries[index].action = action;
  }
}

void GitRebasePlan::setMessage(int index, const QString &message) {
  if (index >= 0 && index < m_entries.size()) {
    m_entries[index].newMessage = message;
  }
}

int GitRebasePlan::applyAutosquash() {

  QList<GitRebaseEntry> pending;
  QList<GitRebaseEntry> kept;
  for (const GitRebaseEntry &entry : m_entries) {
    bool hasTarget = false;
    if (entry.isAutosquash && !entry.autosquashFor.isEmpty()) {
      for (const GitRebaseEntry &candidate : m_entries) {
        if (&candidate != &entry &&
            candidate.commit.subject == entry.autosquashFor) {
          hasTarget = true;
          break;
        }
      }
    }
    if (hasTarget) {
      pending.append(entry);
    } else {
      kept.append(entry);
    }
  }

  m_entries = kept;
  int moved = 0;

  for (GitRebaseEntry entry : pending) {
    int destination = -1;
    for (int j = 0; j < m_entries.size(); ++j) {
      if (m_entries.at(j).commit.subject == entry.autosquashFor) {
        destination = j;
        break;
      }
    }
    if (destination < 0) {
      m_entries.append(entry);
      continue;
    }

    int insertAt = destination + 1;
    while (insertAt < m_entries.size() && m_entries.at(insertAt).isAutosquash) {
      ++insertAt;
    }

    entry.action = entry.commit.subject.startsWith(QStringLiteral("squash! "))
                       ? GitRebaseAction::Squash
                       : GitRebaseAction::Fixup;
    m_entries.insert(insertAt, entry);
    ++moved;
  }

  return moved;
}

QString GitRebasePlan::todoText() const {
  QStringList lines;
  for (const GitRebaseEntry &entry : m_entries) {
    if (entry.action == GitRebaseAction::Drop) {

      continue;
    }
    lines << QStringLiteral("%1 %2 %3")
                 .arg(gitRebaseActionKeyword(entry.action),
                      entry.commit.hash.isEmpty() ? entry.commit.shortHash
                                                  : entry.commit.hash,
                      entry.commit.subject);
  }
  return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QStringList GitRebasePlan::pendingMessages() const {
  QStringList messages;
  for (int i = 0; i < m_entries.size(); ++i) {
    const GitRebaseEntry &entry = m_entries.at(i);
    if (entry.action == GitRebaseAction::Reword) {
      messages << (entry.newMessage.isEmpty() ? entry.commit.subject
                                              : entry.newMessage);
      continue;
    }
    if (entry.action != GitRebaseAction::Squash) {
      continue;
    }

    const bool lastOfRun =
        i + 1 >= m_entries.size() ||
        (m_entries.at(i + 1).action != GitRebaseAction::Squash &&
         m_entries.at(i + 1).action != GitRebaseAction::Fixup);
    if (!lastOfRun) {
      continue;
    }
    QString message = entry.newMessage;
    if (message.isEmpty()) {

      for (int j = i - 1; j >= 0; --j) {
        if (m_entries.at(j).action == GitRebaseAction::Pick ||
            m_entries.at(j).action == GitRebaseAction::Reword ||
            m_entries.at(j).action == GitRebaseAction::Edit) {
          message = m_entries.at(j).newMessage.isEmpty()
                        ? m_entries.at(j).commit.subject
                        : m_entries.at(j).newMessage;
          break;
        }
      }
    }
    messages << message;
  }
  return messages;
}

QStringList GitRebasePlan::validationProblems() const {
  QStringList problems;
  if (m_entries.isEmpty()) {
    return problems;
  }

  bool sawKeeper = false;
  for (int i = 0; i < m_entries.size(); ++i) {
    const GitRebaseEntry &entry = m_entries.at(i);
    const bool folds = entry.action == GitRebaseAction::Squash ||
                       entry.action == GitRebaseAction::Fixup;
    if (folds && !sawKeeper) {
      problems << QObject::tr(
                      "%1 is set to fold into the commit above it, but there "
                      "is no commit above it to fold into.")
                      .arg(entry.commit.shortHash);
    }
    if (!folds && entry.action != GitRebaseAction::Drop) {
      sawKeeper = true;
    }
  }

  bool anyKept = false;
  for (const GitRebaseEntry &entry : m_entries) {
    if (entry.action != GitRebaseAction::Drop) {
      anyKept = true;
      break;
    }
  }
  if (!anyKept) {
    problems << QObject::tr(
        "Every commit is dropped, which would leave the branch with nothing "
        "of its own.");
  }

  return problems;
}

QList<GitCommitInfo> GitRebasePlan::resultingCommits() const {
  QList<GitCommitInfo> result;
  for (const GitRebaseEntry &entry : m_entries) {
    switch (entry.action) {
    case GitRebaseAction::Drop:
      continue;
    case GitRebaseAction::Squash:
    case GitRebaseAction::Fixup:

      continue;
    case GitRebaseAction::Pick:
    case GitRebaseAction::Edit:
    case GitRebaseAction::Reword: {
      GitCommitInfo commit = entry.commit;

      commit.shortHash = QObject::tr("new");
      commit.hash.clear();
      if (entry.action == GitRebaseAction::Reword &&
          !entry.newMessage.isEmpty()) {
        commit.subject = entry.newMessage;
      }
      result.append(commit);
      break;
    }
    }
  }
  return result;
}

bool GitRebasePlan::touchesPublishedHistory() const {
  for (const GitRebaseEntry &entry : m_entries) {
    if (entry.published) {
      return true;
    }
  }
  return false;
}
