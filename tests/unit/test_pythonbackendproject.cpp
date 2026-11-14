#include "python/pythonbackendproject.h"
#include "test_templates/testdiscovery.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestPythonBackendProject : public QObject {
  Q_OBJECT
private:
  static QString write(const QTemporaryDir &dir, const QString &path,
                       const QByteArray &contents) {
    const QString full = dir.filePath(path);
    QDir().mkpath(QFileInfo(full).absolutePath());
    QFile file(full);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(contents) != contents.size())
      return {};
    return full;
  }
  static void config(const QTemporaryDir &dir, const QJsonObject &settings) {
    QVERIFY(
        !write(dir, ".lightpad/backend.json", QJsonDocument(settings).toJson())
             .isEmpty());
  }

private slots:
  void detectsSrcLayoutWithoutImportingCode() {
    QTemporaryDir dir;
    const auto entry = write(
        dir, "src/main.py",
        "from fastapi import FastAPI\n"
        "app = FastAPI(\n title='Demo'\n)\n"
        "raise RuntimeError('must never be imported during discovery')\n");
    const auto project = PythonBackendProject::discover(dir.path());
    QVERIFY2(project.isValid(), qPrintable(project.error));
    QCOMPARE(project.framework, "fastapi");
    QCOMPARE(project.application, "main:app");
    QCOMPARE(project.entryFile, entry);
    QCOMPARE(project.sourceDirectory, dir.filePath("src"));
    QCOMPARE(project.workingDirectory, dir.path());
    QCOMPARE(project.envFile, dir.filePath(".env"));
    QCOMPARE(project.baseUrl(), "http://127.0.0.1:8000");
    QVERIFY(project.ownsFile(dir.filePath("src/routers/posts.py")));
    QVERIFY(project.ownsFile(dir.filePath("src/templates/index.html")));
    QVERIFY(project.ownsFile({}));
    QVERIFY(!project.ownsFile(dir.filePath("tests/test_api.py")));
    QVERIFY(!project.ownsFile(dir.filePath("scripts/import.py")));
    QVERIFY(!project.ownsFile(dir.filePath(".venv/lib/server.py")));
    QVERIFY(!project.ownsFile(dir.filePath("../outside.py")));
    QVERIFY(project.environment()
                .value("PYTHONPATH")
                .startsWith(dir.filePath("src")));
    QVERIFY(project.serverArguments().contains("--reload"));
    QVERIFY(!project.serverArguments(true).contains("--reload"));
  }

  void detectsFlaskPackageAndFactory() {
    QTemporaryDir dir;
    write(dir, "blog/__init__.py",
          "from flask import Flask\ndef create_app():\n return "
          "Flask(__name__)\n");
    const auto project = PythonBackendProject::discover(dir.path());
    QVERIFY2(project.isValid(), qPrintable(project.error));
    QCOMPARE(project.application, "blog:create_app");
    QCOMPARE(project.sourceDirectory, dir.path());
    QCOMPARE(project.module(), "flask");
    QCOMPARE(project.port, 5000);
    const auto debug = project.serverArguments(true);
    QVERIFY(debug.contains("--no-reload"));
    QVERIFY(debug.contains("--no-debugger"));
    QVERIFY(!debug.contains("--debug"));
    QCOMPARE(project.environment().value("FLASK_SKIP_DOTENV"), "1");
  }

  void refusesAmbiguityAndAcceptsExplicitEntry() {
    QTemporaryDir dir;
    write(dir, "one.py", "from fastapi import FastAPI\napp = FastAPI()\n");
    write(dir, "two.py", "from flask import Flask\napp = Flask(__name__)\n");
    auto project = PythonBackendProject::discover(dir.path());
    QVERIFY(!project.isValid());
    QVERIFY(project.error.contains("Several"));
    config(dir, {{"entryFile", "two.py"}, {"port", 5050}});
    project = PythonBackendProject::discover(dir.path());
    QVERIFY(project.isValid());
    QCOMPARE(project.framework, "flask");
    QCOMPARE(project.application, "two:app");
    QCOMPARE(project.port, 5050);
  }

  void ignoresTestsAndEnvironmentPackages() {
    QTemporaryDir dir;
    for (const QString &path : {QString("tests/app.py"), QString("test_api.py"),
                                QString("venv/app.py"), QString(".venv/app.py"),
                                QString("scripts/server.py")})
      write(dir, path, "from fastapi import FastAPI\napp = FastAPI()\n");
    QVERIFY(!PythonBackendProject::discover(dir.path()).isValid());
  }

  void reportsBadSettingsAndAllowsOptOut() {
    QTemporaryDir dir;
    write(dir, "main.py", "from fastapi import FastAPI\napp = FastAPI()\n");
    write(dir, ".lightpad/backend.json", "{broken");
    QVERIFY(!PythonBackendProject::discover(dir.path()).error.isEmpty());
    config(dir, {{"enabled", false}});
    const auto disabled = PythonBackendProject::discover(dir.path());
    QVERIFY(!disabled.isValid());
    QVERIFY(disabled.error.isEmpty());
    QVERIFY(disabled.databaseFiles().isEmpty());
    config(dir, {{"entryFile", "missing.py"}});
    QVERIFY(
        PythonBackendProject::discover(dir.path()).error.contains("entryFile"));
    config(dir, {{"port", 70000}});
    QVERIFY(!PythonBackendProject::discover(dir.path()).isValid());
    config(dir, {{"application", "$(touch marker):app"}});
    QVERIFY(!PythonBackendProject::discover(dir.path()).isValid());
    QVERIFY(!QFileInfo::exists(dir.filePath("marker")));
  }

  void supportsExplicitApplicationsAndDirectoriesWithSpaces() {
    QTemporaryDir dir(QDir::tempPath() + "/backend project XXXXXX");
    QDir().mkpath(dir.filePath("source folder"));
    config(dir, {{"framework", "fastapi"},
                 {"application", "service:create_app"},
                 {"sourceDirectory", "source folder"},
                 {"workingDirectory", "."},
                 {"envFile", "dev.env"},
                 {"host", "0.0.0.0"},
                 {"port", 8080}});
    const auto project = PythonBackendProject::discover(dir.path());
    QVERIFY2(project.isValid(), qPrintable(project.error));
    QCOMPARE(project.sourceDirectory, dir.filePath("source folder"));
    QCOMPARE(project.workingDirectory, dir.path());
    QCOMPARE(project.envFile, dir.filePath("dev.env"));
    QCOMPARE(project.baseUrl(), "http://localhost:8080");
  }

  void dotenvTreatsValuesAsData() {
    QTemporaryDir dir;
    const auto path =
        write(dir, ".env",
              "# comment\nexport LP_TEST_BACKEND_A=first\n"
              "LP_TEST_BACKEND_B=\"contains # hash\" # comment\n"
              "LP_TEST_BACKEND_C='literal value'\n"
              "LP_TEST_BACKEND_D=value # comment\n"
              "LP_TEST_BACKEND_E=\"line 1\nline 2\"\n"
              "LP_TEST_BACKEND_F=${LP_TEST_BACKEND_A}/second\n"
              "LP_TEST_BACKEND_G=$(touch marker)\nLP_TEST_BACKEND_EMPTY=\n");
    const auto env = PythonBackendProject::readEnvFile(path);
    QCOMPARE(env.value("LP_TEST_BACKEND_A"), "first");
    QCOMPARE(env.value("LP_TEST_BACKEND_B"), "contains # hash");
    QCOMPARE(env.value("LP_TEST_BACKEND_C"), "literal value");
    QCOMPARE(env.value("LP_TEST_BACKEND_D"), "value");
    QCOMPARE(env.value("LP_TEST_BACKEND_E"), "line 1\nline 2");
    QCOMPARE(env.value("LP_TEST_BACKEND_F"), "first/second");
    QCOMPARE(env.value("LP_TEST_BACKEND_G"), "$(touch marker)");
    QVERIFY(env.contains("LP_TEST_BACKEND_EMPTY"));
    QVERIFY(!QFileInfo::exists(dir.filePath("marker")));
  }

  void environmentPrecedence() {
    QTemporaryDir dir;
    write(dir, "main.py", "from fastapi import FastAPI\napp = FastAPI()\n");
    write(dir, ".env", "LP_TEST_BACKEND_PRIORITY=dotenv\n");
    const QByteArray previous = qgetenv("LP_TEST_BACKEND_PRIORITY");
    const bool existed = qEnvironmentVariableIsSet("LP_TEST_BACKEND_PRIORITY");
    const auto cleanup = qScopeGuard([&]() {
      if (existed)
        qputenv("LP_TEST_BACKEND_PRIORITY", previous);
      else
        qunsetenv("LP_TEST_BACKEND_PRIORITY");
    });
    qputenv("LP_TEST_BACKEND_PRIORITY", "process");
    auto project = PythonBackendProject::discover(dir.path());
    QCOMPARE(project.environment().value("LP_TEST_BACKEND_PRIORITY"),
             "process");
    config(dir,
           {{"env", QJsonObject{{"LP_TEST_BACKEND_PRIORITY", "workspace"}}}});
    project = PythonBackendProject::discover(dir.path());
    QCOMPARE(project.environment().value("LP_TEST_BACKEND_PRIORITY"),
             "workspace");
  }

  void resolvesSqliteUrls_data() {
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expected");
    QTest::newRow("relative") << QStringLiteral("sqlite:///./blog.db")
                              << QStringLiteral("/project/blog.db");
    QTest::newRow("absolute") << QStringLiteral("sqlite:////tmp/blog.db")
                              << QStringLiteral("/tmp/blog.db");
    QTest::newRow("driver")
        << QStringLiteral("sqlite+pysqlite:///blog%20data.db?timeout=5")
        << QStringLiteral("/project/blog data.db");
    QTest::newRow("memory")
        << QStringLiteral("sqlite:///:memory:") << QStringLiteral("");
    QTest::newRow("remote")
        << QStringLiteral("postgresql://user:pass@localhost/blog")
        << QStringLiteral("");
    QTest::newRow("uri") << QStringLiteral(
                                "sqlite:///file:test?mode=memory&uri=true")
                         << QStringLiteral("");
  }
  void resolvesSqliteUrls() {
    QFETCH(QString, url);
    QFETCH(QString, expected);
    QCOMPARE(PythonBackendProject::sqlitePath(url, "/project"), expected);
  }

  void discoversExistingDatabasesWithoutCreatingFiles() {
    QTemporaryDir dir;
    write(dir, "src/main.py", "from fastapi import FastAPI\napp = FastAPI()\n");
    const QByteArray header("SQLite format 3\0", 16);
    const auto rootDb = write(dir, "blog.db", header);
    const auto srcDb = write(dir, "src/blog.db", header);
    const auto customDb = write(dir, "storage/data", header);
    write(dir, "invalid.db", "not a database");
    write(dir, ".env", "BLOG_DATABASE_URL=sqlite:///blog.db\n");
    config(dir, {{"databaseFiles",
                  QJsonArray{"storage/data", "blog.db", "missing.db"}}});
    const auto project = PythonBackendProject::discover(dir.path());
    const auto files = project.databaseFiles();
    QCOMPARE(files.size(), 3);
    QCOMPARE(files.first(), rootDb);
    QVERIFY(files.contains(srcDb));
    QVERIFY(files.contains(customDb));
    QVERIFY(!QFileInfo::exists(dir.filePath("missing.db")));
  }

  void sharesEntryInterpreterPreferenceAndFindsDevRequirements() {
    QTemporaryDir dir;
    write(dir, "src/main.py", "from fastapi import FastAPI\napp = FastAPI()\n");
    write(dir, "requirements.txt", "fastapi\n");
    const auto dev =
        write(dir, "requirements-dev.txt", "-r requirements.txt\npytest\n");
    write(dir, ".lightpad/run_config.json",
          QJsonDocument(
              QJsonObject{{"assignments",
                           QJsonArray{QJsonObject{
                               {"file", "src/main.py"},
                               {"pythonMode", "customInterpreter"},
                               {"pythonInterpreter", "/tools/python"}}}}})
              .toJson());
    auto project = PythonBackendProject::discover(dir.path());
    QCOMPARE(project.requirementsFile(), dev);
    QCOMPARE(project.pythonPreference().mode, "customInterpreter");
    QCOMPARE(project.pythonPreference().customInterpreter, "/tools/python");
    config(dir, {{"pythonInterpreter", "/override/python"}});
    project = PythonBackendProject::discover(dir.path());
    QCOMPARE(project.pythonPreference().customInterpreter, "/override/python");
  }

  void pytestDiscoveryUsesWorkspacePython() {
#ifdef Q_OS_WIN
    QSKIP("Uses a POSIX test interpreter");
#else
    QTemporaryDir dir;
    write(dir, "src/main.py", "from fastapi import FastAPI\napp = FastAPI()\n");
    const auto python = write(
        dir, ".venv/bin/python",
        "#!/bin/sh\n"
        "case \"$PYTHONPATH\" in */src*) ;; *) exit 41 ;; esac\n"
        "test \"$LP_TEST_DISCOVERY_ENV\" = expected || exit 42\n"
        "printf 'tests/test_api.py::test_health\\n1 test collected\\n'\n");
    QVERIFY(QFile::setPermissions(python, QFileDevice::ReadOwner |
                                              QFileDevice::WriteOwner |
                                              QFileDevice::ExeOwner));
    PytestDiscoveryAdapter adapter;
    TestConfiguration config;
    config.env["LP_TEST_DISCOVERY_ENV"] = "expected";
    adapter.setTestConfiguration(config, dir.path());
    QSignalSpy finished(&adapter, &ITestDiscoveryAdapter::discoveryFinished);
    QSignalSpy errors(&adapter, &ITestDiscoveryAdapter::discoveryError);
    adapter.discover(dir.path());
    QVERIFY(finished.wait(5000));
    QCOMPARE(errors.size(), 0);
    const auto tests =
        qvariant_cast<QList<DiscoveredTest>>(finished.first().first());
    QCOMPARE(tests.size(), 1);
    QCOMPARE(tests.first().name, "test_health");
#endif
  }
};

QTEST_GUILESS_MAIN(TestPythonBackendProject)
#include "test_pythonbackendproject.moc"
