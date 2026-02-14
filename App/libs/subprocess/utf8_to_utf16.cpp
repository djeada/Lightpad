#include "utf8_to_utf16.hpp"

#include <codecvt>
#include <locale>

#ifdef _WIN32
#include <windows.h>
#endif

#include <assert.h>

namespace subprocess {

#ifndef _WIN32
std::u16string utf8_to_utf16(const std::string &str) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  std::u16string dest = convert.from_bytes(str);
  return dest;
}
std::string utf16_to_utf8(const std::u16string &str) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  std::string dest = convert.to_bytes(str);
  return dest;
}

std::wstring utf8_to_utf16_w(const std::string &str) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
  std::wstring dest = convert.from_bytes(str);
  return dest;
}

std::string utf16_to_utf8(const std::wstring &str) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> convert;
  std::string dest = convert.to_bytes(str);
  return dest;
}
#endif

#if 0
    
    std::u16string utf8_to_utf16(const std::string& str) {
        std::wstring result;
        for (int i = 0; i < str.size(); ++i) {
            if (str[i] <= 0x7F) {
                
                char16_t ch = str[i];
                result += ch;
                continue;
            }
            uint32_t code = 0;
            uint_t nbytes = str[i];
            

            if ((nbytes & 0xF8) == 0xF0) {
                nbytes = 4;
                code = str[i] & 0x07;
                code <<= 6;
                code |= str[i+1] & 0x3F;
                code <<= 6;
                code |= str[i+2] & 0x3F;
                code <<= 6;
                code |= str[i+3] & 0x3F;
                i += 3;
            } else if ((nbytes & 0xF0) == 0xD0) {
                nbytes = 3;
                code = str[i] & 0x0F;
                code <<= 6;
                code |= str[i+1] & 0x3F;
                code <<= 6;
                code |= str[i+2] & 0x3F;
                i += 2;
            } else if ((nbytes & 0xD0) == 0xC0) {
                nbytes = 2;
                code = str[i] & 0x1F;
                code <<= 6;
                code |= str[i+1] & 0x3F
                i += 1;
            } else {
                std::abort();
                return "";
            }

            
            if (code <= 0xD7FF) {

            }
            uint32_t parts[4] = {0};
            parts[0] = str[i];

        }
    }
#endif

#ifdef _WIN32

std::u16string utf8_to_utf16(const std::string &string) {
  static_assert(sizeof(wchar_t) == 2, "wchar_t must be of size 2");
  static_assert(sizeof(wchar_t) == sizeof(char16_t),
                "wchar_t must be of size 2");
  int size = string.size() + 1;

  int r = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, string.c_str(),
                              size, NULL, 0);
  if (r == 0) {
    return std::u16string();
  }
  assert(r > 0);

  wchar_t *wstring = new wchar_t[r];
  if (wstring == NULL) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return NULL;
  }

  r = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), size, wstring, r);
  if (r == 0) {
    delete[] wstring;
    return NULL;
  }
  std::u16string result((char16_t *)wstring, r - 1);
  delete[] wstring;
  return result;
}

#ifdef __MINGW32__

constexpr int WC_ERR_INVALID_CHARS = 0;
#endif

std::string utf16_to_utf8(const std::u16string &wstring) {
  int size = wstring.size() + 1;

  int r = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                              (wchar_t *)wstring.c_str(), size, NULL, 0, NULL,
                              NULL);

  if (r == 0) {
    return "";
  }
  assert(r > 0);

  char *string = new char[r];
  if (string == nullptr) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return NULL;
  }

  r = WideCharToMultiByte(CP_UTF8, 0, (wchar_t *)wstring.c_str(), size, string,
                          r, NULL, NULL);
  if (r == 0) {
    delete[] string;
    return NULL;
  }
  std::string result(string, r - 1);
  delete[] string;
  return result;
}

std::string utf16_to_utf8(const std::wstring &wstring) {
  int size = wstring.size() + 1;

  int r = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                              (wchar_t *)wstring.c_str(), size, NULL, 0, NULL,
                              NULL);

  if (r == 0) {
    return "";
  }
  assert(r > 0);

  char *string = new char[r];
  if (string == nullptr) {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return NULL;
  }

  r = WideCharToMultiByte(CP_UTF8, 0, (wchar_t *)wstring.c_str(), size, string,
                          r, NULL, NULL);
  if (r == 0) {
    delete[] string;
    return NULL;
  }
  std::string result(string, r - 1);
  delete[] string;
  return result;
}

size_t strlen16(char16_t *str) {
  size_t size = 0;
  for (; *str; ++str)
    ++size;
  return size;
}
size_t strlen16(wchar_t *str) {
  size_t size = 0;
  for (; *str; ++str)
    ++size;
  return size;
}

template <typename T> std::string lptstr_to_string_t(T str) {
  if (str == nullptr)
    return "";
  if constexpr (sizeof(*str) == 1) {
    static_assert(sizeof(*str) == 1);
    return (const char *)str;
  } else {
    static_assert(sizeof(*str) == 2);
    return utf16_to_utf8((const wchar_t *)str);
  }
}

std::string lptstr_to_string(LPTSTR str) { return lptstr_to_string_t(str); }
#endif
} // namespace subprocess