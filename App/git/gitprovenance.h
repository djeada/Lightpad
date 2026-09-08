#ifndef GITPROVENANCE_H
#define GITPROVENANCE_H

#include "gitcommittypes.h"
#include <QList>
#include <QString>

class GitIntegration;

struct GitProvenanceStep {
  GitCommitInfo commit;

  QString pathAtRevision;
};

struct GitLineProvenance {
  bool valid = false;
  QString filePath;
  int startLine = 0;
  int endLine = 0;

  GitCommitInfo latest;

  QList<GitProvenanceStep> steps;

  QString previousText;
  QString currentText;

  bool followedRename = false;

  bool attributionUncertain = false;
  QString movedFromPath;
};

QString gitProvenanceChurnSummary(const GitLineProvenance &provenance);

QString gitProvenanceCaveat(const GitLineProvenance &provenance);

GitLineProvenance buildLineProvenance(GitIntegration *git,
                                      const QString &filePath, int startLine,
                                      int endLine);

#endif
