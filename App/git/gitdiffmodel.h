#ifndef GITDIFFMODEL_H
#define GITDIFFMODEL_H

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

enum class GitDiffLineType {
  Context,
  Added,
  Removed,

  NoNewline,
};

struct GitDiffLine {
  GitDiffLineType type = GitDiffLineType::Context;

  QString text;

  int oldLine = -1;
  int newLine = -1;

  bool isChange() const {
    return type == GitDiffLineType::Added || type == GitDiffLineType::Removed;
  }
};

struct GitHunk {

  QString header;

  QString section;
  int oldStart = 0;
  int oldCount = 0;
  int newStart = 0;
  int newCount = 0;
  QList<GitDiffLine> lines;

  int changedLineCount() const;
  int additions() const;
  int deletions() const;
};

struct GitDiffFile {
  QString path;

  QString oldPath;
  bool isBinary = false;
  bool isNew = false;
  bool isDeleted = false;
  bool isRenamed = false;
  bool valid = false;

  QStringList headerLines;
  QList<GitHunk> hunks;

  int additions() const;
  int deletions() const;
  bool isEmpty() const { return hunks.isEmpty(); }
};

GitDiffFile parseUnifiedDiff(const QString &diffText);

QList<GitDiffFile> parseUnifiedDiffFiles(const QString &diffText);

QString buildHunkPatch(const GitDiffFile &file, const QList<int> &hunkIndices);

QString buildLinePatch(const GitDiffFile &file, int hunkIndex,
                       const QSet<int> &selectedLineIndices, bool reverse);

QString buildLinePatchMulti(const GitDiffFile &file,
                            const QList<QPair<int, QSet<int>>> &selection,
                            bool reverse);

#endif
