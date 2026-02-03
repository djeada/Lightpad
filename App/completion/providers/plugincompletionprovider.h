#ifndef PLUGINCOMPLETIONPROVIDER_H
#define PLUGINCOMPLETIONPROVIDER_H

#include "../icompletionprovider.h"
#include <QMap>

/**
 * @brief Completion provider using ISyntaxPlugin::keywords()
 *
 * Queries registered syntax plugins for their keywords and
 * provides them as completions.
 */
class PluginCompletionProvider : public ICompletionProvider {
public:
  PluginCompletionProvider();

  QString id() const override { return "plugins"; }
  QString displayName() const override { return "Plugin Keywords"; }
  int basePriority() const override { return 80; }
  QStringList supportedLanguages() const override;

  void requestCompletions(
      const CompletionContext &context,
      std::function<void(const QList<CompletionItem> &)> callback) override;

  bool isEnabled() const override { return m_enabled; }
  void setEnabled(bool enabled) override { m_enabled = enabled; }

  /**
   * @brief Refresh the cached keywords from plugins
   *
   * Call this after plugins are registered/unregistered.
   */
  void refreshCache();

private:
  void ensureCachePopulated();

  bool m_enabled = true;
  bool m_cachePopulated = false;
  QMap<QString, QStringList> m_cachedKeywords; // languageId -> keywords
};

#endif // PLUGINCOMPLETIONPROVIDER_H
