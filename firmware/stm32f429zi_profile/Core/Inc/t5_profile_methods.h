#ifndef T5_PROFILE_METHODS_H
#define T5_PROFILE_METHODS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Optional: 3 s delay then T5_Profile_RunAll() — call from main after peripheral init. */
void T5_Profile_AfterInit(void);

/** Run all classical codec benchmarks and print T5PROFILE CSV lines over UART. */
void T5_Profile_RunAll(void);

#ifdef __cplusplus
}
#endif

#endif /* T5_PROFILE_METHODS_H */
