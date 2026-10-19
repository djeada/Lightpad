"""Named workers with distinct locals; alpha releases beta after inspection."""
import threading

beta_ready = threading.Event()
release = threading.Event()


def worker(name):
    payload = {"worker": name, "matrix": [[1, 2], [3, 4]]}
    if name == "beta":
        beta_ready.set()
        # Return to Python periodically so debugpy can suspend this worker.
        while not release.wait(0.05):
            pass
    else:
        assert beta_ready.wait(30)
        inspection_marker = payload["worker"]  # THREAD_INSPECT
        release.set()
    print("thread-debug-ok", payload["worker"], flush=True)


if __name__ == "__main__":
    workers = [threading.Thread(target=worker, args=(name,), name=name) for name in ("alpha", "beta")]
    for thread in workers:
        thread.start()
    for thread in workers:
        thread.join()
