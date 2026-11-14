#include "sqlquerycontext.h"
#include "sqlkeywords.h"
#include <QSet>
#include <algorithm>
#include <memory>

namespace {
struct Token {
  enum Kind { Name, Quoted, String, Number, Symbol, Comment } kind = Symbol;
  QString text;
  int start = 0, end = 0, pair = -1;
  bool is(const char *value) const {
    return kind == Name &&
           text.compare(QLatin1String(value), Qt::CaseInsensitive) == 0;
  }
  bool punctuation(char value) const {
    return kind == Symbol && text == QChar(value);
  }
  bool name() const { return kind == Name || kind == Quoted; }
};

QVector<Token> tokenize(const QString &sql,
                        DbEngine engine = DbEngine::PostgreSql) {
  QVector<Token> tokens;
  int i = 0;
  auto nameChar = [](QChar c) {
    return c.isLetterOrNumber() || c == '_' || c == '$' || c == '#' || c == '@';
  };
  while (i < sql.size() && tokens.size() < 30000) {
    if (sql[i].isSpace()) {
      ++i;
      continue;
    }
    Token token;
    token.start = i;
    const QChar first = sql[i];
    if (sql.mid(i, 2) == "--" || sql.mid(i, 2) == "/*" ||
        (first == '#' && engine == DbEngine::MySql)) {
      token.kind = Token::Comment;
      if (first == '-' || first == '#') {
        const int end = sql.indexOf('\n', i);
        i = end < 0 ? sql.size() : end;
      } else {
        i += 2;
        int depth = 1;
        while (i < sql.size() && depth) {
          if (sql.mid(i, 2) == "/*") {
            ++depth;
            i += 2;
          } else if (sql.mid(i, 2) == "*/") {
            --depth;
            i += 2;
          } else
            ++i;
        }
      }
    } else if (first == '\'' || first == '"' || first == '`' || first == '[') {
      token.kind = first == '\'' ? Token::String : Token::Quoted;
      const QChar close = first == '[' ? QChar(']') : first;
      ++i;
      while (i < sql.size()) {
        if (first == '\'' && sql[i] == '\\' && i + 1 < sql.size() &&
            (engine == DbEngine::MySql ||
             (engine == DbEngine::PostgreSql && token.start > 0 &&
              sql[token.start - 1].toLower() == 'e'))) {
          token.text += sql[i + 1];
          i += 2;
          continue;
        }
        if (sql[i] == close) {
          if (i + 1 < sql.size() && sql[i + 1] == close) {
            token.text += close;
            i += 2;
            continue;
          }
          ++i;
          break;
        }
        token.text += sql[i++];
      }
    } else if (first == '$') {
      int tagEnd = i + 1;
      while (tagEnd < sql.size() &&
             (sql[tagEnd].isLetterOrNumber() || sql[tagEnd] == '_'))
        ++tagEnd;
      if (tagEnd < sql.size() && sql[tagEnd] == '$') {
        const QString tag = sql.mid(i, tagEnd - i + 1);
        const int end = sql.indexOf(tag, tagEnd + 1);
        token.kind = Token::String;
        i = end < 0 ? sql.size() : end + tag.size();
      } else {
        token.kind = Token::Name;
        ++i;
        while (i < sql.size() && nameChar(sql[i]))
          token.text += sql[i++];
      }
    } else if (first.isLetter() || first == '_' || first == '#' ||
               first == '@') {
      token.kind = Token::Name;
      while (i < sql.size() && nameChar(sql[i]))
        token.text += sql[i++];
    } else if (first.isDigit()) {
      token.kind = Token::Number;
      while (i < sql.size() && (sql[i].isLetterOrNumber() || sql[i] == '.'))
        token.text += sql[i++];
    } else {
      token.kind = Token::Symbol;
      token.text = first;
      ++i;
      if (token.text == ":" && i < sql.size() && sql[i] == ':') {
        token.text += ':';
        ++i;
      }
    }
    token.end = i;
    tokens << token;
  }
  QList<int> brackets;
  for (int n = 0; n < tokens.size(); ++n) {
    if (tokens[n].punctuation('('))
      brackets << n;
    else if (tokens[n].punctuation(')') && !brackets.isEmpty()) {
      const int open = brackets.takeLast();
      tokens[open].pair = n;
      tokens[n].pair = open;
    }
  }
  return tokens;
}

using Source = SqlQueryContext::Source;
using SourceKind = SqlQueryContext::SourceKind;
using Environment = QMap<QString, Source>;
struct Projection {
  DbColumnInfo column;
  bool alias = false;
};
struct Scope {
  int start = 0, end = 0, first = 0, last = 0;
  Environment ctes;
  QVector<Source> sources, outer;
  QVector<Projection> output;
  QVector<QPair<int, QString>> clauses;
  bool clausesReady = false;
  QVector<std::shared_ptr<Scope>> children;
};

bool reserved(const Token &token) {
  if (token.kind == Token::Quoted)
    return false;
  return SqlKeywords::keywords().contains(token.text.toUpper()) ||
         QSet<QString>{"asc",  "desc",  "materialized", "apply", "lateral",
                       "only", "index", "indexed",      "not"}
             .contains(token.text.toLower());
}

class Analyzer {
public:
  Analyzer(const QString &sql, DbEngine engine, const DbSchema *catalog)
      : m_sql(sql), m_engine(engine), m_catalog(catalog) {
    for (const auto &token : tokenize(sql, engine))
      if (token.kind != Token::Comment)
        m_tokens << token;
    QList<int> brackets;
    for (int n = 0; n < m_tokens.size(); ++n) {
      if (m_tokens[n].punctuation(';'))
        m_statementEnds << n;
      m_tokens[n].pair = -1;
      if (m_tokens[n].punctuation('('))
        brackets << n;
      else if (m_tokens[n].punctuation(')') && !brackets.isEmpty()) {
        const int open = brackets.takeLast();
        m_tokens[open].pair = n;
        m_tokens[n].pair = open;
      }
    }
  }

