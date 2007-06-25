#ifndef __DLL_LOAD_H__
#define __DLL_LOAD_H__


#define DLL_DECLARE(api, ret, name, args)                    \
  typedef ret (api * __dll_##name##_t)args; __dll_##name##_t name

#define DLL_LOAD(dll, name, ret_on_failure)                   \
  do {                                                        \
  HMODULE h = GetModuleHandle(#dll);                          \
  if(!h)                                                      \
    h = LoadLibrary(#dll);                                    \
  if(!h) {                                                    \
    if(ret_on_failure)                                        \
      return USBI_STATUS_UNKNOWN;                             \
    else break; }                                             \
  if((name = (__dll_##name##_t)GetProcAddress(h, #name)))     \
    break;                                                    \
  if((name = (__dll_##name##_t)GetProcAddress(h, #name "A"))) \
    break;                                                    \
  if((name = (__dll_##name##_t)GetProcAddress(h, #name "W"))) \
    break;                                                    \
  if(ret_on_failure)                                          \
    return USBI_STATUS_UNKNOWN;                               \
  } while(0)


#endif
