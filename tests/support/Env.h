#pragma once

#include <cstdlib>
#include <string>

// setenv/unsetenv are POSIX; MSVC has _putenv_s, where an empty value clears.
namespace tdtest {

inline void setEnv(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

inline void unsetEnv(const char* name) {
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

}  // namespace tdtest
