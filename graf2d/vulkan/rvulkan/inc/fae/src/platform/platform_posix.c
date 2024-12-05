#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

internal
i32 os_page_size()
{
  return sysconf(_SC_PAGESIZE);
}

internal 
void *os_reserve(u64 size)
{
  void *mem = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  if (UNLIKELY(mem == MAP_FAILED)) {
    ERR("Error reserving memory: %s (%d)\n", strerror(errno), errno);
  }
  return mem;
}

internal 
void os_release(void *mem, u64 size)
{
  munmap(mem, size);
}

internal 
b32x os_commit(void *addr, u64 size)
{
  b32x err = mprotect(addr, size, PROT_READ|PROT_WRITE);
  if (UNLIKELY(err)) {
    ERR("Error committing memory: %s (%d)\n", strerror(errno), errno);
  }
  return err == 0;
}

internal
void os_exit(i32 status)
{
  _exit(status);
}

internal
void os_abort()
{
  os_exit(1);
}

internal
const char *os_get_exe_path()
{
  static char exe_path[1024] = {};

  if (exe_path[0] != 0)
    return exe_path;

  i32 res = access("/proc/self/exe", F_OK);
  if (res == -1) {
    ERR("Failed to determine executable path: %s\n", strerror(errno));
    return "";
  }

  ssize_t bytes = readlink("/proc/self/exe", exe_path, countof(exe_path) - 1);
  if (bytes == -1) {
    ERR("Failed to determine executable path: %s\n", strerror(errno));
    return "";
  }
  exe_path[bytes] = 0;

  return exe_path;
}

internal
u64 os_clock_time_ns()
{
  struct timespec tspec;
  clock_gettime(CLOCK_MONOTONIC, &tspec);
  return tspec.tv_sec * 1e9 + tspec.tv_nsec;
}

internal
void os_sleep_ms(u32 ms)
{
  struct timespec sleep_duration;
  sleep_duration.tv_sec = 0;
  sleep_duration.tv_nsec = ms * 1e6;
  nanosleep(&sleep_duration, NULL);
}

internal
void os_trap()
{
  raise(SIGTRAP);
}
