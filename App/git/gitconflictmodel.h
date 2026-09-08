#ifndef GITCONFLICTMODEL_H
#define GITCONFLICTMODEL_H

#include "gitcommittypes.h"
#include "gitrepositorystate.h"
#include <QList>
#include <QPair>
#include <QString>

class GitIntegration;

enum class GitConflictClass {
  SameLineEdit,

  AddAdd,

  DeleteModify,

  ModifyDelete,
  BothDeleted,

  AddedByOneSide,
  Binary,
  Unknown,
};

struct GitConflictSide {
  QString label;
  QString commitHash;
  QString shortHash;
  QString author;
  QString subject;

  bool exists = true;
  QString content;
};

struct GitConflictFile {
  QString path;

  QString statusCode;
  GitConflictClass conflictClass = GitConflictClass::Unknown;

  GitConflictSide base;
  GitConflictSide ours;
  GitConflictSide theirs;

  QString merged;
  bool resolved = false;
};

struct GitConflictContext {
  bool valid = false;
  GitOperation operation = GitOperation::None;

  QString oursLabel;
  QString theirsLabel;
  QString oursHint;
  QString theirsHint;

  QString mergeBase;
  QString mergeBaseSubject;

  QList<GitConflictFile> files;

  int resolvedCount() const;
  int remainingCount() const { return files.size() - resolvedCount(); }
};

QString gitConflictClassName(GitConflictClass value);
QString gitConflictClassExplanation(GitConflictClass value);

GitConflictClass classifyConflictCode(const QString &code);

QList<QPair<QString, QString>>
parseUnmergedEntries(const QString &statusOutput);

QString gitConflictOursHint(GitOperation operation);
QString gitConflictTheirsHint(GitOperation operation);

GitConflictContext buildGitConflictContext(GitIntegration *git);

#endif
