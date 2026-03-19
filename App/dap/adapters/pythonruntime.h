#ifndef PYTHONDEBUGRUNTIME_H
#define PYTHONDEBUGRUNTIME_H

#include "../debugconfiguration.h"

#include <QString>

struct PythonInterpreterResolution {
  QString interpreter;
  QString statusMessage;
};

PythonInterpreterResolution
resolvePythonInterpreter(const DebugConfiguration *configuration = nullptr,
                         const QString &filePath = {},
                         const QString &workingDirectory = {});
QString preferredPythonInterpreterCandidate(
    const DebugConfiguration *configuration = nullptr,
    const QString &filePath = {}, const QString &workingDirectory = {});
QString globalPythonInterpreter();

#endif
