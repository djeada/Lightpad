#include "debugadapterregistry.h"

#include "../core/logging/logger.h"
#include "adapters/builtinadapters.h"

#include <QFileInfo>

DebugAdapterRegistry &DebugAdapterRegistry::instance() {
  static DebugAdapterRegistry instance;
  return instance;
}

DebugAdapterRegistry::DebugAdapterRegistry() : QObject(nullptr) {
  registerBuiltinAdapters();
}

void DebugAdapterRegistry::registerBuiltinAdapters() {
  registerAdapter(createPythonDebugAdapter());
  registerAdapter(createNodeDebugAdapter());
  registerAdapter(createGdbDebugAdapter());
  registerAdapter(createLldbDebugAdapter());
  LOG_INFO("Registered built-in debug adapters");
}

void DebugAdapterRegistry::registerAdapter(
    std::shared_ptr<IDebugAdapter> adapterInstance) {
  if (!adapterInstance) {
    return;
  }

  const QString id = adapterInstance->config().id;
  m_adapters[id] = adapterInstance;

  LOG_INFO(QString("Registered debug adapter: %1").arg(id));
  emit adapterRegistered(id);
}

void DebugAdapterRegistry::unregisterAdapter(const QString &adapterId) {
  if (m_adapters.remove(adapterId) > 0) {
    LOG_INFO(QString("Unregistered debug adapter: %1").arg(adapterId));
    emit adapterUnregistered(adapterId);
  }
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::allAdapters() const {
  return m_adapters.values();
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::availableAdapters() const {
  QList<std::shared_ptr<IDebugAdapter>> result;
  for (const auto &adapterInstance : m_adapters) {
    if (adapterInstance->isAvailable()) {
      result.append(adapterInstance);
    }
  }
  return result;
}

std::shared_ptr<IDebugAdapter>
DebugAdapterRegistry::adapter(const QString &adapterId) const {
  return m_adapters.value(adapterId);
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::adaptersForFile(const QString &filePath) const {
  QList<std::shared_ptr<IDebugAdapter>> result;
  for (const auto &adapterInstance : m_adapters) {
    if (adapterInstance->canDebug(filePath)) {
      result.append(adapterInstance);
    }
  }
  return result;
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::adaptersForLanguage(const QString &languageId) const {
  QList<std::shared_ptr<IDebugAdapter>> result;
  for (const auto &adapterInstance : m_adapters) {
    if (adapterInstance->supportsLanguage(languageId)) {
      result.append(adapterInstance);
    }
  }
  return result;
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::adaptersForType(const QString &type) const {
  QList<std::shared_ptr<IDebugAdapter>> result;
  for (const auto &adapterInstance : m_adapters) {
    if (adapterInstance && adapterInstance->config().type.compare(
                               type, Qt::CaseInsensitive) == 0) {
      result.append(adapterInstance);
    }
  }
  return result;
}

QList<std::shared_ptr<IDebugAdapter>>
DebugAdapterRegistry::adaptersForConfiguration(
    const DebugConfiguration &configuration) const {
  QList<std::shared_ptr<IDebugAdapter>> result;

  if (!configuration.adapterId.trimmed().isEmpty()) {
    const auto selected = adapter(configuration.adapterId.trimmed());
    if (selected) {
      result.append(selected);
    }
    return result;
  }

  for (const auto &adapterInstance : m_adapters) {
    if (adapterInstance &&
        adapterInstance->matchesConfiguration(configuration)) {
      result.append(adapterInstance);
    }
  }

  if (result.isEmpty()) {
    const auto explicitAdapter = adapter(configuration.type);
    if (explicitAdapter) {
      result.append(explicitAdapter);
    }
  }

  return result;
}

std::shared_ptr<IDebugAdapter>
DebugAdapterRegistry::preferredAdapterForFile(const QString &filePath) const {
  for (const auto &adapterInstance : m_adapters) {
    if (adapterInstance->canDebug(filePath)) {
      return adapterInstance;
    }
  }
  return nullptr;
}

std::shared_ptr<IDebugAdapter>
DebugAdapterRegistry::preferredAdapterForLanguage(
    const QString &languageId) const {
  for (const auto &adapterInstance : m_adapters) {
    if (adapterInstance->supportsLanguage(languageId)) {
      return adapterInstance;
    }
  }
  return nullptr;
}

std::shared_ptr<IDebugAdapter>
DebugAdapterRegistry::preferredAdapterForConfiguration(
    const DebugConfiguration &configuration) const {
  const auto candidates = adaptersForConfiguration(configuration);
  for (const auto &adapterInstance : candidates) {
    if (adapterInstance &&
        adapterInstance->isAvailableForConfiguration(configuration)) {
      return adapterInstance;
    }
  }
  return nullptr;
}

void DebugAdapterRegistry::refreshAvailability() { emit availabilityChanged(); }
