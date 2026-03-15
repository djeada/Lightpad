#ifndef BUILTINDEBUGADAPTERS_H
#define BUILTINDEBUGADAPTERS_H

#include <memory>

class IDebugAdapter;

std::shared_ptr<IDebugAdapter> createPythonDebugAdapter();
std::shared_ptr<IDebugAdapter> createNodeDebugAdapter();
std::shared_ptr<IDebugAdapter> createGdbDebugAdapter();
std::shared_ptr<IDebugAdapter> createLldbDebugAdapter();
std::shared_ptr<IDebugAdapter> createGoDebugAdapter();
std::shared_ptr<IDebugAdapter> createRustDebugAdapter();

#endif
