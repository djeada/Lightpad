#include "dbconnectiontemplates.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace {

QJsonObject readJsonObject(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  return doc.isObject() ? doc.object() : QJsonObject();
}

QStringList stringList(const QJsonValue &value) {
  QStringList out;
  for (const QJsonValue &item : value.toArray()) {
    if (!item.toString().isEmpty()) {
      out << item.toString();
    }
  }
  return out;
}

} // namespace

DbConnectionProfile
DbConnectionTemplate::instantiate(const QString &newName) const {
  DbConnectionProfile p = profile;
  p.id.clear();
  p.name = newName.isEmpty() ? name : newName;
  return p;
}

QString DbConnectionTemplate::searchText() const {
  return QStringList{name, category, description,
                     DbEngineInfo::displayName(profile.engine),
                     keywords.join(' ')}
      .join(' ');
}

DbConnectionTemplateCatalog &DbConnectionTemplateCatalog::instance() {
  static DbConnectionTemplateCatalog catalog;
  return catalog;
}

DbConnectionTemplateCatalog::DbConnectionTemplateCatalog() { reload(); }

QString DbConnectionTemplateCatalog::userTemplatesPath() {
  return QDir(QStandardPaths::writableLocation(
                  QStandardPaths::AppConfigLocation))
      .filePath(QStringLiteral("database/connection_templates.json"));
}

QList<DbConnectionTemplate>
DbConnectionTemplateCatalog::parse(const QJsonObject &root,
                                   QStringList *categories) {
  QList<DbConnectionTemplate> out;
  if (categories) {
    *categories = stringList(root.value("categories"));
  }
  for (const QJsonValue &value : root.value("templates").toArray()) {
    const QJsonObject obj = value.toObject();
    bool engineOk = false;
    const DbEngine engine =
        DbEngineInfo::fromId(obj.value("engine").toString(), &engineOk);
    DbConnectionTemplate entry;
    entry.id = obj.value("id").toString().trimmed();
    entry.name = obj.value("name").toString().trimmed();
    if (!engineOk || !entry.isValid()) {
      continue;
    }
    entry.category = obj.value("category").toString(QStringLiteral("Other"));
    entry.description = obj.value("description").toString();
    entry.help = obj.value("help").toString();
    entry.docsUrl = obj.value("docs").toString();
    entry.keywords = stringList(obj.value("keywords"));
    entry.hostPlaceholder = obj.value("hostPlaceholder").toString();
    entry.userPlaceholder = obj.value("userPlaceholder").toString();
    entry.databasePlaceholder = obj.value("databasePlaceholder").toString();

    DbConnectionProfile &p = entry.profile;
    p.engine = engine;
    p.transport = obj.value("transport").toString() == QLatin1String("docker")
                      ? DbTransport::Docker
                      : DbTransport::Direct;
    // Without a host, local templates mean this machine; cloud templates
    // leave it empty and show their placeholder instead.
    p.host =
        obj.contains("host") ? obj.value("host").toString()
        : DbEngineInfo::usesFile(engine) || !entry.hostPlaceholder.isEmpty()
            ? QString()
            : QStringLiteral("localhost");
    p.port = obj.value("port").toInt(0);
    p.user = obj.contains("user") ? obj.value("user").toString()
                                  : DbEngineInfo::defaultUser(engine);
    p.database = obj.contains("database")
                     ? obj.value("database").toString()
                     : DbEngineInfo::defaultDatabase(engine);
    p.filePath = obj.value("file").toString();
    p.sslMode =
        DbEngineInfo::normalizedSslMode(obj.value("sslMode").toString());
    p.trustServerCertificate = obj.value("trustServerCertificate").toBool(true);
    p.readOnly = obj.value("readOnly").toBool(false);
    p.color = obj.value("color").toString();
    const QString password = obj.value("password").toString();
    p.passwordSource = password == QLatin1String("none")
                           ? DbPasswordSource::None
                           : DbPasswordSource::Ask;
    if (categories && !categories->contains(entry.category)) {
      categories->append(entry.category);
    }
    out.append(entry);
  }
  return out;
}

void DbConnectionTemplateCatalog::merge(
    const QList<DbConnectionTemplate> &entries, const QStringList &categories) {
  for (const QString &category : categories) {
    if (!m_categories.contains(category)) {
      m_categories.append(category);
    }
  }
  for (const DbConnectionTemplate &entry : entries) {
    bool replaced = false;
    for (DbConnectionTemplate &existing : m_templates) {
      if (existing.id == entry.id) {
        existing = entry;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      m_templates.append(entry);
    }
  }
}

void DbConnectionTemplateCatalog::reload() {
  m_templates.clear();
  m_categories.clear();
  for (const QString &path :
       {QStringLiteral(":/database/connection_templates.json"),
        userTemplatesPath()}) {
    QStringList categories;
    const QList<DbConnectionTemplate> entries =
        parse(readJsonObject(path), &categories);
    merge(entries, categories);
  }
}

DbConnectionTemplate
DbConnectionTemplateCatalog::templateById(const QString &id) const {
  for (const DbConnectionTemplate &entry : m_templates) {
    if (entry.id == id) {
      return entry;
    }
  }
  return {};
}
