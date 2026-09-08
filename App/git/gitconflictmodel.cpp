#include "gitconflictmodel.h"
#include "gitintegration.h"
#include <QObject>

int GitConflictContext::resolvedCount() const {
  int count = 0;
  for (const GitConflictFile &file : files) {
    if (file.resolved) {
      ++count;
    }
  }
  return count;
}

QString gitConflictClassName(GitConflictClass value) {
  switch (value) {
  case GitConflictClass::SameLineEdit:
    return QObject::tr("Both sides edited the same lines");
  case GitConflictClass::AddAdd:
    return QObject::tr("Both sides added this file");
  case GitConflictClass::DeleteModify:
    return QObject::tr("Deleted here, changed there");
  case GitConflictClass::ModifyDelete:
    return QObject::tr("Changed here, deleted there");
  case GitConflictClass::BothDeleted:
    return QObject::tr("Both sides deleted it");
  case GitConflictClass::AddedByOneSide:
    return QObject::tr("Added by one side only");
  case GitConflictClass::Binary:
    return QObject::tr("Binary file");
  case GitConflictClass::Unknown:
    return QObject::tr("Conflict");
  }
  return QString();
}

QString gitConflictClassExplanation(GitConflictClass value) {
  switch (value) {
  case GitConflictClass::SameLineEdit:
    return QObject::tr(
        "Both sides changed the same region relative to the merge base, so "
        "Git has no way to know which change should survive — or whether both "
        "should.");
  case GitConflictClass::AddAdd:
    return QObject::tr(
        "The file did not exist at the merge base and both sides created it "
        "independently. There is no common ancestor to compare against, so "
        "every line looks contested.");
  case GitConflictClass::DeleteModify:
    return QObject::tr(
        "You deleted this file while the other side kept changing it. Git "
        "will not guess whether the deletion or the changes should win — "
        "keeping the file means keeping their version.");
  case GitConflictClass::ModifyDelete:
    return QObject::tr(
        "You changed this file while the other side deleted it. Deleting it "
        "throws your changes away; keeping it undoes their deletion.");
  case GitConflictClass::BothDeleted:
    return QObject::tr(
        "Both sides deleted the file, but something else about it differs. "
        "Removing it is almost always right.");
  case GitConflictClass::AddedByOneSide:
    return QObject::tr(
        "Only one side has this file. Git records it as unmerged so the "
        "decision to keep or drop it is made deliberately.");
  case GitConflictClass::Binary:
    return QObject::tr(
        "Git cannot merge binary content line by line, so one whole version "
        "has to be chosen.");
  case GitConflictClass::Unknown:
    return QObject::tr("Git could not merge this file automatically.");
  }
  return QString();
}

GitConflictClass classifyConflictCode(const QString &code) {
  const QString value = code.trimmed().toUpper();
  if (value == QLatin1String("UU")) {
    return GitConflictClass::SameLineEdit;
  }
  if (value == QLatin1String("AA")) {
    return GitConflictClass::AddAdd;
  }
  if (value == QLatin1String("DU")) {
    return GitConflictClass::DeleteModify;
  }
  if (value == QLatin1String("UD")) {
    return GitConflictClass::ModifyDelete;
  }
  if (value == QLatin1String("DD")) {
    return GitConflictClass::BothDeleted;
  }
  if (value == QLatin1String("AU") || value == QLatin1String("UA")) {
    return GitConflictClass::AddedByOneSide;
  }
  return GitConflictClass::Unknown;
}

QList<QPair<QString, QString>>
parseUnmergedEntries(const QString &statusOutput) {
  QList<QPair<QString, QString>> entries;
  for (const QString &line :
       statusOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
    if (!line.startsWith(QLatin1String("u "))) {
      continue;
    }

    const QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (fields.size() < 11) {
      continue;
    }
    const QString code = fields.at(1);

    int position = 0;
    int seen = 0;
    while (position < line.size() && seen < 10) {
      while (position < line.size() && line.at(position) == QLatin1Char(' ')) {
        ++position;
      }
      while (position < line.size() && line.at(position) != QLatin1Char(' ')) {
        ++position;
      }
      ++seen;
    }
    const QString path = line.mid(position).trimmed();
    if (!path.isEmpty()) {
      entries.append({path, code});
    }
  }
  return entries;
}

QString gitConflictOursHint(GitOperation operation) {
  switch (operation) {
  case GitOperation::Rebase:
    return QObject::tr(
        "During a rebase \"ours\" is the branch you are replaying onto, not "
        "your own commits.");
  case GitOperation::CherryPick:
  case GitOperation::Revert:
    return QObject::tr(
        "\"Ours\" is the branch you are on, which is where the change is "
        "being applied.");
  default:
    return QObject::tr("\"Ours\" is the branch you are on.");
  }
}

QString gitConflictTheirsHint(GitOperation operation) {
  switch (operation) {
  case GitOperation::Rebase:
    return QObject::tr(
        "\"Theirs\" is the commit of yours currently being replayed — which "
        "is why the labels feel backwards during a rebase.");
  case GitOperation::CherryPick:
    return QObject::tr("\"Theirs\" is the commit being copied over.");
  case GitOperation::Revert:
    return QObject::tr("\"Theirs\" is the inverse of the commit being undone.");
  default:
    return QObject::tr("\"Theirs\" is the branch being merged in.");
  }
}

GitConflictContext buildGitConflictContext(GitIntegration *git) {
  GitConflictContext context;
  if (!git || !git->isValidRepository()) {
    return context;
  }

  const GitRepositoryState state = git->repositoryState();
  context.valid = true;
  context.operation = state.operation;
  context.oursHint = gitConflictOursHint(state.operation);
  context.theirsHint = gitConflictTheirsHint(state.operation);

  const GitConflictIdentities identities = git->conflictIdentities();
  context.oursLabel = identities.oursLabel;
  context.theirsLabel = identities.theirsLabel;
  context.mergeBase = identities.mergeBase;
  context.mergeBaseSubject = identities.mergeBaseSubject;

  for (const auto &entry : git->unmergedEntries()) {
    GitConflictFile file;
    file.path = entry.first;
    file.statusCode = entry.second;
    file.conflictClass = classifyConflictCode(entry.second);

    file.base.label = QObject::tr("merge base");
    file.base.content = git->stageContent(file.path, 1);
    file.base.exists = !file.base.content.isNull();

    file.ours.label = context.oursLabel;
    file.ours.commitHash = identities.oursHash;
    file.ours.shortHash = identities.oursHash.left(7);
    file.ours.author = identities.oursAuthor;
    file.ours.subject = identities.oursSubject;
    file.ours.content = git->stageContent(file.path, 2);
    file.ours.exists = !file.ours.content.isNull();

    file.theirs.label = context.theirsLabel;
    file.theirs.commitHash = identities.theirsHash;
    file.theirs.shortHash = identities.theirsHash.left(7);
    file.theirs.author = identities.theirsAuthor;
    file.theirs.subject = identities.theirsSubject;
    file.theirs.content = git->stageContent(file.path, 3);
    file.theirs.exists = !file.theirs.content.isNull();

    file.merged = git->workingFileContent(file.path);

    if (file.ours.content.contains(QChar(0)) ||
        file.theirs.content.contains(QChar(0))) {
      file.conflictClass = GitConflictClass::Binary;
    }

    context.files.append(file);
  }

  return context;
}
