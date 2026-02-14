#pragma once

#include <string>
#include <vector>

#include "basic_types.hpp"
#include "shell_utils.hpp"

namespace subprocess {
class EnvironSetter {
public:
  EnvironSetter(const std::string &name);
  operator std::string() { return to_string(); }
  explicit operator bool() const;
  std::string to_string();
  EnvironSetter &operator=(const std::string &str);
  EnvironSetter &operator=(const char *str);
  EnvironSetter &operator=(std::nullptr_t) { return *this = (const char *)0; }
  EnvironSetter &operator=(int value);
  EnvironSetter &operator=(bool value);
  EnvironSetter &operator=(float value);

private:
  std::string mName;
};

class Environ {
public:
  EnvironSetter operator[](const std::string &);
};

extern Environ cenv;

EnvMap current_env_copy();

std::u16string create_env_block(const EnvMap &map);

class CwdGuard {
public:
  CwdGuard() { mCwd = subprocess::getcwd(); }
  ~CwdGuard() { subprocess::setcwd(mCwd); }

private:
  std::string mCwd;
};

class EnvGuard : public CwdGuard {
public:
  EnvGuard() { mEnv = current_env_copy(); }
  ~EnvGuard();

private:
  EnvMap mEnv;
};
} // namespace subprocess