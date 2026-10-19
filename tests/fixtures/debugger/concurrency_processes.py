"""Spawn/fork workers must each receive their own debug session and breakpoint."""
import multiprocessing
import os


def worker(number):
    payload = {"worker": number, "matrix": [[number, 2], [3, 4]], "pid": os.getpid()}
    for phase in range(2):
        inspection_marker = payload["worker"]  # PROCESS_INSPECT
        print(f"process-debug-ok {number} {phase}", flush=True)


if __name__ == "__main__":
    context = multiprocessing.get_context(os.environ.get("LIGHTPAD_MP_METHOD", "spawn"))
    workers = [context.Process(target=worker, args=(number,), name=f"worker-{number}") for number in (1, 2)]
    for process in workers:
        process.start()
    for process in workers:
        # Poll so a debugger pause can suspend the supervising Python frame.
        while process.is_alive():
            process.join(timeout=0.05)
        assert process.exitcode == 0, process.exitcode
    print("parent-debug-ok", flush=True)
