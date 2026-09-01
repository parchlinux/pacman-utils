#include "fs_utils.h"
#include "pacman_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_size_formatting(void) {
    char *s1 = fs_format_size(500);
    assert(s1 != NULL);
    assert(strcmp(s1, "500 B") == 0);
    free(s1);

    char *s2 = fs_format_size(1024 * 1024 * 50); // 50 MiB
    assert(s2 != NULL);
    assert(strstr(s2, "50.00 MiB") != NULL);
    free(s2);

    char *s3 = fs_format_size((off_t)1024 * 1024 * 1024 * 4); // 4 GiB
    assert(s3 != NULL);
    assert(strstr(s3, "4.00 GiB") != NULL);
    free(s3);
}

static void test_pkg_filename_parsing(void) {
    char *pkgname = NULL;
    char *pkgver = NULL;
    char *arch = NULL;

    bool ok = fs_parse_pkg_filename("linux-6.9.1.arch1-1-x86_64.pkg.tar.zst", &pkgname, &pkgver, &arch);
    assert(ok == true);
    assert(strcmp(pkgname, "linux") == 0);
    assert(strcmp(pkgver, "6.9.1.arch1-1") == 0);
    assert(strcmp(arch, "x86_64") == 0);

    free(pkgname);
    free(pkgver);
    free(arch);

    ok = fs_parse_pkg_filename("gcc-libs-13.2.1-5-x86_64.pkg.tar.xz", &pkgname, &pkgver, &arch);
    assert(ok == true);
    assert(strcmp(pkgname, "gcc-libs") == 0);
    assert(strcmp(pkgver, "13.2.1-5") == 0);
    assert(strcmp(arch, "x86_64") == 0);

    free(pkgname);
    free(pkgver);
    free(arch);
}

int main(void) {
    printf("Running fs_utils unit tests...\n");
    test_size_formatting();
    test_pkg_filename_parsing();
    printf("All fs_utils tests passed!\n");
    return 0;
}
