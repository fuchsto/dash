#include <dash/util/Config.h>
#include <dash/internal/Logging.h>

#include <map>
#include <string>
#include <cstring>
#include <unistd.h>

// Environment variables as array of strings, terminated by null pointer.
extern char ** environ;

namespace dash {
namespace util {

namespace internal {

std::map<std::string, std::string> __config_values;

}

void Config::init()
{
  set("DASH_BULK_MIN_SIZE", "12M");

  int    i          = 1;
  char * env_var_kv = *environ;
  for (; env_var_kv != 0; ++i) {
    // Split into key and value:
    char * flag_name_cstr  = env_var_kv;
    char * flag_value_cstr = strstr(env_var_kv, "=");
    int    flag_name_len   = flag_value_cstr - flag_name_cstr;
    std::string flag_name(flag_name_cstr, flag_name_cstr + flag_name_len);
    std::string flag_value(flag_value_cstr+1);
    if (strstr(env_var_kv, "DASH_") == env_var_kv) {
      set(flag_name, flag_value);
    }
    env_var_kv = *(environ + i);
  }
}

void Config::set(
  const std::string & key,
  std::string         value)
{
  DASH_LOG_TRACE("util::Config::set(string,string)", key, value);
  internal::__config_values[key] = value;

  // Parse sizes from human-readable format into number of bytes,
  // e.g. 2K -> 2048.
  // Parse config keys that end in '_SIZE' with values ending in
  // 'K', 'M' or 'G'.
  std::string size_suffix = "_SIZE";
  const char * key_suffix = key.c_str()
                            + (key.length() - size_suffix.length());
  if (size_suffix == key_suffix) {
    std::vector<char> value_cstr(value.begin(), value.end());
    value_cstr.push_back('\0');
    char * value_cstr_end = &value_cstr[0] + value.length();
    int    sh = 0;
    errno = 0;
    auto   value_bytes = strtoull(value.c_str(), &value_cstr_end, 10);
    if (!errno && value_cstr_end != value.c_str()) {
      switch(*value_cstr_end) {
        case 'K': sh = 10; break;
        case 'M': sh = 20; break;
        case 'G': sh = 30; break;
        case 0:   sh = 0;  break;
        default:  sh = -1; break;
      }
      if (sh > 0 && value_bytes < SIZE_MAX >> sh) {
        value_bytes <<= sh;
      }
    }
    std::string key_name_bytes = key + "_BYTES";
    set(key_name_bytes, value_bytes);
  }
}

template<>
std::string Config::get<std::string>(
  const std::string & key)
{
  return get_str(key);
}

template<>
bool Config::get<bool>(
  const std::string & key)
{
  return atoi(get_str(key).c_str()) == 1;
}


} // namespace util
} // namespace dash