  std::shared_ptr<Scope> root(int cursor) {
    const auto boundary =
        std::lower_bound(m_statementEnds.cbegin(), m_statementEnds.cend(),
                         cursor, [&](int index, int position) {
                           return m_tokens[index].start < position;
                         });
    const int first =
        boundary == m_statementEnds.cbegin() ? 0 : *(boundary - 1) + 1;
    const int last =
        boundary == m_statementEnds.cend() ? m_tokens.size() : *boundary;
    const QPair<int, int> key{first, last};
    auto found = m_roots.constFind(key);
    if (found != m_roots.cend())
      return found.value();
    auto result =
        query(first, last, {}, {}, 0,
              first < m_tokens.size() ? m_tokens[first].start : cursor,
              last < m_tokens.size() ? m_tokens[last].start : m_sql.size());
    m_roots[key] = result;
    return result;
  }

  SqlQueryContext::Context context(const std::shared_ptr<Scope> &root,
                                   int cursor) const {
    auto scope = root;
    for (;;) {
      std::shared_ptr<Scope> child;
      for (const auto &candidate : scope->children)
        if (cursor >= candidate->start && cursor <= candidate->end) {
          child = candidate;
          break;
        }
      if (!child)
        break;
      scope = child;
    }
    SqlQueryContext::Context result;
    result.scopeStart = scope->start;
    result.sources = scope->sources;
    QSet<QString> aliases;
    for (const auto &source : result.sources)
      aliases.insert(source.alias.toLower());
    for (const auto &source : scope->outer)
      if (!aliases.contains(source.alias.toLower())) {
        result.sources << source;
        aliases.insert(source.alias.toLower());
      }
    for (const auto &cte : scope->ctes)
      result.commonTables << cte;
    for (const auto &column : scope->output)
      if (column.alias && !column.column.name.isEmpty())
        result.outputAliases << column.column;
    if (!scope->clausesReady)
      for (int i = scope->first; i < scope->last;) {
        const auto &token = m_tokens[i];
        if (token.punctuation('(')) {
          i = afterBracket(i, scope->last);
          continue;
        }
        if (token.is("select") || token.is("from") || token.is("join") ||
            token.is("into") || token.is("update") || token.is("table") ||
            token.is("where") || token.is("on") || token.is("set") ||
            token.is("having") || token.is("and") || token.is("or") ||
            token.is("when") || token.is("then") || token.is("else") ||
            token.is("returning") || token.is("values"))
          scope->clauses << qMakePair(token.start, token.text.toUpper());
        if ((token.is("order") || token.is("group")) && i + 1 < scope->last &&
            m_tokens[i + 1].is("by"))
          scope->clauses << qMakePair(token.start,
                                      token.text.toUpper() + " BY");
        ++i;
      }
    scope->clausesReady = true;
    const auto clause = std::lower_bound(
        scope->clauses.cbegin(), scope->clauses.cend(), cursor,
        [](const auto &mark, int position) { return mark.first < position; });
    if (clause != scope->clauses.cbegin())
      result.keyword = (clause - 1)->second;
    return result;
  }

private:
  int afterBracket(int i, int end) const {
    return m_tokens[i].pair >= 0 ? qMin(end, m_tokens[i].pair + 1) : end;
  }
  int closeBracket(int i, int end) const {
    return m_tokens[i].pair >= 0 ? qMin(end, m_tokens[i].pair) : end;
  }
  QVector<DbColumnInfo> columnNames(int first, int last) const {
    QVector<DbColumnInfo> result;
    for (int i = first; i < last; ++i)
      if (m_tokens[i].name()) {
        DbColumnInfo column;
        column.name = m_tokens[i].text;
        result << column;
      }
    return result;
  }
  static QVector<DbColumnInfo> outputs(const Scope &scope) {
    QVector<DbColumnInfo> result;
    for (const auto &projection : scope.output)
      result << projection.column;
    return result;
  }
  static bool references(const Scope &scope, const QString &name) {
    for (const auto &source : scope.sources)
      if (source.kind == SourceKind::Cte &&
          source.name.compare(name, Qt::CaseInsensitive) == 0)
        return true;
    for (const auto &child : scope.children)
      if (references(*child, name))
        return true;
    return false;
  }
  Source physical(const QStringList &parts) const {
    Source source;
    if (parts.isEmpty())
      return source;
    source.name = source.alias = parts.last();
    source.qualifiedName = parts.join('.');
    if (parts.size() > 1)
      source.schema = parts[parts.size() - 2];
    if (m_catalog) {
      const auto *table = source.schema.isEmpty()
                              ? m_catalog->findByName(source.name)
                              : m_catalog->find(source.schema, source.name);
      if (table) {
        source.schema = table->schema;
        source.columns = table->columns;
        source.columnsKnown = true;
        source.kind = table->isView ? SourceKind::View : SourceKind::Table;
      }
    }
    return source;
  }

