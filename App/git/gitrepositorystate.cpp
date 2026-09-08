#include "gitrepositorystate.h"
#include <QObject>
#include <QStringList>

namespace {

QString countOf(int count, const QString &singular, const QString &plural) {
  return count == 1 ? singular : plural.arg(count);
}

QString capitalizeFirst(QString text) {
  if (!text.isEmpty()) {
    text[0] = text[0].toUpper();
  }
  return text;
}

QString changeClause(const GitRepositoryState &state) {
  QStringList parts;
  if (state.conflictedCount > 0) {
    parts << countOf(state.conflictedCount, QObject::tr("1 conflicted file"),
                     QObject::tr("%1 conflicted files"));
  }
  if (state.modifiedCount > 0) {
    parts << countOf(state.modifiedCount, QObject::tr("1 unstaged change"),
                     QObject::tr("%1 unstaged changes"));
  }
  if (state.untrackedCount > 0) {
    parts << countOf(state.untrackedCount, QObject::tr("1 untracked file"),
                     QObject::tr("%1 untracked files"));
  }
  if (state.stagedCount > 0) {
    parts << countOf(state.stagedCount, QObject::tr("1 staged change"),
                     QObject::tr("%1 staged changes"));
  }
  if (parts.isEmpty()) {
    return QObject::tr("working tree clean");
  }
  return parts.join(QObject::tr(", "));
}

QString branchClause(const GitRepositoryState &state) {
  if (state.unbornBranch) {
    return state.branch.isEmpty()
               ? QObject::tr("no commits yet")
               : QObject::tr("%1 has no commits yet").arg(state.branch);
  }
  if (state.detachedHead) {
    return state.headShortHash.isEmpty()
               ? QObject::tr("HEAD is detached")
               : QObject::tr("HEAD is detached at %1").arg(state.headShortHash);
  }
  if (state.branch.isEmpty()) {
    return QString();
  }
  if (!state.hasUpstream) {
    return QObject::tr("%1 has no upstream branch").arg(state.branch);
  }
  if (state.ahead > 0 && state.behind > 0) {
    return QObject::tr("%1 is %2 ahead and %3 behind %4")
        .arg(state.branch,
             countOf(state.ahead, QObject::tr("1 commit"),
                     QObject::tr("%1 commits")),
             QString::number(state.behind), state.upstream);
  }
  if (state.ahead > 0) {
    return QObject::tr("%1 is %2 ahead of %3")
        .arg(state.branch,
             countOf(state.ahead, QObject::tr("1 commit"),
                     QObject::tr("%1 commits")),
             state.upstream);
  }
  if (state.behind > 0) {
    return QObject::tr("%1 is %2 behind %3")
        .arg(state.branch,
             countOf(state.behind, QObject::tr("1 commit"),
                     QObject::tr("%1 commits")),
             state.upstream);
  }
  return QObject::tr("%1 is up to date with %2")
      .arg(state.branch, state.upstream);
}

} // namespace

bool GitRepositoryState::operator==(const GitRepositoryState &other) const {
  return valid == other.valid && repositoryRoot == other.repositoryRoot &&
         branch == other.branch && detachedHead == other.detachedHead &&
         unbornBranch == other.unbornBranch &&
         headShortHash == other.headShortHash &&
         headSubject == other.headSubject && upstream == other.upstream &&
         hasUpstream == other.hasUpstream && ahead == other.ahead &&
         behind == other.behind && stagedCount == other.stagedCount &&
         modifiedCount == other.modifiedCount &&
         untrackedCount == other.untrackedCount &&
         conflictedCount == other.conflictedCount &&
         stashCount == other.stashCount && operation == other.operation &&
         operationDetail == other.operationDetail;
}

QString gitOperationName(GitOperation operation) {
  switch (operation) {
  case GitOperation::None:
    return QString();
  case GitOperation::Merge:
    return QObject::tr("Merge");
  case GitOperation::Rebase:
    return QObject::tr("Rebase");
  case GitOperation::CherryPick:
    return QObject::tr("Cherry-pick");
  case GitOperation::Revert:
    return QObject::tr("Revert");
  case GitOperation::Bisect:
    return QObject::tr("Bisect");
  case GitOperation::ApplyMailbox:
    return QObject::tr("Patch apply");
  }
  return QString();
}

QString gitOperationExitHint(GitOperation operation) {
  switch (operation) {
  case GitOperation::None:
    return QString();
  case GitOperation::Merge:
    return QStringLiteral("git merge --continue / git merge --abort");
  case GitOperation::Rebase:
    return QStringLiteral("git rebase --continue / git rebase --abort");
  case GitOperation::CherryPick:
    return QStringLiteral(
        "git cherry-pick --continue / git cherry-pick --abort");
  case GitOperation::Revert:
    return QStringLiteral("git revert --continue / git revert --abort");
  case GitOperation::Bisect:
    return QStringLiteral("git bisect good|bad / git bisect reset");
  case GitOperation::ApplyMailbox:
    return QStringLiteral("git am --continue / git am --abort");
  }
  return QString();
}

