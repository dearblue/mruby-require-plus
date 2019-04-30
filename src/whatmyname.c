#ifdef MATERIALIZE_WHATMYNAME

/* References:
 *  - How to get executable path on FreeBSD? - Ars Technica OpenForum
 *    https://arstechnica.com/civis/viewtopic.php?t=433790
 *  - c++ - Finding current executable's path without /proc/self/exe - Stack Overflow
 *    https://stackoverflow.com/questions/1023306
 *  - c - Programmatically retrieving the absolute path of an OS X command-line app - Stack Overflow
 *    https://stackoverflow.com/questions/799679
 *  - C/C++ tip: How to detect the operating system type using compiler predefined macros | Nadeau Software
 *    http://nadeausoftware.com/articles/2012/01/c_c_tip_how_use_compiler_predefined_macros_detect_operating_system
 */

#if defined(__FreeBSD__) || defined(__DragonFly__)
# include <sys/types.h>   /* for sysctl() */
# include <sys/sysctl.h>  /* for sysctl() */
# define HAVE_SYSCTL_KERN_PROC_PATHNAME CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1
#elif defined(__NetBSD__)
  // CHECK ME...
//# include <unistd.h>
//# define HAVE_READLINK_WITH_PROCFS "/proc/curproc/exe"
# include <sys/types.h>   /* for sysctl() */
# include <sys/sysctl.h>  /* for sysctl() */
# define HAVE_SYSCTL_KERN_PROC_PATHNAME CTL_KERN, KERN_PROC, -1, KERN_PROC_PATHNAME
#elif defined(__OpenBSD__)
  // IMPLEMENT HERE...
#elif defined(__linux__)
  // CHECK ME...
# include <unistd.h>
# define HAVE_READLINK_WITH_PROCFS "/proc/self/exe"
#elif defined(_WIN32) || defined(_WIN64)
  // CHECK ME...
# include <windows.h>
# define HAVE_GETMODULEFILENAME 1
#elif defined(__APPLE__)
  // CHECK ME...
# include <mach-o/dyld.h> /* for _NSGetExecutablePath() */
# define HAVE_NSGETEXECUTABLEPATH 1
#else
  // IMPLEMENT HERE...
#endif

/*
 * 実行ファイルのパスを取得する
 */
static int
whatmyname(void *buf, size_t size)
{
#if defined(HAVE_SYSCTL_KERN_PROC_PATHNAME)
  int mib[] = { HAVE_SYSCTL_KERN_PROC_PATHNAME };
  size_t len = size;
  if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), buf, &len, NULL, 0) == 0) {
    len --; /* 'NUL' が含まれているため除去 */
    return len;
  }
#elif define(HAVE_READLINK_WITH_PROCFS)
  ssize_t ret;
  if ((ret = readlink(HAVE_READLINK_WITH_PROCFS, buf, size)) > 0) {
    return ret;
  }
#elif defined(HAVE_GETMODULEFILENAME)
  DWORD ret = GetModuleFileName(NULL, buf, size);
  if (ret > 0 && ret < size) {
    return ret;
  }
#elif defined(HAVE_NSGETEXECUTABLEPATH)
  uint32_t len = size;
  if (_NSGetExecutablePath(buf, &len) == 0) {
    return len;
  }
#endif

  return -1;
}

#endif /* MATERIALIZE_WHATMYNAME */

void dummy_whatmyname_function(void);