  std::shared_ptr<Scope> query(int first, int last, Environment ctes,
                               QVector<Source> outer, int depth, int start,
                               int end) {
    auto scope = std::make_shared<Scope>();
    scope->start = start;
    scope->end = end;
    scope->first = first;
    scope->last = last;
    scope->ctes = ctes;
    scope->outer = outer;
    if (first >= last || depth > 24)
      return scope;
    int body = first;
    if (m_tokens[body].is("with")) {
      ++body;
      if (body < last && m_tokens[body].is("recursive"))
        ++body;
      while (body < last && m_tokens[body].name()) {
        const Token name = m_tokens[body++];
        QVector<DbColumnInfo> explicitColumns;
        if (body < last && m_tokens[body].punctuation('(')) {
          explicitColumns = columnNames(body + 1, closeBracket(body, last));
          body = afterBracket(body, last);
        }
        if (body >= last || !m_tokens[body].is("as"))
          break;
        ++body;
        if (body < last && m_tokens[body].is("not"))
          ++body;
        if (body < last && m_tokens[body].is("materialized"))
          ++body;
        if (body >= last || !m_tokens[body].punctuation('('))
          break;
        const int close = closeBracket(body, last);
        Source relation;
        relation.name = relation.alias = relation.qualifiedName = name.text;
        relation.kind = SourceKind::Cte;
        relation.columns = explicitColumns;
        relation.columnsKnown = !explicitColumns.isEmpty();
        auto environment = ctes;
        environment[name.text.toLower()] = relation;
        auto child = query(body + 1, close, environment, outer, depth + 1,
                           m_tokens[body].end,
                           close < last ? m_tokens[close].start : end);
        // The anchor of a recursive UNION supplies the implicit output names.
        if (explicitColumns.isEmpty() && !child->output.isEmpty() &&
            references(*child, name.text)) {
          relation.columns = outputs(*child);
          relation.columnsKnown = true;
          environment[name.text.toLower()] = relation;
          child = query(body + 1, close, environment, outer, depth + 1,
                        m_tokens[body].end,
                        close < last ? m_tokens[close].start : end);
        }
        if (!explicitColumns.isEmpty()) {
          const auto inferred = outputs(*child);
          for (int n = 0; n < relation.columns.size() && n < inferred.size();
               ++n)
            relation.columns[n].type = inferred[n].type;
        } else {
          relation.columns = outputs(*child);
          relation.columnsKnown = !relation.columns.isEmpty();
          for (const auto &column : relation.columns)
            if (column.name.isEmpty())
              relation.columnsKnown = false;
        }
        ctes[name.text.toLower()] = relation;
        scope->children << child;
        body = close < last ? close + 1 : last;
        if (body < last && m_tokens[body].punctuation(','))
          ++body;
        else
          break;
      }
    }
    scope->ctes = ctes;
    scope->first = body;
    // Set-operation branches have independent FROM aliases. The first branch
    // names the output.
    QVector<int> branches{body};
    for (int i = body; i < last;) {
      if (m_tokens[i].punctuation('(')) {
        i = afterBracket(i, last);
        continue;
      }
      if (m_tokens[i].is("union") || m_tokens[i].is("except") ||
          m_tokens[i].is("intersect"))
        branches << i;
      ++i;
    }
    if (branches.size() > 1) {
      for (int n = 0; n < branches.size(); ++n) {
        int begin = branches[n];
        if (n > 0) {
          ++begin;
          if (begin < last &&
              (m_tokens[begin].is("all") || m_tokens[begin].is("distinct")))
            ++begin;
        }
        const int finish = n + 1 < branches.size() ? branches[n + 1] : last;
        auto child = query(begin, finish, ctes, outer, depth + 1,
                           begin < last ? m_tokens[begin].start : end,
                           finish < last ? m_tokens[finish].start : end);
        scope->children << child;
        if (n == 0)
          scope->output = child->output;
      }
      return scope;
    }
    QSet<int> parsedBrackets;
    bool from = false;
    for (int i = body; i < last;) {
      const auto &token = m_tokens[i];
      if (token.punctuation('(')) {
        i = afterBracket(i, last);
        continue;
      }
      const bool begins = token.is("from") || token.is("join") ||
                          token.is("apply") || token.is("update") ||
                          token.is("into");
      if (begins || (from && token.punctuation(','))) {
        from = true;
        const int next = source(*scope, i + 1, last, ctes, outer,
                                parsedBrackets, depth + 1, token.is("into"));
        i = qMax(i + 1, next);
        continue;
      }
      if (token.is("where") || token.is("group") || token.is("order") ||
          token.is("having") || token.is("set") || token.is("returning") ||
          token.is("limit"))
        from = false;
      ++i;
    }
    projections(*scope, body, last);
    for (int i = body; i < last; ++i) {
      if (!m_tokens[i].punctuation('('))
        continue;
      const int close = closeBracket(i, last);
      if (parsedBrackets.contains(i)) {
        i = close;
        continue;
      }
      if (i + 1 < close &&
          (m_tokens[i + 1].is("select") || m_tokens[i + 1].is("with"))) {
        scope->children << query(i + 1, close, ctes, scope->sources + outer,
                                 depth + 1, m_tokens[i].end,
                                 close < last ? m_tokens[close].start : end);
        i = close;
      }
    }
    return scope;
  }

