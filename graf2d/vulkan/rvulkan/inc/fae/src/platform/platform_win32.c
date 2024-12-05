#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef near
#undef far

internal
i32 os_page_size()
{
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return (i32)info.dwPageSize;
}

internal 
void *os_reserve(u64 size)
{
  void *mem = VirtualAlloc(0, (SIZE_T)size, MEM_RESERVE, PAGE_READWRITE);
  if (UNLIKELY(mem == NULL)) {
    ERR("Error reserving memory: %s\n", GetLastError());
  }
  return mem;
}

internal 
void os_release(void *mem, u64 size)
{
  VirtualFree(mem, 0, MEM_RELEASE);
}

internal 
b32x os_commit(void *addr, u64 size)
{
  void *mem = VirtualAlloc(addr, (SIZE_T)size, MEM_COMMIT, PAGE_READWRITE);
  if (UNLIKELY(mem == NULL)) {
    ERR("Error committing memory: %s)\n", GetLastError());
  }
  return mem != NULL;
}

FAE_DECLSPEC(noreturn)
internal
void os_exit(i32 status)
{
  ExitProcess(status);
}

FAE_DECLSPEC(noreturn)
internal
void os_abort()
{
  ExitProcess(1);
}

internal
const char *os_get_exe_path()
{
  static char exe_path[1024] = {};

  if (exe_path[0] != 0)
    return exe_path;

  GetModuleFileNameA(NULL, exe_path, countof(exe_path) - 1);

  return exe_path;
}

internal
u64 os_clock_time_ns()
{
  static LARGE_INTEGER freq;
  static f64 inv_freq;
  if (!freq.QuadPart) {
    QueryPerformanceFrequency(&freq);
    inv_freq = 1e9 / freq.QuadPart;
  }

  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (u64)(now.QuadPart * inv_freq);
}

internal
void os_sleep_ms(u32 ms)
{
  Sleep(ms);
}

internal
void os_trap()
{
  DebugBreak();
}
