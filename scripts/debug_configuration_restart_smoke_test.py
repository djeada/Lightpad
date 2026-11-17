#!/usr/bin/env python3
"""Check saved debug arguments through real MainWindow instances after reopening."""

import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

DRIVER = r"""
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QMenu>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include "ui/mainwindow.h"
#include "ui/dialogs/debugconfigurationdialog.h"
#include "dap/debugsession.h"
#include "settings/settingsmanager.h"

static void require(bool ok, const char *message) {
  if (!ok) {
    fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

static void waitFor(const std::function<bool()> &condition) {
  QElapsedTimer timer;
  timer.start();
  while (!condition() && timer.elapsed() < 10000) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    QThread::msleep(5);
  }
  require(condition(), "debuggee did not write its arguments");
}

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName("LightpadDebugRestartTest");
  app.setOrganizationName("LightpadDebugRestartTest");
  const QString mode = QString::fromLocal8Bit(argv[1]);
  const QString workspace = QString::fromLocal8Bit(argv[2]);
  MainWindow window;
  QTimer::singleShot(0, &window, [&]() {
    auto &settings = SettingsManager::instance();
    auto *button = window.findChild<QToolButton *>("debugButton");
    require(button, "Debug button missing");
    if (mode == "save") {
      require(QMetaObject::invokeMethod(
                  &window, "on_actionEdit_Debug_Configurations_triggered"),
              "could not open debug configurations");
      auto *dialog = window.findChild<DebugConfigurationDialog *>();
      require(dialog, "configuration dialog missing");
      QLineEdit *arguments = nullptr;
      for (auto *edit : dialog->findChildren<QLineEdit *>())
        if (edit->placeholderText() == "Program arguments (space-separated)")
          arguments = edit;
      require(arguments, "Arguments field missing");
      arguments->setText("REOPEN_ARG");
      require(QMetaObject::invokeMethod(dialog, "onSave"),
              "could not save debug arguments");
      require(settings.getValue("activeDebugTarget").toString() ==
                  "C++: GDB Launch",
              "saving did not select the configuration");
      require(settings.saveSettings(), "could not persist selected target");
    } else {
      require(settings.getValue("activeDebugTarget").toString() ==
                  "C++: GDB Launch",
              "reopening lost the selected configuration");
      require(button->text().contains("C++: GDB Launch"),
              "Debug button lost the selected configuration");
      const QString proof = workspace + "/received.txt";
      QFile::remove(proof);
      QStringList actualArguments;
      auto &sessions = DebugSessionManager::instance();
      QObject::connect(&sessions, &DebugSessionManager::sessionStarted,
                       &window, [&](const QString &id) {
        if (auto *session = sessions.session(id))
          actualArguments = session->configuration().args;
      });
      if (mode == "button") {
        button->click();
      } else if (mode == "menu") {
        auto *action = window.findChild<QAction *>("actionDebug_file_name");
        require(action, "Debug menu action missing");
        action->trigger();
      } else {
        auto *run = window.findChild<QToolButton *>("runButton");
        require(run && run->menu(), "Run target menu missing");
        QMetaObject::invokeMethod(run->menu(), "aboutToShow");
        QAction *debug = nullptr;
        for (auto *action : run->menu()->actions())
          if (action->text().startsWith("Debug "))
            debug = action;
        require(debug, "Run menu Debug action missing");
        require(debug->text().contains("C++: GDB Launch"),
                "Run menu does not identify the selected debug profile");
        debug->trigger();
      }
      waitFor([&]() { return QFileInfo(proof).size() > 0; });
      require(actualArguments == QStringList{"REOPEN_ARG"},
              "debug session lost the saved argument");
      QFile received(proof);
      require(received.open(QIODevice::ReadOnly), "cannot read argument proof");
      require(received.readAll() == "argc=2\nREOPEN_ARG\n",
              "debuggee did not receive the saved argument");
      waitFor([&]() { return !sessions.hasActiveSessions(); });
    }
    printf("PASS: %s in a fresh application process\n",
           mode.toLocal8Bit().constData());
    fflush(stdout);
    window.close();
    app.quit();
  });
  return app.exec();
}
"""


def link_command(build):
    make_link = build / "App/CMakeFiles/Lightpad.dir/link.txt"
    if make_link.exists():
        return shlex.split(make_link.read_text()), build / "App"
    if (build / "build.ninja").exists() and shutil.which("ninja"):
        commands = subprocess.check_output(
            ["ninja", "-C", str(build), "-t", "commands", "Lightpad"], text=True
        )
        for line in reversed(commands.splitlines()):
            tokens = shlex.split(line)
            while "&&" in tokens:
                end = tokens.index("&&")
                segment, tokens = tokens[:end], tokens[end + 1 :]
                if "-o" in segment and any(
                    token.endswith("Lightpad.dir/main.cpp.o") for token in segment
                ):
                    return segment, build
            if "-o" in tokens and any(
                token.endswith("Lightpad.dir/main.cpp.o") for token in tokens
            ):
                return tokens, build
    return None, None