  int source(Scope &scope, int i, int last, const Environment &ctes,
             const QVector<Source> &outer, QSet<int> &parsed, int depth,
             bool insert) {
    while (i < last && (m_tokens[i].is("lateral") || m_tokens[i].is("only")))
      ++i;
    if (i >= last)
      return i;
    Source relation;
    const int sourceStart = m_tokens[i].start;
    if (m_tokens[i].punctuation('(')) {
      const int close = closeBracket(i, last);
      auto child = query(i + 1, close, ctes, scope.sources + outer, depth,
                         m_tokens[i].end,
                         close < last ? m_tokens[close].start : scope.end);
      parsed.insert(i);
      scope.children << child;
      relation.kind = SourceKind::Subquery;
      relation.columns = outputs(*child);
      relation.columnsKnown = !relation.columns.isEmpty();
      for (const auto &column : relation.columns)
        if (column.name.isEmpty())
          relation.columnsKnown = false;
      i = close < last ? close + 1 : last;
    } else if (m_tokens[i].name()) {
      QStringList parts{m_tokens[i++].text};
      while (i + 1 < last && m_tokens[i].punctuation('.') &&
             m_tokens[i + 1].name()) {
        parts << m_tokens[i + 1].text;
        i += 2;
      }
      if (!insert && i < last && m_tokens[i].punctuation('(')) {
        relation.kind = SourceKind::Subquery;
        relation.name = relation.alias = parts.last();
        i = afterBracket(i, last);
      } else {
        relation = parts.size() == 1 && ctes.contains(parts.first().toLower())
                       ? ctes.value(parts.first().toLower())
                       : physical(parts);
      }
    } else
      return i + 1;
    relation.start = sourceStart;
    if (i < last && m_tokens[i].is("as"))
      ++i;
    if (i < last && m_tokens[i].name() && !reserved(m_tokens[i])) {
      relation.alias = m_tokens[i].text;
      relation.aliasStart = m_tokens[i].start;
      ++i;
    }
    if (relation.kind == SourceKind::Subquery && relation.name.isEmpty())
      relation.name = relation.alias;
    if (!insert && i < last && m_tokens[i].punctuation('(')) {
      const auto names = columnNames(i + 1, closeBracket(i, last));
      if (!names.isEmpty()) {
        relation.columns = names;
        relation.columnsKnown = true;
      }
      i = afterBracket(i, last);
    }
    scope.sources << relation;
    return i;
  }

