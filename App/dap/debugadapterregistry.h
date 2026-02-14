#ifndef DEBUGADAPTERREGISTRY_H
#define DEBUGADAPTERREGISTRY_H

#include <QList>
#include <QMap>
#include <QObject>
#include <memory>

#include "idebugadapter.h"

class DebugAdapterRegistry : public QObject {
  Q_OBJECT

public:
  static DebugAdapterRegistry &instance();

  void registerAdapter(std::shared_ptr<IDebugAdapter> adapter);

  void unregisterAdapter(const QString &adapterId);

  QList<std::shared_ptr<IDebugAdapter>> allAdapters() const;

  QList<std::shared_ptr<IDebugAdapter>> availableAdapters() const;

  std::shared_ptr<IDebugAdapter> adapter(const QString &adapterId) const;

  QList<std::shared_ptr<IDebugAdapter>>
  adaptersForFile(const QString &filePath) const;

  QList<std::shared_ptr<IDebugAdapter>>
  adaptersForLanguage(const QString &languageId) const;

  QList<std::shared_ptr<IDebugAdapter>>
  adaptersForType(const QString &type) const;

  std::shared_ptr<IDebugAdapter>
  preferredAdapterForFile(const QString &filePath) const;

  std::shared_ptr<IDebugAdapter>
  preferredAdapterForLanguage(const QString &languageId) const;

  void refreshAvailability();

signals:

  void adapterRegistered(const QString &adapterId);

  void adapterUnregistered(const QString &adapterId);

  void availabilityChanged();

private:
  DebugAdapterRegistry();
  ~DebugAdapterRegistry() = default;
  DebugAdapterRegistry(const DebugAdapterRegistry &) = delete;
  DebugAdapterRegistry &operator=(const DebugAdapterRegistry &) = delete;

  void registerBuiltinAdapters();

  QMap<QString, std::shared_ptr<IDebugAdapter>> m_adapters;
};

#endif
