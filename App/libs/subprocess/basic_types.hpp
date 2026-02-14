#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <csignal>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace subprocess {

typedef intptr_t ssize_t;

#ifdef _WIN32

constexpr bool kIsWin32 = true;
#else
constexpr bool kIsWin32 = false;
#endif

enum SigNum {
  PSIGHUP = 1,
  PSIGINT = SIGINT,
  PSIGQUIT = 3,
  PSIGILL = SIGILL,
  PSIGTRAP = 5,
  PSIGABRT = SIGABRT,
  PSIGIOT = 6,
  PSIGBUS = 7,
  PSIGFPE = SIGFPE,
  PSIGKILL = 9,
  PSIGUSR1 = 10,
  PSIGSEGV = SIGSEGV,
  PSIGUSR2 = 12,
  PSIGPIPE = 13,
  PSIGALRM = 14,
  PSIGTERM = SIGTERM,
  PSIGSTKFLT = 16,
  PSIGCHLD = 17,
  PSIGCONT = 18,
  PSIGSTOP = 19,
  PSIGTSTP = 20,
  PSIGTTIN = 21,
  PSIGTTOU = 22,
  PSIGURG = 23,
  PSIGXCPU = 24,
  PSIGXFSZ = 25,
  PSIGVTALRM = 26,
  PSIGPROF = 27,
  PSIGWINCH = 28,
  PSIGIO = 29
};
#ifndef _WIN32
typedef int PipeHandle;
typedef ::pid_t pid_t;

constexpr char kPathDelimiter = ':';

const PipeHandle kBadPipeValue = (PipeHandle)-1;
#else
typedef HANDLE PipeHandle;
typedef DWORD pid_t;

constexpr char kPathDelimiter = ';';
const PipeHandle kBadPipeValue = INVALID_HANDLE_VALUE;
#endif
constexpr int kStdInValue = 0;
constexpr int kStdOutValue = 1;
constexpr int kStdErrValue = 2;

constexpr int kBadReturnCode = -1000;

typedef std::vector<std::string> CommandLine;
typedef std::map<std::string, std::string> EnvMap;

enum class PipeOption : int {
  inherit,
  cout,
  cerr,

  specific,
  pipe,
  close
};

struct SubprocessError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct OSError : SubprocessError {
  using SubprocessError::SubprocessError;
};

struct CommandNotFoundError : SubprocessError {
  using SubprocessError::SubprocessError;
};

struct SpawnError : OSError {
  using OSError::OSError;
};

struct TimeoutExpired : SubprocessError {
  using SubprocessError::SubprocessError;

  CommandLine command;

  double timeout;

  std::string cout;

  std::string cerr;
};

struct CalledProcessError : SubprocessError {
  using SubprocessError::SubprocessError;

  int returncode;

  CommandLine cmd;

  std::string cout;

  std::string cerr;
};

struct CompletedProcess {

  CommandLine args;

  int returncode = -1;

  std::string cout;

  std::string cerr;
  explicit operator bool() const { return returncode == 0; }
};

namespace details {
void throw_os_error(const char *function, int errno_code);
}
} // namespace subprocess