  void projections(Scope &scope, int first, int last) {
    int select = -1, end = last;
    for (int i = first; i < last;) {
      if (m_tokens[i].punctuation('(')) {
        i = afterBracket(i, last);
        continue;
      }
      if (select < 0 && m_tokens[i].is("select"))
        select = i;
      else if (select >= 0 &&
               (m_tokens[i].is("from") || m_tokens[i].is("where") ||
                m_tokens[i].is("group") || m_tokens[i].is("order") ||
                m_tokens[i].is("limit"))) {
        end = i;
        break;
      }
      ++i;
    }
    if (select < 0)
      return;
    first = select + 1;
    while (first < end &&
           (m_tokens[first].is("all") || m_tokens[first].is("distinct")))
      ++first;
    if (first < end && m_tokens[first].is("on") && first + 1 < end &&
        m_tokens[first + 1].punctuation('('))
      first = afterBracket(first + 1, end);
    if (m_engine == DbEngine::SqlServer && first < end &&
        m_tokens[first].is("top")) {
      ++first;
      if (first < end)
        first = m_tokens[first].punctuation('(') ? afterBracket(first, end)
                                                 : first + 1;
      if (first < end && m_tokens[first].is("percent"))
        ++first;
      if (first + 1 < end && m_tokens[first].is("with") &&
          m_tokens[first + 1].is("ties"))
        first += 2;
    }
    int item = first;
    for (int i = first; i <= end;) {
      if (i == end || m_tokens[i].punctuation(',')) {
        projection(scope, item, i);
        item = ++i;
      } else if (m_tokens[i].punctuation('('))
        i = afterBracket(i, end);
      else
        ++i;
    }
  }

