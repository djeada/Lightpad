"""Lightpad's dependency-free pytest reporter. Loaded only by IDE test runs."""

import inspect
import json
import os
import re
import sys

PREFIX = "@@LIGHTPAD_PYTEST@@"
_tests = {}
_reports = {}
_reporting = os.environ.get("LIGHTPAD_PYTEST_REPORTING", "1") != "0"


def _emit(event, **values):
    if not _reporting:
        return
    # A record always starts on a fresh line, even beside pytest's progress text.
    sys.__stdout__.write("\n" + PREFIX + json.dumps({"event": event, **values}) + "\n")
    sys.__stdout__.flush()


def pytest_collection_modifyitems(config, items):
    """Honor an IDE selection even when the configuration also names a directory."""
    selection = json.loads(os.environ.get("LIGHTPAD_PYTEST_SELECTION", "[]"))
    if not selection:
        return

    def key(nodeid):
        path, separator, name = nodeid.partition("::")
        if not os.path.isabs(path):
            path = os.path.join(str(config.rootpath), path)
        return os.path.normcase(os.path.realpath(path)) + separator + name

    targets = [key(target) for target in selection]
    selected, deselected, seen = [], [], set()
    for item in items:
        identity = key(item.nodeid)
        matches = any(identity == target or identity.startswith(target + "::")
                      or identity.startswith(target + os.sep) for target in targets)
        if matches and identity not in seen:
            selected.append(item)
            seen.add(identity)
        else:
            deselected.append(item)
    items[:] = selected
    config.hook.pytest_deselected(items=deselected)


def _metadata(item):
    path, line, _ = item.location
    source = os.path.abspath(str(path))
    line += 1
    try:
        obj = inspect.unwrap(item.obj)
        source = os.path.abspath(inspect.getsourcefile(obj) or source)
        lines, first = inspect.getsourcelines(obj)
        definition = re.compile(r"^\s*(?:async\s+)?def\s+" + re.escape(obj.__name__) + r"\s*\(")
        line = next((first + i for i, text in enumerate(lines) if definition.match(text)), first)
    except (AttributeError, OSError, TypeError):
        pass
    return {"id": item.nodeid, "name": item.nodeid.split("::", 1)[-1],
            "file": source, "line": line}


def pytest_collection_finish(session):
    if not _reporting:
        return
    _emit("session", total=len(session.items))
    for item in session.items:
        metadata = _metadata(item)
        _tests[item.nodeid] = metadata
        _emit("collected", **metadata)


def pytest_runtest_logstart(nodeid, location):
    _emit("started", **_tests.get(nodeid, {"id": nodeid, "name": nodeid.split("::", 1)[-1]}))


def pytest_runtest_logreport(report):
    if not _reporting:
        return
    reports = _reports.setdefault(report.nodeid, [])
    reports.append(report)
    if report.when != "teardown":
        return
    metadata = _tests.get(report.nodeid, {"id": report.nodeid, "name": report.nodeid.split("::", 1)[-1]})
    failures = [item for item in reports if item.failed]
    skips = [item for item in reports if item.skipped]
    if failures:
        status = "error" if any(item.when != "call" for item in failures) else "failed"
    elif skips:
        status = "skipped"
    else:
        status = "passed"
    details = "\n\n".join(str(item.longrepr) for item in failures + skips)
    stdout, stderr = [], []
    for item in reports:
        for title, text in item.sections:
            target = stderr if "stderr" in title.lower() else stdout
            if text and text not in target:
                target.append(text)
    _emit("finished", **metadata, status=status,
          durationMs=round(sum(item.duration for item in reports) * 1000),
          message=details.splitlines()[-1] if details else "",
          traceback=details, stdout="\n".join(stdout), stderr="\n".join(stderr))
    _reports.pop(report.nodeid, None)
