#ifndef QIWO_TEST_STUB_RIME_API_H_
#define QIWO_TEST_STUB_RIME_API_H_

typedef int Bool;

#ifndef True
#define True 1
#endif

#ifndef False
#define False 0
#endif

typedef struct {
  int reserved;
} RimeConfig;

typedef struct {
  Bool (*config_open)(const char *config_id, RimeConfig *config);
  Bool (*config_get_bool)(RimeConfig *config, const char *key, Bool *value);
  const char *(*config_get_cstring)(RimeConfig *config, const char *key);
  Bool (*config_get_int)(RimeConfig *config, const char *key, int *value);
  void (*config_close)(RimeConfig *config);
} RimeApi;

#endif  // QIWO_TEST_STUB_RIME_API_H_
