#ifndef LIBNODE
#define LIBNODE 1

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WIN32) || defined(_WIN32)
	#define EXPORT __declspec(dllexport)
#else
	#define EXPORT __attribute__((visibility("default")))
#endif

typedef void (*free_ptr)(void *);

EXPORT char *string_function(void);

EXPORT free_ptr random_function_for_compiling_with_libc_as_dependency(void);

#ifdef __cplusplus
}
#endif

#endif
