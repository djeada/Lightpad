"""Inspect waiting coroutine frames, then cancel one task and finish the other."""
import asyncio


async def worker(name, gate):
    payload = {"worker": name, "matrix": [[1, 2], [3, 4]]}
    await gate.wait()
    return payload["worker"]


async def main():
    asyncio.current_task().set_name("driver")
    gate = asyncio.Event()
    reader = asyncio.create_task(worker("reader", gate), name="io-reader")
    writer = asyncio.create_task(worker("writer", gate), name="io-writer")
    await asyncio.sleep(0)
    inspection_marker = "tasks-ready"  # ASYNC_INSPECT
    writer.cancel()
    gate.set()
    results = await asyncio.gather(reader, writer, return_exceptions=True)
    assert results[0] == "reader"
    assert isinstance(results[1], asyncio.CancelledError)
    print("async-debug-ok", results[0], flush=True)


if __name__ == "__main__":
    asyncio.run(main())