QString gitRepositoryStateSummary(const GitRepositoryState &state) {
  if (!state.valid) {
    return QObject::tr("No Git repository.");
  }

  QStringList clauses;

  if (state.operation != GitOperation::None) {
    const QString name = gitOperationName(state.operation);
    clauses << (state.operationDetail.isEmpty()
                    ? QObject::tr("%1 in progress").arg(name)
                    : QObject::tr("%1 in progress (%2)")
                          .arg(name, state.operationDetail));
  }

  clauses << changeClause(state);

  const QString branch = branchClause(state);
  if (!branch.isEmpty()) {
    clauses << branch;
  }

  if (state.stashCount > 0) {
    clauses << countOf(state.stashCount, QObject::tr("1 stash saved"),
                       QObject::tr("%1 stashes saved"));
  }

  return capitalizeFirst(clauses.join(QStringLiteral("; "))) +
         QStringLiteral(".");
}

QString gitRepositoryStateOneLine(const GitRepositoryState &state) {
  if (!state.valid) {
    return QObject::tr("No Git repository");
  }

  QStringList parts;

  if (state.detachedHead) {
    parts << QObject::tr("detached %1").arg(state.headShortHash);
  } else if (!state.branch.isEmpty()) {
    QString branch = state.branch;
    if (state.hasUpstream && (state.ahead > 0 || state.behind > 0)) {
      if (state.ahead > 0) {
        branch += QStringLiteral(" ↑%1").arg(state.ahead);
      }
      if (state.behind > 0) {
        branch += QStringLiteral(" ↓%1").arg(state.behind);
      }
    }
    parts << branch;
  }

  if (state.operation != GitOperation::None) {
    parts << gitOperationName(state.operation);
  }
  if (state.conflictedCount > 0) {
    parts << QObject::tr("%1 conflicted").arg(state.conflictedCount);
  }
  if (state.stagedCount > 0) {
    parts << QObject::tr("%1 staged").arg(state.stagedCount);
  }
  if (state.workingTreeCount() > 0) {
    parts << QObject::tr("%1 changed").arg(state.workingTreeCount());
  }
  if (state.stashCount > 0) {
    parts << QObject::tr("%1 stashed").arg(state.stashCount);
  }
  if (parts.size() == 1 && state.isClean()) {
    parts << QObject::tr("clean");
  }

  return parts.join(QStringLiteral(" · "));
}

void parseGitStatusPorcelainV2(const QString &output,
                               GitRepositoryState &state) {
  const QStringList lines = output.split(QLatin1Char('\n'));
  for (const QString &line : lines) {
    if (line.isEmpty()) {
      continue;
    }

    if (line.startsWith(QLatin1String("# branch.oid "))) {
      const QString oid = line.mid(13).trimmed();
      if (oid == QLatin1String("(initial)")) {
        state.unbornBranch = true;
      } else {
        state.headShortHash = oid.left(7);
      }
      continue;
    }
    if (line.startsWith(QLatin1String("# branch.head "))) {
      const QString head = line.mid(14).trimmed();
      if (head == QLatin1String("(detached)")) {
        state.detachedHead = true;
      } else {
        state.branch = head;
      }
      continue;
    }
    if (line.startsWith(QLatin1String("# branch.upstream "))) {
      state.upstream = line.mid(18).trimmed();
      state.hasUpstream = !state.upstream.isEmpty();
      continue;
    }
    if (line.startsWith(QLatin1String("# branch.ab "))) {
      const QStringList parts =
          line.mid(12).trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
      for (const QString &part : parts) {
        if (part.startsWith(QLatin1Char('+'))) {
          state.ahead = part.mid(1).toInt();
        } else if (part.startsWith(QLatin1Char('-'))) {
          state.behind = part.mid(1).toInt();
        }
      }
      continue;
    }
    if (line.startsWith(QLatin1Char('#'))) {
      continue;
    }

    const QChar kind = line.at(0);
    if (kind == QLatin1Char('?')) {
      ++state.untrackedCount;
      continue;
    }
    if (kind == QLatin1Char('u')) {
      ++state.conflictedCount;
      continue;
    }
    if (kind != QLatin1Char('1') && kind != QLatin1Char('2')) {

      continue;
    }

    if (line.size() < 4) {
      continue;
    }
    const QChar indexStatus = line.at(2);
    const QChar workTreeStatus = line.at(3);
    if (indexStatus != QLatin1Char('.')) {
      ++state.stagedCount;
    }
    if (workTreeStatus != QLatin1Char('.')) {
      ++state.modifiedCount;
    }
  }
}
