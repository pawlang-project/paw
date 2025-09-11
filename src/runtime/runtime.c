#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

EXPORT int64_t paw_print_cstr(const char* s) {
    fputs(s, stdout);
    return 0;
}

EXPORT int64_t paw_read_file_cstr(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, (size_t)n, f);
    buf[n] = 0;
    fclose(f);
    return (int64_t)buf;
}

EXPORT void paw_exit(int code) {
    exit(code);
}
