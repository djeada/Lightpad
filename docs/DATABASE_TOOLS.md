# Database Tools

Lightpad has a built-in database workbench: connect to several databases at
once, browse their schema, run SQL from any editor tab or from a console, and
look at the results as grids, exports and column statistics.

| Engine                | How Lightpad talks to it                                   |
|-----------------------|------------------------------------------------------------|
| SQLite                | Qt SQL (`QSQLITE`), on its own thread                      |
| SQL Server            | `sqlcmd`, on this machine or inside a Docker container     |
| PostgreSQL            | `psql`, on this machine or inside a Docker container       |
| MySQL / MariaDB       | `mysql` / `mariadb`, on this machine or inside a container |

The client-based engines keep **one client process alive per connection**, so
`USE`, `SET`, transactions and temp tables behave like a real session.
Running the client through `docker exec` means a database in a container needs
nothing installed on the host but Docker.

## Getting started

1. Open the panel: **Database → Database Panel** (`Ctrl+Shift+D`).
2. Click **+** (New connection). Running database containers are listed at the
   top of the dialog; click one and the engine, container, user, database and
   (when the image was started with one) the password are filled in.
3. **Test connection**, then **Save & Connect**.
4. Type SQL in the console and press `Ctrl+Enter`, or open a `.sql` file and
   press `F9`.

## Running SQL

| Where | Shortcut | What runs |
|-------|----------|-----------|
| Console | `Ctrl+Enter` | The selection, or the statement under the caret |
| Console | `Ctrl+Shift+Enter` | Every statement in the console |
| Editor tab | `F9` | The selection, or the statement under the caret |
| Editor tab | `Shift+F9` | The whole script |
| Editor tab / console | `Ctrl+F9` | The execution plan (`EXPLAIN`, `SHOWPLAN_ALL`) |
| Console | `Alt+Up` / `Alt+Down` | Older / newer statements from the history |

The statement under the caret is highlighted lightly in the console. Statements
are split on `;` with awareness of quotes, comments, PostgreSQL `$$ … $$`
bodies, MySQL `#` comments, T-SQL `GO` batches and `BEGIN … END` blocks (also
SQLite trigger bodies).

**Choosing the connection.** Queries run against the active connection (status
bar chip, connection box in the panel). A script can pin its own with a comment
in its first lines:

```sql
-- connection: Local SQL Server
SELECT TOP 10 * FROM sys.tables;
```

`Database → New SQL Query` (`Ctrl+Shift+Q`) opens an editor tab and adds that
header when you have more than one connection.

## Results

* **Results** — one grid per result set (stored procedures may return several).
  Click a header to sort (numbers sort numerically, `NULL`s first), type in the
  filter box to narrow rows, double-click a cell to see the full value (JSON is
  pretty-printed). `Ctrl+C` copies the selection as tab-separated text,
  `Ctrl+Shift+C` adds headers; **Copy as** offers CSV, JSON, Markdown and SQL
  `INSERT`; **Export** writes the visible rows to a file.
* **Messages** — a log of every statement with timing, row counts, server
  messages and errors.
* **Insights** — per-column profile of the result: kind, completeness (NULLs),
  distinct values, min/max, mean/median and the most frequent values.

At most 10 000 rows per result are kept in memory; the grid says when a result
was cut off.

## Schema browser

Each connection is a tree of schemas → tables/views → columns (with types,
`NOT NULL` and primary keys). The filter box matches table and column names.
Right-click a table for:

* **Preview data** (also double-click), **Count rows**, **Show columns**
* **Profile columns…** — loads up to 10 000 rows and opens Insights
* **Generate SQL** — `SELECT` with all columns, `INSERT`/`UPDATE` templates,
  approximate `CREATE TABLE`

The tree refreshes by itself after `CREATE`/`ALTER`/`DROP`. SQL Server and
PostgreSQL show the database selected in the toolbar; MySQL shows every
database as a node.

## Completion

In `.sql` files and in the console: table, view and column names of the
connection (`u.` after `FROM users u` lists the columns of `users`), schemas,
keywords, functions and types. Keywords follow the case you type.

## Safety

