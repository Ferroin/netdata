# Macros covering basic build environment checks.
#
# Everything checked by these functions should be non-mandatory for the
# build. They should just be used to select implementation details based
# on the build environment.
#
# Copyright (c) 2024 Netdata Inc.
# SPDX-License-Identifier: GPL-3.0-or-later

macro(netdata_basic_include_checks)
        include(CheckIncludeFile)

        check_include_file("netinet/in.h" HAVE_NETINET_IN_H)
        check_include_file("resolv.h" HAVE_RESOLV_H)
        check_include_file("netdb.h" HAVE_NETDB_H)
        check_include_file("sys/prctl.h" HAVE_SYS_PRCTL_H)
        check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
        check_include_file("sys/vfs.h" HAVE_SYS_VFS_H)
        check_include_file("sys/statfs.h" HAVE_SYS_STATFS_H)
        check_include_file("linux/magic.h" HAVE_LINUX_MAGIC_H)
        check_include_file("sys/mount.h" HAVE_SYS_MOUNT_H)
        check_include_file("sys/statvfs.h" HAVE_SYS_STATVFS_H)
        check_include_file("inttypes.h" HAVE_INTTYPES_H)
        check_include_file("stdint.h" HAVE_STDINT_H)
        check_include_file("sys/capability.h" HAVE_SYS_CAPABILITY_H)
endmacro()

macro(netdata_basic_symbol_checks)
        include(CheckSymbolExists)

        check_symbol_exists(major "sys/sysmacros.h" MAJOR_IN_SYSMACROS)
        check_symbol_exists(major "sys/mkdev.h" MAJOR_IN_MKDEV)
        check_symbol_exists(clock_gettime "time.h" HAVE_CLOCK_GETTIME)
        check_symbol_exists(strerror_r "string.h" HAVE_STRERROR_R)
        check_symbol_exists(finite "math.h" HAVE_FINITE)
        check_symbol_exists(isfinite "math.h" HAVE_ISFINITE)
        check_symbol_exists(dlsym "dlfcn.h" HAVE_DLSYM)
endmacro()

macro(netdata_basic_function_checks)
        include(CheckFunctionExists)

        check_function_exists(nice HAVE_NICE)
        check_function_exists(recvmmsg HAVE_RECVMMSG)
        check_function_exists(getpriority HAVE_GETPRIORITY)
        check_function_exists(sched_getscheduler HAVE_SCHED_GETSCHEDULER)
        check_function_exists(sched_setscheduler HAVE_SCHED_SETSCHEDULER)
        check_function_exists(sched_get_priority_min HAVE_SCHED_GET_PRIORITY_MIN)
        check_function_exists(sched_get_priority_max HAVE_SCHED_GET_PRIORITY_MAX)
        check_function_exists(close_range HAVE_CLOSE_RANGE)
        check_function_exists(backtrace HAVE_BACKTRACE)
endmacro()

macro(netdata_basic_source_checks)
        include(CheckCSourceCompiles)
        include(CheckCXXSourceCompiles)

        set(CMAKE_REQUIRED_LIBRARIES pthread)

check_c_source_compiles("
#define _GNU_SOURCE
#include <pthread.h>
int main() {
        char name[16];
        pthread_t thread = pthread_self();
        return pthread_getname_np(thread, name, sizeof(name));
}
" HAVE_PTHREAD_GETNAME_NP)

check_c_source_compiles("
#include <stdio.h>
#define mytype(X) _Generic((X), int: 'i', float: 'f', default: 'u')
int main() {
        char type = mytype(0);
        return 0;
}
" HAVE_C__GENERIC)

check_c_source_compiles("
#include <malloc.h>
int main() {
        mallopt(M_ARENA_MAX, 1);
        mallopt(M_PERTURB, 0x5A);
        return 0;
}
" HAVE_C_MALLOPT)

check_c_source_compiles("
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/socket.h>
int main() {
        accept4(0, NULL, NULL, 0);
        return 0;
}
" HAVE_ACCEPT4)

check_c_source_compiles("
#define _GNU_SOURCE
#include <string.h>
int main() {
        char x = *strerror_r(0, &x, sizeof(x)); return 0;
}
" STRERROR_R_CHAR_P)

check_c_source_compiles("
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
int main() {
        setns(0, 0); return 0;
}
" HAVE_SETNS)

check_cxx_source_compiles("
int main() {
        __atomic_load_8(nullptr, 0);
        return 0;
}
" HAVE_BUILTIN_ATOMICS)

check_c_source_compiles("
void my_printf(char const *s, ...) __attribute__((format(printf, 1, 2)));
int main() { return 0; }
" HAVE_FUNC_ATTRIBUTE_FORMAT)

check_c_source_compiles("
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
void* my_alloc(size_t size) __attribute__((malloc));
int main() {
    void *x = my_alloc(1);
    free(x);
    return 0;
}
void* my_alloc(size_t size) {
    void *ret = malloc(size);
    if(!ret) exit(1);
    return ret;
}
" HAVE_FUNC_ATTRIBUTE_MALLOC)

check_c_source_compiles("
void my_function() __attribute__((noinline));
int main() { my_function(); return 0; }
void my_function() { ; }
" HAVE_FUNC_ATTRIBUTE_NOINLINE)

check_c_source_compiles("
void my_exit_function() __attribute__((noreturn));
int main() {
        my_exit_function(); // Call the noreturn function
        return 0;
}
void my_exit_function() {
        exit(1);
}
" HAVE_FUNC_ATTRIBUTE_NORETURN)

check_c_source_compiles("
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
void* my_alloc(size_t size) __attribute__((returns_nonnull));
int main() {
        void* ptr = my_alloc(10);
        free(ptr);
        return 0;
}
void* my_alloc(size_t size) {
        void *ret = malloc(size);
        if(!ret) exit(1);
        return ret;
}
" HAVE_FUNC_ATTRIBUTE_RETURNS_NONNULL)

check_c_source_compiles("
int my_function() __attribute__((warn_unused_result));
int main() {
        return my_function();
}
int my_function() {
        return 1;
}
" HAVE_FUNC_ATTRIBUTE_WARN_UNUSED_RESULT)
endmacro()
