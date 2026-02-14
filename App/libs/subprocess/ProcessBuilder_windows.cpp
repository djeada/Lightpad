#ifdef _WIN32

#include "ProcessBuilder.hpp"

#include <stdio.h>
#include <strsafe.h>
#include <tchar.h>
#include <windows.h>

#include "environ.hpp"
#include "shell_utils.hpp"

static STARTUPINFO g_startupInfo;
static bool g_startupInfoInit = false;

static void init_startup_info() {
  if (g_startupInfoInit)
    return;
  GetStartupInfo(&g_startupInfo);
}

bool disable_inherit(subprocess::PipeHandle handle) {
  return !!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
}

namespace subprocess {

Popen ProcessBuilder::run_command(const CommandLine &command) {
  std::string program = find_program(command[0]);
  if (program.empty()) {
    throw CommandNotFoundError("command not found " + command[0]);
  }
  init_startup_info();

  Popen process;
  PipePair cin_pair;
  PipePair cout_pair;
  PipePair cerr_pair;
  PipePair closed_pair;

  SECURITY_ATTRIBUTES saAttr = {0};

  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  PROCESS_INFORMATION piProcInfo = {0};
  STARTUPINFO siStartInfo = {0};
  BOOL bSuccess = FALSE;

  siStartInfo.cb = sizeof(STARTUPINFO);
  siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

  if (cin_option == PipeOption::close) {
    cin_pair = pipe_create();
    siStartInfo.hStdInput = cin_pair.input;
    disable_inherit(cin_pair.output);
  } else if (cin_option == PipeOption::specific) {
    pipe_set_inheritable(cin_pipe, true);
    siStartInfo.hStdInput = cin_pipe;
  } else if (cin_option == PipeOption::pipe) {
    cin_pair = pipe_create();
    siStartInfo.hStdInput = cin_pair.input;
    process.cin = cin_pair.output;
    disable_inherit(cin_pair.output);
  }

  if (cout_option == PipeOption::close) {
    cout_pair = pipe_create();
    siStartInfo.hStdOutput = cout_pair.output;
    disable_inherit(cout_pair.input);
  } else if (cout_option == PipeOption::pipe) {
    cout_pair = pipe_create();
    siStartInfo.hStdOutput = cout_pair.output;
    process.cout = cout_pair.input;
    disable_inherit(cout_pair.input);
  } else if (cout_option == PipeOption::cerr) {

  } else if (cout_option == PipeOption::specific) {
    pipe_set_inheritable(cout_pipe, true);
    siStartInfo.hStdOutput = cout_pipe;
  }

  if (cerr_option == PipeOption::close) {
    cerr_pair = pipe_create();
    siStartInfo.hStdError = cerr_pair.output;
    disable_inherit(cerr_pair.input);
  } else if (cerr_option == PipeOption::pipe) {
    cerr_pair = pipe_create();
    siStartInfo.hStdError = cerr_pair.output;
    process.cerr = cerr_pair.input;
    disable_inherit(cerr_pair.input);
  } else if (cerr_option == PipeOption::cout) {
    siStartInfo.hStdError = siStartInfo.hStdOutput;
  } else if (cerr_option == PipeOption::specific) {
    pipe_set_inheritable(cerr_pipe, true);
    siStartInfo.hStdError = cerr_pipe;
  }

  if (cout_option == PipeOption::cerr) {
    siStartInfo.hStdOutput = siStartInfo.hStdError;
  }
  const char *cwd = this->cwd.empty() ? nullptr : this->cwd.c_str();
  std::string args = windows_args(command);

  void *env = nullptr;
  std::u16string envblock;
  if (!this->env.empty()) {

    envblock = create_env_block(this->env);
    env = (void *)envblock.data();
  }
  DWORD process_flags = CREATE_UNICODE_ENVIRONMENT;
  if (this->new_process_group) {
    process_flags |= CREATE_NEW_PROCESS_GROUP;
  }

  bSuccess =
      CreateProcess(program.c_str(), (char *)args.c_str(), NULL, NULL, TRUE,
                    process_flags, env, cwd, &siStartInfo, &piProcInfo);
  process.process_info = piProcInfo;
  process.pid = piProcInfo.dwProcessId;
  if (cin_pair)
    cin_pair.close_input();
  if (cout_pair)
    cout_pair.close_output();
  if (cerr_pair)
    cerr_pair.close_output();

  if (cin_option == PipeOption::close)
    cin_pair.close();
  if (cout_option == PipeOption::close)
    cout_pair.close();
  if (cerr_option == PipeOption::close)
    cerr_pair.close();

  cin_pair.disown();
  cout_pair.disown();
  cerr_pair.disown();

  process.args = command;

  if (!bSuccess)
    throw SpawnError("CreateProcess failed");
  return process;
}
} // namespace subprocess

#endif