* **Read-only** connections refuse anything but reads (`SELECT`, `SHOW`,
  `EXPLAIN`, `USE`/`SET`, transaction control).
* **Confirm destructive statements** (default on): `DROP`, `TRUNCATE`, and
  `DELETE`/`UPDATE` without `WHERE` ask first.
* **Colour tags** (green/amber/red …) colour the connection and a strip above
  the console, so production is hard to mistake for a dev database.
* **Passwords are never written to disk.** They live in memory for the session;
  a rejected login is forgotten. In Docker mode the password reaches the client
  through stdin, not through `docker exec` arguments or `ps`; in direct mode it
  is passed through the client's password environment variable.
* Only `connections.json` (without passwords) and `history.json` are stored, in
  the application config folder under `database/`.

## Limits worth knowing

* Query cancellation stops the client process for server engines; the session
  reconnects on the next query (transactions in flight are lost). SQLite queries
  cannot be interrupted — the result is discarded when the query finishes.
* The clients print no column types and no distinction between `NULL` and the
  text `NULL` for SQL Server and MySQL/MariaDB (PostgreSQL uses a sentinel), so
  result values are text; sorting and Insights infer numbers from the text.
* PostgreSQL client meta-commands (`\d`, `\dt`) work; `\c` is used internally
  to switch databases. Multi-line values in the *last* column of a one-column
  SQL Server result are shown as separate rows (a `sqlcmd` limitation).
* The official PostgreSQL image trusts local socket connections, so with the
  Docker transport any password is accepted.

## Architecture

```
App/database/                 (no GUI, unit-tested)
  dbtypes            profiles, results, engine metadata
  sqlstatementsplitter        splitting, classification, safety analysis
  clidialect         psql / mysql / sqlcmd: launch, framing, output parsers
  clisession         persistent client process, markers per statement, cancel
  sqlitesession      QtSql on a worker thread
  dbconnection       session + queue + schema cache + read-only guard
  databasemanager    connections, active selection, history
  dbcatalog          catalog queries, schema model, SQL templates
  sqlcompletion      schema-aware suggestions
  resultexporter, columnprofiler, dockerdiscovery, connectionstore, queryhistory
App/ui/panels/                databasepanel, dbresultview, dbinsightsview, sqlconsoleedit
App/ui/dialogs/               connectiondialog, queryhistorydialog
App/ui/mainwindow_database.cpp  dock, menu, status chip, F9 commands, completion provider
App/syntax/sqlsyntaxplugin      SQL highlighting for .sql files
```

**How a statement is run through a client.** Every statement is written to the
client's stdin followed by a unique marker (`\echo`/`\warn` for psql,
`SELECT '…'` + `\! echo … 1>&2` for mysql, `SELECT` + `PRINT` for sqlcmd). The
session collects stdout and stderr separately until the marker shows up on both,
then the dialect parses the text into rows, messages and errors. Adding another
engine that has a scriptable CLI means implementing one `CliDialect`.

## Testing

```
ctest -R "SqlStatementSplitter|DatabaseCore|DatabasePanel|DatabaseLive"
```

`DatabaseLiveTests` always exercises SQLite. To run the same workflow against
real servers, start containers and point the test at them:

```
docker run -d --name lp-mssql -e ACCEPT_EULA=Y -e MSSQL_SA_PASSWORD='LpTest#2026x' mcr.microsoft.com/mssql/server:2022-latest
docker run -d --name lp-pg    -e POSTGRES_PASSWORD=pgpass postgres:16-alpine
docker run -d --name lp-maria -e MARIADB_ROOT_PASSWORD=mypass mariadb:11
docker exec lp-maria mariadb -uroot -pmypass -e 'CREATE DATABASE lp_live_db'

LIGHTPAD_TEST_MSSQL='docker:lp-mssql:sa:LpTest#2026x' \
LIGHTPAD_TEST_PG='docker:lp-pg:postgres:pgpass' \
LIGHTPAD_TEST_MYSQL='docker:lp-maria:root:mypass' \
ctest -R DatabaseLive --output-on-failure
```

(Lightpad looks for `sqlcmd` in `/opt/mssql-tools18/bin` and `/opt/mssql-tools/bin` inside the container; the 2025 image ships it.)