def main():
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: debug_configuration_restart_smoke_test.py BUILD/App/Lightpad"
        )
    executable = Path(sys.argv[1]).resolve()
    repo = Path(__file__).resolve().parent.parent
    build = executable.parent.parent
    compiler = shutil.which("cc")
    gdb = shutil.which("gdb")
    database = build / "compile_commands.json"
    if not executable.exists() or not database.exists() or not compiler or not gdb:
        print("SKIP: needs a built Lightpad, compilation database, C compiler and GDB")
        return 2
    probe = subprocess.run(
        [gdb, "--interpreter=dap", "--version"],
        capture_output=True,
        timeout=10,
        check=False,
    )
    trace = subprocess.run(
        [gdb, "-q", "-batch", "-ex", "file /bin/true", "-ex", "starti"],
        capture_output=True,
        timeout=10,
        check=False,
    )
    if probe.returncode or trace.returncode:
        print("SKIP: GDB DAP or process tracing is unavailable")
        return 2
    command, link_directory = link_command(build)
    if command is None:
        print("SKIP: needs a Makefiles or Ninja application build")
        return 2
    entry = next(
        item
        for item in json.loads(database.read_text())
        if Path(item["file"]).resolve() == repo / "App/main.cpp"
    )
    with tempfile.TemporaryDirectory(prefix="lightpad-debug-restart-") as temporary:
        root = Path(temporary)
        workspace = root / "workspace"
        workspace.mkdir()
        (workspace / ".git").mkdir()
        source = workspace / "argument.c"
        proof = workspace / "received.txt"
        source.write_text(
            "#include <stdio.h>\nint main(int argc, char **argv) {\n"
            f' FILE *f = fopen({json.dumps(str(proof))}, "w");\n'
            ' if (!f) return 3;\n fprintf(f, "argc=%d\\n", argc);\n'
            ' for (int i = 1; i < argc; ++i) fprintf(f, "%s\\n", argv[i]);\n'
            " fclose(f);\n return argc == 2 ? 0 : 2;\n}\n"
        )
        subprocess.run(
            [compiler, "-g", "-O0", str(source), "-o", str(workspace / "argument")],
            check=True,
            capture_output=True,
            timeout=30,
        )
        configuration = {
            "name": "C++: GDB Launch",
            "type": "cppdbg",
            "adapterId": "cppdbg-gdb",
            "request": "launch",
            "program": str(workspace / "argument"),
            "cwd": str(workspace),
        }
        launch = workspace / ".lightpad/debug/launch.json"
        launch.parent.mkdir(parents=True)
        launch.write_text(json.dumps({"configurations": [configuration]}))
        settings = (
            root
            / "config/LightpadDebugRestartTest/LightpadDebugRestartTest/settings.json"
        )
        settings.parent.mkdir(parents=True)
        settings.write_text(
            json.dumps(
                {
                    "lastProjectPath": str(workspace),
                    "openTabs": [str(source)],
                    "currentFilePath": str(source),
                    "showDebugDock": True,
                }
            )
        )
        driver = root / "driver.cpp"
        driver.write_text(DRIVER)
        obj = root / "driver.o"
        compile_command = entry.get("arguments") or shlex.split(entry["command"])
        compile_command[compile_command.index("-o") + 1] = str(obj)
        compile_command[compile_command.index("-c") + 1] = str(driver)
        subprocess.run(
            compile_command,
            cwd=entry["directory"],
            check=True,
            capture_output=True,
            timeout=60,
        )
        # Reuse the production application objects and resources, replacing main
        # with a driver that exercises its public Qt actions in separate processes.
        main_index = next(
            i
            for i, token in enumerate(command)
            if token.endswith("Lightpad.dir/main.cpp.o")
        )
        command[main_index] = str(obj)
        binary = root / "restart_driver"
        command[command.index("-o") + 1] = str(binary)
        subprocess.run(
            command, cwd=link_directory, check=True, capture_output=True, timeout=60
        )
        environment = dict(
            os.environ, QT_QPA_PLATFORM="offscreen", QTWEBENGINE_DISABLE_SANDBOX="1"
        )
        for key, folder in (
            ("XDG_CONFIG_HOME", "config"),
            ("XDG_DATA_HOME", "data"),
            ("XDG_CACHE_HOME", "cache"),
        ):
            path = root / folder
            path.mkdir(exist_ok=True)
            environment[key] = str(path)
        for mode in ("save", "button", "menu", "run-menu"):
            result = subprocess.run(
                [str(binary), mode, str(workspace)],
                cwd=workspace,
                env=environment,
                text=True,
                capture_output=True,
                timeout=35,
                check=False,
            )
            if result.returncode:
                print((result.stdout + result.stderr)[-12000:])
                return 1
            print(
                "\n".join(
                    line
                    for line in result.stdout.splitlines()
                    if line.startswith("PASS:")
                )
            )
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as error:
        for output in (error.stdout, error.stderr):
            if output:
                print(
                    output.decode(errors="replace")
                    if isinstance(output, bytes)
                    else output
                )
        sys.exit(1)