  void projection(Scope &scope, int first, int last) {
    if (first >= last)
      return;
    int alias = -1, expressionEnd = last;
    for (int i = first; i < last;) {
      if (m_tokens[i].punctuation('(')) {
        i = afterBracket(i, last);
        continue;
      }
      if (m_tokens[i].is("as") && i + 1 < last && m_tokens[i + 1].name()) {
        alias = i + 1;
        expressionEnd = i;
        break;
      }
      ++i;
    }
    if (alias < 0 && last - first >= 2 && m_tokens[last - 1].name() &&
        !reserved(m_tokens[last - 1])) {
      const auto &previous = m_tokens[last - 2];
      if (!previous.punctuation('.') &&
          !(previous.kind == Token::Symbol && !previous.punctuation(')'))) {
        alias = last - 1;
        expressionEnd = alias;
      }
    }
    QStringList parts;
    bool simple = true;
    for (int i = first; i < expressionEnd; ++i) {
      if ((i - first) % 2 == 0 &&
          (m_tokens[i].name() || m_tokens[i].punctuation('*')))
        parts << m_tokens[i].text;
      else if ((i - first) % 2 != 1 || !m_tokens[i].punctuation('.')) {
        simple = false;
        break;
      }
    }
    if (simple && !parts.isEmpty() && parts.last() == "*") {
      const QString qualifier =
          parts.size() > 1 ? parts[parts.size() - 2] : QString();
      for (const auto &source : scope.sources)
        if (qualifier.isEmpty() ||
            source.alias.compare(qualifier, Qt::CaseInsensitive) == 0)
          for (const auto &column : source.columns)
            scope.output << Projection{column, false};
      return;
    }
    Projection output;
    if (simple && !parts.isEmpty()) {
      output.column.name = parts.last();
      const QString qualifier =
          parts.size() > 1 ? parts[parts.size() - 2] : QString();
      for (const auto &source : scope.sources)
        if (qualifier.isEmpty() ||
            source.alias.compare(qualifier, Qt::CaseInsensitive) == 0)
          for (const auto &column : source.columns)
            if (column.name.compare(parts.last(), Qt::CaseInsensitive) == 0)
              output.column = column;
    }
    if (alias >= 0) {
      output.column.name = m_tokens[alias].text;
      output.alias = true;
    }
    scope.output << output;
  }

  QString m_sql;
  DbEngine m_engine;
  const DbSchema *m_catalog;
  QVector<Token> m_tokens;
  QVector<int> m_statementEnds;
  QMap<QPair<int, int>, std::shared_ptr<Scope>> m_roots;
};
} // namespace

namespace SqlQueryContext {
Context analyze(const QString &statement, int cursor, DbEngine engine,
                const DbSchema *schema) {
  return analyzePositions(statement, {cursor}, engine, schema).first();
}

QVector<Context> analyzePositions(const QString &statement,
                                  const QVector<int> &cursors, DbEngine engine,
                                  const DbSchema *schema) {
  QVector<Context> results;
  results.reserve(cursors.size());
  const auto tokens = tokenize(statement, engine);
  Analyzer analyzer(statement, engine, schema);
  for (int cursor : cursors) {
    cursor = qBound(0, cursor, int(statement.size()));
    const auto after = std::lower_bound(tokens.cbegin(), tokens.cend(), cursor,
                                        [](const Token &token, int position) {
                                          return token.start < position;
                                        });
    if (after != tokens.cbegin()) {
      const auto &token = *(after - 1);
      if (cursor <= token.end &&
          (token.kind == Token::String || token.kind == Token::Comment)) {
        Context context;
        context.inCommentOrString = true;
        results << context;
        continue;
      }
    }
    results << analyzer.context(analyzer.root(cursor), cursor);
  }
  return results;
}

IdentifierPrefix identifierPrefix(const QString &text, int cursor) {
  IdentifierPrefix result;
  result.start = result.end = cursor;
  for (const auto &token : tokenize(text)) {
    if (token.start < cursor && cursor <= token.end && token.name()) {
      result.text = text.mid(token.start, cursor - token.start);
      result.start = token.start;
      result.end = token.end;
      break;
    }
  }
  return result;
}

QStringList qualifierBefore(const QString &text, int identifierStart) {
  const auto tokens = tokenize(text.left(identifierStart));
  QStringList names;
  int i = tokens.size() - 1;
  while (i >= 0 && tokens[i].kind == Token::Comment)
    --i;
  if (i < 0 || !tokens[i].punctuation('.'))
    return names;
  --i;
  while (i >= 0 && tokens[i].name()) {
    names.prepend(tokens[i--].text);
    if (i < 0 || !tokens[i].punctuation('.'))
      break;
    --i;
  }
  return names;
}
} // namespace SqlQueryContext
