#ifndef EXPRESSIONTRANSLATOR_H
#define EXPRESSIONTRANSLATOR_H

#include <QList>
#include <QString>

struct DebugEvaluateRequest {
  QString expression;
  QString context;
};

class DebugExpressionTranslator {
public:
  static QList<DebugEvaluateRequest>
  buildConsoleEvaluationPlan(const QString &userInput,
                             const QString &adapterId,
                             const QString &adapterType);

  static DebugEvaluateRequest localsFallbackRequest(const QString &adapterId,
                                                    const QString &adapterType);
};

#endif
