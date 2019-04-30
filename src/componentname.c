#ifdef MATERIALIZE_COMPONENTNAME

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

//#define DEBUG_FORCE_WITH_WINDOWS_CODE

#if !defined(__CYGWIN__) && (defined(_WIN32) || defined(_WIN64)) || defined(DEBUG_FORCE_WITH_WINDOWS_CODE)
# define WITHIN_WINDOWS_CODE
# define CASE_PATH_SEPARATOR '/': case '\\'
#else
# define CASE_PATH_SEPARATOR '/'
#endif

static inline bool ispathsep(int ch);
static bool needpathsep(const char *path, size_t len);
static void splitpath(const char *path, size_t len, const char **rootterm, const char **dirterm, const char **basename, const char **extname, const char **nameterm);

static inline bool
ispathsep(int ch)
{
  switch (ch) {
  case CASE_PATH_SEPARATOR:
    return true;
  default:
    return false;
  }
}

static const char *
skip_root_component(const char *path, const uintptr_t end)
{
#ifdef WITHIN_WINDOWS_CODE
  if (end - (uintptr_t)path >= 2) {
    if (ispathsep(path[0]) && ispathsep(path[1])) {
      path += 2;

      /* 連続したパス区切り文字を読み飛ばす */
      for (; (uintptr_t)path < end; path ++) {
        if (!ispathsep(*path)) {
          break;
        }
      }

      for (; (uintptr_t)path < end; path ++) {
        if (*path == '\0') {
          break;
        } else if (ispathsep(*path)) {
          path ++;
          break;
        }
      }

      return path;
    } else if (isalpha((uint8_t)path[0]) && path[1] == ':') {
      path += 2;
    }
  }
#endif

  for (; (uintptr_t)path < end; path ++) {
    if (!ispathsep(path[0])) {
      break;
    }
  }

  return path;
}

static const char *
skip_separator(const char *path, uintptr_t end)
{
  for (; (uintptr_t)path < end; path ++) {
    if (!ispathsep(*path)) {
      break;
    }
  }

  return path;
}

static const char *
skip_leading_dot(const char *path, const uintptr_t end)
{
  for (; (uintptr_t)path < end && *path == '.'; path ++)
    ;

  return path;
}

static const char *
scan_component_name(const char *path, const uintptr_t end, const char **extname, const char **nameterm)
{
  for (; (uintptr_t)path < end; path ++) {
    if (*path == '\0' || ispathsep(*path)) {
      break;
    } else if (*path == '.') {
      *extname = path;
    }
  }

  if (*extname && path - *extname < 2) {
    *extname = path;
  }
  *nameterm = path;

  return path;
}

static void
splitpath(const char *path, size_t len, const char **rootterm, const char **dirterm, const char **basename, const char **extname, const char **nameterm)
{
  const uintptr_t end = (uintptr_t)path + len;

  *basename = path;
  path = skip_root_component(path, end);
  *rootterm = *dirterm = *extname = *nameterm = path;

  for (; (uintptr_t)path < end && *path != '\0'; path ++) {
    path = skip_separator(path, end);
    if (path == NULL) { break; }

    *dirterm = *nameterm;
    *basename = path;
    *extname = path;

    path = skip_leading_dot(path, end);
    path = scan_component_name(path, end, extname, nameterm);
    if (*extname == *basename) { *extname = path; }
  }
}

static bool
needpathsep(const char *path, size_t len)
{
  if (len == 0) { return true; }

  const uintptr_t end = (uintptr_t)path + len;
  path = skip_root_component(path, end);

  return !((uintptr_t)path == end ||
           ispathsep(((const char *)end)[-1]));
}

#endif /* MATERIALIZE_COMPONENTNAME */

void dummy_componentname_function(void);
