#ifndef DBCONNECTIONTEMPLATES_H
#define DBCONNECTIONTEMPLATES_H

#include "dbtypes.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// A starting point for a new connection: local servers, Docker containers,
// managed cloud databases and SQLite files. Built-in templates live in
// database/connection_templates.json; users add or override entries (by id)
// in <AppConfigLocation>/database/connection_templates.json.
struct DbConnectionTemplate {
  QString id;
  QString name;
  QString category;
  QString description;
  // Shown next to the form: where to find the values for this service.
  QString help;
  QString docsUrl;
  QStringList keywords;
  // Example values shown as placeholders where the template has no default.
  QString hostPlaceholder;
  QString userPlaceholder;
  QString databasePlaceholder;
  DbConnectionProfile profile;

  bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
  // A new profile from this template, named `name` when given.
  DbConnectionProfile instantiate(const QString &name = QString()) const;
  QString searchText() const;
};

class DbConnectionTemplateCatalog {
public:
  static DbConnectionTemplateCatalog &instance();

  void reload();
  QList<DbConnectionTemplate> templates() const { return m_templates; }
  QStringList categories() const { return m_categories; }
  DbConnectionTemplate templateById(const QString &id) const;

  static QString userTemplatesPath();
  // Parses a catalog document; entries without an id, name or engine are
  // skipped. Categories are returned in document order.
  static QList<DbConnectionTemplate> parse(const QJsonObject &root,
                                           QStringList *categories = nullptr);

private:
  DbConnectionTemplateCatalog();
  void merge(const QList<DbConnectionTemplate> &entries,
             const QStringList &categories);

  QList<DbConnectionTemplate> m_templates;
  QStringList m_categories;
};

#endif
