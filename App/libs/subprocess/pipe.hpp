#pragma once

#include "basic_types.hpp"

namespace subprocess {

struct PipePair {
  PipePair(){};
  PipePair(PipeHandle input, PipeHandle output)
      : input(input), output(output) {}
  ~PipePair() { close(); }

  PipePair(const PipePair &) = delete;
  PipePair &operator=(const PipePair &) = delete;
  PipePair(PipePair &&other) { *this = std::move(other); }
  PipePair &operator=(PipePair &&other);

  const PipeHandle input = kBadPipeValue;
  const PipeHandle output = kBadPipeValue;

  void disown() {
    const_cast<PipeHandle &>(input) = const_cast<PipeHandle &>(output) =
        kBadPipeValue;
  }
  void close();
  void close_input();
  void close_output();
  explicit operator bool() const noexcept { return input != output; }
};

bool pipe_close(PipeHandle handle);

PipePair pipe_create(bool inheritable = true);

void pipe_set_inheritable(PipeHandle handle, bool inheritable);

ssize_t pipe_read(PipeHandle, void *buffer, size_t size);

ssize_t pipe_write(PipeHandle, const void *buffer, size_t size);

void pipe_ignore_and_close(PipeHandle handle);

std::string pipe_read_all(PipeHandle handle);
} // namespace subprocess