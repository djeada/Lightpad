#pragma once

#include <initializer_list>
#include <string>
#include <vector>

#include "PipeVar.hpp"
#include "pipe.hpp"

namespace subprocess {

struct RunOptions {

  bool check = false;

  PipeVar cin = PipeOption::inherit;

  PipeVar cout = PipeOption::inherit;

  PipeVar cerr = PipeOption::inherit;

  bool new_process_group = false;

  std::string cwd;

  EnvMap env;

  double timeout = -1;
};
class ProcessBuilder;

struct Popen {
public:
  Popen() {}

  Popen(CommandLine command, const RunOptions &options);

  Popen(CommandLine command, RunOptions &&options);
  Popen(const Popen &) = delete;
  Popen &operator=(const Popen &) = delete;

  Popen(Popen &&);
  Popen &operator=(Popen &&);

  ~Popen();

  PipeHandle cin = kBadPipeValue;

  PipeHandle cout = kBadPipeValue;

  PipeHandle cerr = kBadPipeValue;

  pid_t pid = 0;

  int returncode = kBadReturnCode;
  CommandLine args;

  void ignore_cout() {
    pipe_ignore_and_close(cout);
    cout = kBadPipeValue;
  }

  void ignore_cerr() {
    pipe_ignore_and_close(cerr);
    cerr = kBadPipeValue;
  }

  void ignore_output() {
    ignore_cout();
    ignore_cerr();
  }

  bool poll();

  int wait(double timeout = -1);

  bool send_signal(int signal);

  bool terminate();

  bool kill();

  void close();

  void close_cin() {
    if (cin != kBadPipeValue) {
      pipe_close(cin);
      cin = kBadPipeValue;
    }
  }
  friend ProcessBuilder;

private:
  void init(CommandLine &command, RunOptions &options);

#ifdef _WIN32
  PROCESS_INFORMATION process_info;
#endif
};

class ProcessBuilder {
public:
  std::vector<PipeHandle> child_close_pipes;

  PipeHandle cin_pipe = kBadPipeValue;
  PipeHandle cout_pipe = kBadPipeValue;
  PipeHandle cerr_pipe = kBadPipeValue;

  PipeOption cin_option = PipeOption::inherit;
  PipeOption cout_option = PipeOption::inherit;
  PipeOption cerr_option = PipeOption::inherit;

  bool new_process_group = false;

  EnvMap env;
  std::string cwd;
  CommandLine command;

  std::string windows_command();
  std::string windows_args();
  std::string windows_args(const CommandLine &command);

  Popen run() { return run_command(this->command); }
  Popen run_command(const CommandLine &command);
};

CompletedProcess run(Popen &popen, bool check = false);

CompletedProcess run(CommandLine command, RunOptions options = {});

struct RunBuilder {
  RunOptions options;
  CommandLine command;

  RunBuilder() {}

  RunBuilder(CommandLine cmd) : command(cmd) {}

  RunBuilder(std::initializer_list<std::string> command) : command(command) {}

  RunBuilder &check(bool ch) {
    options.check = ch;
    return *this;
  }

  RunBuilder &cin(const PipeVar &cin) {
    options.cin = cin;
    return *this;
  }

  RunBuilder &cout(const PipeVar &cout) {
    options.cout = cout;
    return *this;
  }

  RunBuilder &cerr(const PipeVar &cerr) {
    options.cerr = cerr;
    return *this;
  }

  RunBuilder &cwd(std::string cwd) {
    options.cwd = cwd;
    return *this;
  }

  RunBuilder &env(const EnvMap &env) {
    options.env = env;
    return *this;
  }

  RunBuilder &timeout(double timeout) {
    options.timeout = timeout;
    return *this;
  }

  RunBuilder &new_process_group(bool new_group) {
    options.new_process_group = new_group;
    return *this;
  }
  operator RunOptions() const { return options; }

  CompletedProcess run() { return subprocess::run(command, options); }

  Popen popen() { return Popen(command, options); }
};

double monotonic_seconds();

double sleep_seconds(double seconds);

class StopWatch {
public:
  StopWatch() { start(); }

  void start() { mStart = monotonic_seconds(); }
  double seconds() const { return monotonic_seconds() - mStart; }

private:
  double mStart;
};
} // namespace subprocess