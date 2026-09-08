#ifndef COMPILERDIAGNOSTICPARSER_H
#define COMPILERDIAGNOSTICPARSER_H

#include "../lsp/lspclient.h"

#include <QList>
#include <QMap>
#include <QString>

struct CompilerDiagnostic {
  QString filePath;
  LspDiagnostic diagnostic;
};

class CompilerDiagnosticParser {
public:
  static QList<CompilerDiagnostic> parse(const QString &output,
                                         const QString &workingDirectory);

  static QMap<QString, QList<LspDiagnostic>>
  parseByFile(const QString &output, const QString &workingDirectory);
};

#endif
