#ifndef LAUNCHTEMPLATECATALOG_H
#define LAUNCHTEMPLATECATALOG_H

#include "../dap/debugconfiguration.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// A ready-made launch configuration offered when the user adds one.
struct LaunchTemplate {
  QString id;
  QString name;
  QString category;
  QString description;
  // Language ids (as in LanguageCatalog) the template is suggested for.
  QStringList languages;
  // Files or folders at the workspace root that make the template a strong
  // suggestion (package.json, Cargo.toml, manage.py, ...).
  QStringList markers;
  // The launch.json entry, without its name.
  QJsonObject configuration;

  bool isValid() const { return !id.isEmpty() && !configuration.isEmpty(); }
  // True when Debug can start it ("type": "command" only runs).
  bool isDebuggable() const;
  // The configuration this template creates, named name.
  DebugConfiguration instantiate(const QString &name) const;
};

class LaunchTemplateCatalog {
public:
  static LaunchTemplateCatalog &instance();

  // Built-in templates, then the user's launch_templates.json (same schema;
  // an entry with an existing id replaces the built-in one).
  void reload();

  QList<LaunchTemplate> templates() const;
  LaunchTemplate templateById(const QString &id) const;
  QStringList categories() const;

  // Templates matching the language or the workspace, best first.
  QList<LaunchTemplate> suggestionsFor(const QString &languageId,
                                       const QString &workspaceFolder) const;

  static QString userTemplatesPath();

  // Parses a catalog document; exposed for tests.
  static QList<LaunchTemplate> parse(const QByteArray &json,
                                     QStringList *categories = nullptr);

private:
  LaunchTemplateCatalog() = default;
  void ensureLoaded() const;

  mutable bool m_loaded = false;
  mutable QList<LaunchTemplate> m_templates;
  mutable QStringList m_categories;
};

#endif
