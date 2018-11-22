/*
 * Path tests for kernelbase.dll
 *
 * Copyright 2017 Michael Müller
 * Copyright 2018 Zhiyi Zhang
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <stdlib.h>
#include <winerror.h>
#include <winnls.h>
#include <pathcch.h>
#include <strsafe.h>

#include "wine/test.h"

HRESULT (WINAPI *pPathCchAddBackslash)(WCHAR *out, SIZE_T size);
HRESULT (WINAPI *pPathCchAddBackslashEx)(WCHAR *out, SIZE_T size, WCHAR **endptr, SIZE_T *remaining);
HRESULT (WINAPI *pPathCchAddExtension)(WCHAR *path, SIZE_T size, const WCHAR *extension);
HRESULT (WINAPI *pPathCchCombineEx)(WCHAR *out, SIZE_T size, const WCHAR *path1, const WCHAR *path2, DWORD flags);
HRESULT (WINAPI *pPathCchFindExtension)(const WCHAR *path, SIZE_T size, const WCHAR **extension);
BOOL    (WINAPI *pPathCchIsRoot)(const WCHAR *path);
HRESULT (WINAPI *pPathCchRemoveExtension)(WCHAR *path, SIZE_T size);
HRESULT (WINAPI *pPathCchRenameExtension)(WCHAR *path, SIZE_T size, const WCHAR *extension);
HRESULT (WINAPI *pPathCchSkipRoot)(const WCHAR *path, const WCHAR **root_end);
HRESULT (WINAPI *pPathCchStripPrefix)(WCHAR *path, SIZE_T size);
HRESULT (WINAPI *pPathCchStripToRoot)(WCHAR *path, SIZE_T size);
BOOL    (WINAPI *pPathIsUNCEx)(const WCHAR *path, const WCHAR **server);

static const struct
{
    const char *path1;
    const char *path2;
    const char *result;
}
combine_test[] =
{
    /* normal paths */
    {"C:\\",  "a",     "C:\\a" },
    {"C:\\b", "..\\a", "C:\\a" },
    {"C:",    "a",     "C:\\a" },
    {"C:\\",  ".",     "C:\\" },
    {"C:\\",  "..",    "C:\\" },
    {"\\a",   "b",      "\\a\\b" },

    /* normal UNC paths */
    {"\\\\192.168.1.1\\test", "a",  "\\\\192.168.1.1\\test\\a" },
    {"\\\\192.168.1.1\\test", "..", "\\\\192.168.1.1" },

    /* NT paths */
    {"\\\\?\\C:\\", "a",  "C:\\a" },
    {"\\\\?\\C:\\", "..", "C:\\" },

    /* NT UNC path */
    {"\\\\?\\UNC\\192.168.1.1\\test", "a",  "\\\\192.168.1.1\\test\\a" },
    {"\\\\?\\UNC\\192.168.1.1\\test", "..", "\\\\192.168.1.1" },
};

static void test_PathCchCombineEx(void)
{
    WCHAR expected[MAX_PATH] = {'C',':','\\','a',0};
    WCHAR p1[MAX_PATH] = {'C',':','\\',0};
    WCHAR p2[MAX_PATH] = {'a',0};
    WCHAR output[MAX_PATH];
    HRESULT hr;
    int i;

    if (!pPathCchCombineEx)
    {
        skip("PathCchCombineEx() is not available.\n");
        return;
    }

    hr = pPathCchCombineEx(NULL, 2, p1, p2, 0);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);

    memset(output, 0xff, sizeof(output));
    hr = pPathCchCombineEx(output, 0, p1, p2, 0);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);
    ok(output[0] == 0xffff, "Expected output buffer to be unchanged\n");

    memset(output, 0xff, sizeof(output));
    hr = pPathCchCombineEx(output, 1, p1, p2, 0);
    ok(hr == STRSAFE_E_INSUFFICIENT_BUFFER, "Expected STRSAFE_E_INSUFFICIENT_BUFFER, got %08x\n", hr);
    ok(output[0] == 0, "Expected output buffer to contain NULL string\n");

    memset(output, 0xff, sizeof(output));
    hr = pPathCchCombineEx(output, 4, p1, p2, 0);
    ok(hr == STRSAFE_E_INSUFFICIENT_BUFFER, "Expected STRSAFE_E_INSUFFICIENT_BUFFER, got %08x\n", hr);
    ok(output[0] == 0x0, "Expected output buffer to contain NULL string\n");

    memset(output, 0xff, sizeof(output));
    hr = pPathCchCombineEx(output, 5, p1, p2, 0);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(!lstrcmpW(output, expected),
        "Combination of %s + %s returned %s, expected %s\n",
        wine_dbgstr_w(p1), wine_dbgstr_w(p2), wine_dbgstr_w(output), wine_dbgstr_w(expected));

    for (i = 0; i < ARRAY_SIZE(combine_test); i++)
    {
        MultiByteToWideChar(CP_ACP, 0, combine_test[i].path1, -1, p1, MAX_PATH);
        MultiByteToWideChar(CP_ACP, 0, combine_test[i].path2, -1, p2, MAX_PATH);
        MultiByteToWideChar(CP_ACP, 0, combine_test[i].result, -1, expected, MAX_PATH);

        hr = pPathCchCombineEx(output, MAX_PATH, p1, p2, 0);
        ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
        ok(!lstrcmpW(output, expected), "Combining %s with %s returned %s, expected %s\n",
            wine_dbgstr_w(p1), wine_dbgstr_w(p2), wine_dbgstr_w(output), wine_dbgstr_w(expected));
    }
}

struct addbackslash_test
{
    const char *path;
    const char *result;
    HRESULT hr;
    SIZE_T size;
    SIZE_T remaining;
};

static const struct addbackslash_test addbackslash_tests[] =
{
    { "C:",    "C:\\",    S_OK, MAX_PATH, MAX_PATH - 3 },
    { "a.txt", "a.txt\\", S_OK, MAX_PATH, MAX_PATH - 6 },
    { "a/b",   "a/b\\",   S_OK, MAX_PATH, MAX_PATH - 4 },

    { "C:\\",  "C:\\",    S_FALSE, MAX_PATH, MAX_PATH - 3 },
    { "C:\\",  "C:\\",    S_FALSE, 4, 1 },

    { "C:",    "C:",      STRSAFE_E_INSUFFICIENT_BUFFER, 2, 0 },
    { "C:",    "C:",      STRSAFE_E_INSUFFICIENT_BUFFER, 3, 1 },
    { "C:\\",  "C:\\",    STRSAFE_E_INSUFFICIENT_BUFFER, 2, 0 },
    { "C:\\",  "C:\\",    STRSAFE_E_INSUFFICIENT_BUFFER, 3, 0 },
    { "C:\\",  "C:\\",    STRSAFE_E_INSUFFICIENT_BUFFER, 0, 0 },
    { "C:",    "C:",      STRSAFE_E_INSUFFICIENT_BUFFER, 0, 0 },
};

static void test_PathCchAddBackslash(void)
{
    WCHAR pathW[MAX_PATH];
    unsigned int i;
    HRESULT hr;

    if (!pPathCchAddBackslash)
    {
        win_skip("PathCchAddBackslash() is not available.\n");
        return;
    }

    pathW[0] = 0;
    hr = pPathCchAddBackslash(pathW, 0);
    ok(hr == STRSAFE_E_INSUFFICIENT_BUFFER, "Unexpected hr %#x.\n", hr);
    ok(pathW[0] == 0, "Unexpected path.\n");

    pathW[0] = 0;
    hr = pPathCchAddBackslash(pathW, 1);
    ok(hr == S_FALSE, "Unexpected hr %#x.\n", hr);
    ok(pathW[0] == 0, "Unexpected path.\n");

    pathW[0] = 0;
    hr = pPathCchAddBackslash(pathW, 2);
    ok(hr == S_FALSE, "Unexpected hr %#x.\n", hr);
    ok(pathW[0] == 0, "Unexpected path.\n");

    for (i = 0; i < ARRAY_SIZE(addbackslash_tests); i++)
    {
        const struct addbackslash_test *test = &addbackslash_tests[i];
        char path[MAX_PATH];

        MultiByteToWideChar(CP_ACP, 0, test->path, -1, pathW, ARRAY_SIZE(pathW));
        hr = pPathCchAddBackslash(pathW, test->size);
        ok(hr == test->hr, "%u: unexpected return value %#x.\n", i, hr);

        WideCharToMultiByte(CP_ACP, 0, pathW, -1, path, ARRAY_SIZE(path), NULL, NULL);
        ok(!strcmp(path, test->result), "%u: unexpected resulting path %s.\n", i, path);
    }
}

static void test_PathCchAddBackslashEx(void)
{
    WCHAR pathW[MAX_PATH];
    SIZE_T remaining;
    unsigned int i;
    HRESULT hr;
    WCHAR *ptrW;

    if (!pPathCchAddBackslashEx)
    {
        win_skip("PathCchAddBackslashEx() is not available.\n");
        return;
    }

    pathW[0] = 0;
    hr = pPathCchAddBackslashEx(pathW, 0, NULL, NULL);
    ok(hr == STRSAFE_E_INSUFFICIENT_BUFFER, "Unexpected hr %#x.\n", hr);
    ok(pathW[0] == 0, "Unexpected path.\n");

    pathW[0] = 0;
    ptrW = (void *)0xdeadbeef;
    remaining = 123;
    hr = pPathCchAddBackslashEx(pathW, 1, &ptrW, &remaining);
    ok(hr == S_FALSE, "Unexpected hr %#x.\n", hr);
    ok(pathW[0] == 0, "Unexpected path.\n");
    ok(ptrW == pathW, "Unexpected endptr %p.\n", ptrW);
    ok(remaining == 1, "Unexpected remaining size.\n");

    pathW[0] = 0;
    hr = pPathCchAddBackslashEx(pathW, 2, NULL, NULL);
    ok(hr == S_FALSE, "Unexpected hr %#x.\n", hr);
    ok(pathW[0] == 0, "Unexpected path.\n");

    for (i = 0; i < ARRAY_SIZE(addbackslash_tests); i++)
    {
        const struct addbackslash_test *test = &addbackslash_tests[i];
        char path[MAX_PATH];

        MultiByteToWideChar(CP_ACP, 0, test->path, -1, pathW, ARRAY_SIZE(pathW));
        hr = pPathCchAddBackslashEx(pathW, test->size, NULL, NULL);
        ok(hr == test->hr, "%u: unexpected return value %#x.\n", i, hr);

        WideCharToMultiByte(CP_ACP, 0, pathW, -1, path, ARRAY_SIZE(path), NULL, NULL);
        ok(!strcmp(path, test->result), "%u: unexpected resulting path %s.\n", i, path);

        ptrW = (void *)0xdeadbeef;
        remaining = 123;
        MultiByteToWideChar(CP_ACP, 0, test->path, -1, pathW, ARRAY_SIZE(pathW));
        hr = pPathCchAddBackslashEx(pathW, test->size, &ptrW, &remaining);
        ok(hr == test->hr, "%u: unexpected return value %#x.\n", i, hr);
        if (SUCCEEDED(hr))
        {
            ok(ptrW == (pathW + lstrlenW(pathW)), "%u: unexpected end pointer.\n", i);
            ok(remaining == test->remaining, "%u: unexpected remaining buffer length.\n", i);
        }
        else
        {
            ok(ptrW == NULL, "%u: unexpecred end pointer.\n", i);
            ok(remaining == 0, "%u: unexpected remaining buffer length.\n", i);
        }
    }
}

struct addextension_test
{
    const CHAR *path;
    const CHAR *extension;
    const CHAR *expected;
    HRESULT hr;
};

static const struct addextension_test addextension_tests[] =
{
    /* Normal */
    {"", ".exe", ".exe", S_OK},
    {"C:\\", "", "C:\\", S_OK},
    {"C:", ".exe", "C:.exe", S_OK},
    {"C:\\", ".exe", "C:\\.exe", S_OK},
    {"\\", ".exe", "\\.exe", S_OK},
    {"\\\\", ".exe", "\\\\.exe", S_OK},
    {"\\\\?\\C:", ".exe", "\\\\?\\C:.exe", S_OK},
    {"\\\\?\\C:\\", ".exe", "\\\\?\\C:\\.exe", S_OK},
    {"\\\\?\\UNC\\", ".exe", "\\\\?\\UNC\\.exe", S_OK},
    {"\\\\?\\UNC\\192.168.1.1\\", ".exe", "\\\\?\\UNC\\192.168.1.1\\.exe", S_OK},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", ".exe",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\.exe", S_OK},
    {"C:\\", "exe", "C:\\.exe", S_OK},
    {"C:\\", ".", "C:\\", S_OK},
    {"C:\\1.exe", ".txt", "C:\\1.exe", S_FALSE},

    /* Extension contains invalid characters but valid for PathCchAddExtension */
    {"C:\\", "./", "C:\\./", S_OK},
    {"C:\\", ".?", "C:\\.?", S_OK},
    {"C:\\", ".%", "C:\\.%", S_OK},
    {"C:\\", ".*", "C:\\.*", S_OK},
    {"C:\\", ".:", "C:\\.:", S_OK},
    {"C:\\", ".|", "C:\\.|", S_OK},
    {"C:\\", ".\"", "C:\\.\"", S_OK},
    {"C:\\", ".<", "C:\\.<", S_OK},
    {"C:\\", ".>", "C:\\.>", S_OK},

    /* Invalid argument for extension */
    {"C:\\", " exe", NULL, E_INVALIDARG},
    {"C:\\", ". exe", NULL, E_INVALIDARG},
    {"C:\\", " ", NULL, E_INVALIDARG},
    {"C:\\", "\\", NULL, E_INVALIDARG},
    {"C:\\", "..", NULL, E_INVALIDARG},
    {"C:\\", ". ", NULL, E_INVALIDARG},
    {"C:\\", ".\\", NULL, E_INVALIDARG},
    {"C:\\", ".a.", NULL, E_INVALIDARG},
    {"C:\\", ".a ", NULL, E_INVALIDARG},
    {"C:\\", ".a\\", NULL, E_INVALIDARG},
    {"C:\\1.exe", " ", NULL, E_INVALIDARG}
};

static void test_PathCchAddExtension(void)
{
    WCHAR pathW[PATHCCH_MAX_CCH + 1];
    CHAR pathA[PATHCCH_MAX_CCH + 1];
    WCHAR extensionW[MAX_PATH];
    HRESULT hr;
    INT i;

    if (!pPathCchAddExtension)
    {
        win_skip("PathCchAddExtension() is not available.\n");
        return;
    }

    /* Arguments check */
    MultiByteToWideChar(CP_ACP, 0, "C:\\", -1, pathW, ARRAY_SIZE(pathW));
    MultiByteToWideChar(CP_ACP, 0, ".exe", -1, extensionW, ARRAY_SIZE(extensionW));

    hr = pPathCchAddExtension(NULL, PATHCCH_MAX_CCH, extensionW);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchAddExtension(pathW, 0, extensionW);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchAddExtension(pathW, PATHCCH_MAX_CCH, NULL);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    /* Path length check */
    hr = pPathCchAddExtension(pathW, ARRAY_SIZE("C:\\.exe") - 1, extensionW);
    ok(hr == STRSAFE_E_INSUFFICIENT_BUFFER, "expect result %#x, got %#x\n", STRSAFE_E_INSUFFICIENT_BUFFER, hr);

    hr = pPathCchAddExtension(pathW, PATHCCH_MAX_CCH + 1, extensionW);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchAddExtension(pathW, PATHCCH_MAX_CCH, extensionW);
    ok(hr == S_OK, "expect result %#x, got %#x\n", S_OK, hr);

    for (i = 0; i < ARRAY_SIZE(addextension_tests); i++)
    {
        const struct addextension_test *t = addextension_tests + i;
        MultiByteToWideChar(CP_ACP, 0, t->path, -1, pathW, ARRAY_SIZE(pathW));
        MultiByteToWideChar(CP_ACP, 0, t->extension, -1, extensionW, ARRAY_SIZE(extensionW));
        hr = pPathCchAddExtension(pathW, PATHCCH_MAX_CCH, extensionW);
        ok(hr == t->hr, "path %s extension %s expect result %#x, got %#x\n", t->path, t->extension, t->hr, hr);
        if (SUCCEEDED(hr))
        {
            WideCharToMultiByte(CP_ACP, 0, pathW, -1, pathA, ARRAY_SIZE(pathA), NULL, NULL);
            ok(!lstrcmpA(pathA, t->expected), "path %s extension %s expect output path %s, got %s\n", t->path,
               t->extension, t->expected, pathA);
        }
    }
}

struct findextension_test
{
    const CHAR *path;
    INT extension_offset;
};

static const struct findextension_test findextension_tests[] =
{
    /* Normal */
    {"1.exe", 1},
    {"C:1.exe", 3},
    {"C:\\1.exe", 4},
    {"\\1.exe", 2},
    {"\\\\1.exe", 3},
    {"\\\\?\\C:1.exe", 7},
    {"\\\\?\\C:\\1.exe", 8},
    {"\\\\?\\UNC\\1.exe", 9},
    {"\\\\?\\UNC\\192.168.1.1\\1.exe", 21},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\1.exe", 50},

    /* Contains forward slash */
    {"C:\\a/1.exe", 6},
    {"/1.exe", 2},
    {"//1.exe", 3},
    {"C:\\a/b/1.exe", 8},
    {"/a/1.exe", 4},
    {"/a/1.exe", 4},

    /* Malformed */
    {"", 0},
    {" ", 1},
    {".", 0},
    {"..", 1},
    {"a", 1},
    {"a.", 1},
    {".a.b.", 4},
    {"a. ", 3},
    {"a.\\", 3},
    {"\\\\?\\UNC\\192.168.1.1", 17},
    {"\\\\?\\UNC\\192.168.1.1\\", 20},
    {"\\\\?\\UNC\\192.168.1.1\\a", 21}
};

static void test_PathCchFindExtension(void)
{
    WCHAR pathW[PATHCCH_MAX_CCH + 1] = {0};
    const WCHAR *extension;
    HRESULT hr;
    INT i;

    if (!pPathCchFindExtension)
    {
        win_skip("PathCchFindExtension() is not available.\n");
        return;
    }

    /* Arguments check */
    extension = (const WCHAR *)0xdeadbeef;
    hr = pPathCchFindExtension(NULL, PATHCCH_MAX_CCH, &extension);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);
    ok(extension == NULL, "Expect extension null, got %p\n", extension);

    extension = (const WCHAR *)0xdeadbeef;
    hr = pPathCchFindExtension(pathW, 0, &extension);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);
    ok(extension == NULL, "Expect extension null, got %p\n", extension);

    /* Crashed on Windows */
    if (0)
    {
        hr = pPathCchFindExtension(pathW, PATHCCH_MAX_CCH, NULL);
        ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);
    }

    /* Path length check */
    /* size == PATHCCH_MAX_CCH + 1 */
    MultiByteToWideChar(CP_ACP, 0, "C:\\1.exe", -1, pathW, ARRAY_SIZE(pathW));
    hr = pPathCchFindExtension(pathW, PATHCCH_MAX_CCH + 1, &extension);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    /* Size == path length + 1*/
    hr = pPathCchFindExtension(pathW, ARRAY_SIZE("C:\\1.exe"), &extension);
    ok(hr == S_OK, "expect result %#x, got %#x\n", S_OK, hr);
    ok(*extension == '.', "wrong extension value\n");

    /* Size < path length + 1 */
    extension = (const WCHAR *)0xdeadbeef;
    hr = pPathCchFindExtension(pathW, ARRAY_SIZE("C:\\1.exe") - 1, &extension);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);
    ok(extension == NULL, "Expect extension null, got %p\n", extension);

    /* Size == PATHCCH_MAX_CCH */
    hr = pPathCchFindExtension(pathW, PATHCCH_MAX_CCH, &extension);
    ok(hr == S_OK, "expect result %#x, got %#x\n", S_OK, hr);

    /* Path length + 1 > PATHCCH_MAX_CCH */
    for (i = 0; i < ARRAY_SIZE(pathW) - 1; i++) pathW[i] = 'a';
    pathW[PATHCCH_MAX_CCH] = 0;
    hr = pPathCchFindExtension(pathW, PATHCCH_MAX_CCH, &extension);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    /* Path length + 1 == PATHCCH_MAX_CCH */
    pathW[PATHCCH_MAX_CCH - 1] = 0;
    hr = pPathCchFindExtension(pathW, PATHCCH_MAX_CCH, &extension);
    ok(hr == S_OK, "expect result %#x, got %#x\n", S_OK, hr);

    for (i = 0; i < ARRAY_SIZE(findextension_tests); i++)
    {
        const struct findextension_test *t = findextension_tests + i;
        MultiByteToWideChar(CP_ACP, 0, t->path, -1, pathW, ARRAY_SIZE(pathW));
        hr = pPathCchFindExtension(pathW, PATHCCH_MAX_CCH, &extension);
        ok(hr == S_OK, "path %s expect result %#x, got %#x\n", t->path, S_OK, hr);
        if (SUCCEEDED(hr))
            ok(extension - pathW == t->extension_offset, "path %s expect extension offset %d, got %ld\n", t->path,
               t->extension_offset, (UINT_PTR)(extension - pathW));
    }
}

struct isroot_test
{
    const CHAR *path;
    BOOL ret;
};

static const struct isroot_test isroot_tests[] =
{
    {"", FALSE},
    {"a", FALSE},
    {"C:", FALSE},
    {"C:\\", TRUE},
    {"C:\\a", FALSE},
    {"\\\\?\\C:\\", TRUE},
    {"\\\\?\\C:", FALSE},
    {"\\\\?\\C:\\a", FALSE},
    {"\\", TRUE},
    {"\\a\\", FALSE},
    {"\\a\\b", FALSE},
    {"\\\\", TRUE},
    {"\\\\a", TRUE},
    {"\\\\a\\", FALSE},
    {"\\\\a\\b", TRUE},
    {"\\\\a\\b\\", FALSE},
    {"\\\\a\\b\\c", FALSE},
    {"\\\\?\\UNC\\", TRUE},
    {"\\\\?\\UNC\\a", TRUE},
    {"\\\\?\\UNC\\a\\", FALSE},
    {"\\\\?\\UNC\\a\\b", TRUE},
    {"\\\\?\\UNC\\a\\b\\", FALSE},
    {"\\\\?\\UNC\\a\\b\\c", FALSE},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}", FALSE},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", TRUE},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\a", FALSE},
    {"..\\a", FALSE},

    /* Wrong MSDN examples */
    {"\\a", FALSE},
    {"X:", FALSE},
    {"\\server", FALSE}
};

static void test_PathCchIsRoot(void)
{
    WCHAR pathW[MAX_PATH];
    BOOL ret;
    INT i;

    if (!pPathCchIsRoot)
    {
        win_skip("PathCchIsRoot() is not available.\n");
        return;
    }

    ret = pPathCchIsRoot(NULL);
    ok(ret == FALSE, "expect return FALSE\n");

    for (i = 0; i < ARRAY_SIZE(isroot_tests); i++)
    {
        const struct isroot_test *t = isroot_tests + i;
        MultiByteToWideChar(CP_ACP, 0, t->path, -1, pathW, ARRAY_SIZE(pathW));
        ret = pPathCchIsRoot(pathW);
        ok(ret == t->ret, "path %s expect return %d, got %d\n", t->path, t->ret, ret);
    }
}

struct removeextension_test
{
    const CHAR *path;
    const CHAR *expected;
    HRESULT hr;
};

static const struct removeextension_test removeextension_tests[] =
{
    {"1.exe", "1", S_OK},
    {"C:1.exe", "C:1", S_OK},
    {"C:\\1.exe", "C:\\1", S_OK},
    {"\\1.exe", "\\1", S_OK},
    {"\\\\1.exe", "\\\\1", S_OK},
    {"\\\\?\\C:1.exe", "\\\\?\\C:1", S_OK},
    {"\\\\?\\C:\\1.exe", "\\\\?\\C:\\1", S_OK},
    {"\\\\?\\UNC\\1.exe", "\\\\?\\UNC\\1", S_OK},
    {"\\\\?\\UNC\\192.168.1.1\\1.exe", "\\\\?\\UNC\\192.168.1.1\\1", S_OK},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\1.exe",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\1", S_OK},

    /* Malformed */
    {"", "", S_FALSE},
    {" ", " ", S_FALSE},
    {".", "", S_OK},
    {"..", ".", S_OK},
    {"a", "a", S_FALSE},
    {"a.", "a", S_OK},
    {".a.b.", ".a.b", S_OK},
    {"a. ", "a. ", S_FALSE},
    {"a.\\", "a.\\", S_FALSE},
    {"\\\\?\\UNC\\192.168.1.1", "\\\\?\\UNC\\192.168.1", S_OK},
    {"\\\\?\\UNC\\192.168.1.1\\", "\\\\?\\UNC\\192.168.1.1\\", S_FALSE},
    {"\\\\?\\UNC\\192.168.1.1\\a", "\\\\?\\UNC\\192.168.1.1\\a", S_FALSE}
};

static void test_PathCchRemoveExtension(void)
{
    WCHAR pathW[PATHCCH_MAX_CCH] = {0};
    CHAR pathA[PATHCCH_MAX_CCH];
    HRESULT hr;
    INT i;

    if (!pPathCchRemoveExtension)
    {
        win_skip("PathCchRemoveExtension() is not available.\n");
        return;
    }

    /* Arguments check */
    hr = pPathCchRemoveExtension(NULL, PATHCCH_MAX_CCH);
    ok(hr == E_INVALIDARG, "expect %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchRemoveExtension(pathW, 0);
    ok(hr == E_INVALIDARG, "expect %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchRemoveExtension(pathW, PATHCCH_MAX_CCH + 1);
    ok(hr == E_INVALIDARG, "expect %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchRemoveExtension(pathW, PATHCCH_MAX_CCH);
    ok(hr == S_FALSE, "expect %#x, got %#x\n", S_FALSE, hr);

    /* Size < original path length + 1 */
    MultiByteToWideChar(CP_ACP, 0, "C:\\1.exe", -1, pathW, ARRAY_SIZE(pathW));
    hr = pPathCchRemoveExtension(pathW, ARRAY_SIZE("C:\\1.exe") - 1);
    ok(hr == E_INVALIDARG, "expect %#x, got %#x\n", E_INVALIDARG, hr);

    for (i = 0; i < ARRAY_SIZE(removeextension_tests); i++)
    {
        const struct removeextension_test *t = removeextension_tests + i;

        MultiByteToWideChar(CP_ACP, 0, t->path, -1, pathW, ARRAY_SIZE(pathW));
        hr = pPathCchRemoveExtension(pathW, ARRAY_SIZE(pathW));
        ok(hr == t->hr, "path %s expect result %#x, got %#x\n", t->path, t->hr, hr);
        if (SUCCEEDED(hr))
        {
            WideCharToMultiByte(CP_ACP, 0, pathW, -1, pathA, ARRAY_SIZE(pathA), NULL, NULL);
            ok(!lstrcmpA(pathA, t->expected), "path %s expect stripped path %s, got %s\n", t->path, t->expected, pathA);
        }
    }
}

struct renameextension_test
{
    const CHAR *path;
    const CHAR *extension;
    const CHAR *expected;
};

static const struct renameextension_test renameextension_tests[] =
{
    {"1.exe", ".txt", "1.txt"},
    {"C:1.exe", ".txt", "C:1.txt"},
    {"C:\\1.exe", ".txt", "C:\\1.txt"},
    {"\\1.exe", ".txt", "\\1.txt"},
    {"\\\\1.exe", ".txt", "\\\\1.txt"},
    {"\\\\?\\C:1.exe", ".txt", "\\\\?\\C:1.txt"},
    {"\\\\?\\C:\\1.exe", ".txt", "\\\\?\\C:\\1.txt"},
    {"\\\\?\\UNC\\1.exe", ".txt", "\\\\?\\UNC\\1.txt"},
    {"\\\\?\\UNC\\192.168.1.1\\1.exe", ".txt", "\\\\?\\UNC\\192.168.1.1\\1.txt"},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\1.exe", ".txt",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\1.txt"},
    {"C:\\1.exe", "", "C:\\1"},
    {"C:\\1.exe", "txt", "C:\\1.txt"}
};

static void test_PathCchRenameExtension(void)
{
    WCHAR pathW[PATHCCH_MAX_CCH + 1];
    CHAR pathA[PATHCCH_MAX_CCH + 1];
    WCHAR extensionW[MAX_PATH];
    HRESULT hr;
    INT i;

    if (!pPathCchRenameExtension)
    {
        win_skip("PathCchRenameExtension() is not available.\n");
        return;
    }

    /* Invalid arguments */
    MultiByteToWideChar(CP_ACP, 0, "C:\\1.txt", -1, pathW, ARRAY_SIZE(pathW));
    MultiByteToWideChar(CP_ACP, 0, ".exe", -1, extensionW, ARRAY_SIZE(extensionW));

    hr = pPathCchRenameExtension(NULL, PATHCCH_MAX_CCH, extensionW);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchRenameExtension(pathW, 0, extensionW);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchRenameExtension(pathW, PATHCCH_MAX_CCH, NULL);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    /* Path length */
    hr = pPathCchRenameExtension(pathW, ARRAY_SIZE("C:\\1.exe") - 1, extensionW);
    ok(E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchRenameExtension(pathW, PATHCCH_MAX_CCH + 1, extensionW);
    ok(hr == E_INVALIDARG, "expect result %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchRenameExtension(pathW, PATHCCH_MAX_CCH, extensionW);
    ok(hr == S_OK, "expect result %#x, got %#x\n", S_OK, hr);

    for (i = 0; i < ARRAY_SIZE(renameextension_tests); i++)
    {
        const struct renameextension_test *t = renameextension_tests + i;
        MultiByteToWideChar(CP_ACP, 0, t->path, -1, pathW, ARRAY_SIZE(pathW));
        MultiByteToWideChar(CP_ACP, 0, t->extension, -1, extensionW, ARRAY_SIZE(extensionW));
        hr = pPathCchRenameExtension(pathW, PATHCCH_MAX_CCH, extensionW);
        ok(hr == S_OK, "path %s extension %s expect result %#x, got %#x\n", t->path, t->extension, S_OK, hr);
        if (SUCCEEDED(hr))
        {
            WideCharToMultiByte(CP_ACP, 0, pathW, -1, pathA, ARRAY_SIZE(pathA), NULL, NULL);
            ok(!lstrcmpA(pathA, t->expected), "path %s extension %s expect output path %s, got %s\n", t->path,
               t->extension, t->expected, pathA);
        }
    }
}

struct skiproot_test
{
    const char *path;
    int root_offset;
    HRESULT hr;
};

static const struct skiproot_test skiproot_tests [] =
{
    /* Basic combination */
    {"", 0, E_INVALIDARG},
    {"C:\\", 3, S_OK},
    {"\\", 1, S_OK},
    {"\\\\.\\", 4, S_OK},
    {"\\\\?\\UNC\\", 8, S_OK},
    {"\\\\?\\C:\\", 7, S_OK},

    /* Basic + \ */
    {"C:\\\\", 3, S_OK},
    {"\\\\", 2, S_OK},
    {"\\\\.\\\\", 4, S_OK},
    {"\\\\?\\UNC\\\\", 9, S_OK},
    {"\\\\?\\C:\\\\", 7, S_OK},

    /* Basic + a */
    {"a", 0, E_INVALIDARG},
    {"C:\\a", 3, S_OK},
    {"\\a", 1, S_OK},
    {"\\\\.\\a", 5, S_OK},
    {"\\\\?\\UNC\\a", 9, S_OK},

    /* Basic + \a */
    {"\\a", 1, S_OK},
    {"C:\\\\a", 3, S_OK},
    {"\\\\a", 3, S_OK},
    {"\\\\.\\\\a", 4, S_OK},
    {"\\\\?\\UNC\\\\a", 10, S_OK},
    {"\\\\?\\C:\\\\a", 7, S_OK},

    /* Basic + a\ */
    {"a\\", 0, E_INVALIDARG},
    {"C:\\a\\", 3, S_OK},
    {"\\a\\", 1, S_OK},
    {"\\\\.\\a\\", 6, S_OK},
    {"\\\\?\\UNC\\a\\", 10, S_OK},
    {"\\\\?\\C:\\a\\", 7, S_OK},

    /* Network share */
    {"\\\\\\\\", 3, S_OK},
    {"\\\\a\\", 4, S_OK},
    {"\\\\a\\b", 5, S_OK},
    {"\\\\a\\b\\", 6, S_OK},
    {"\\\\a\\b\\\\", 6, S_OK},
    {"\\\\a\\b\\\\c", 6, S_OK},
    {"\\\\a\\b\\c", 6, S_OK},
    {"\\\\a\\b\\c\\", 6, S_OK},
    {"\\\\a\\b\\c\\d", 6, S_OK},
    {"\\\\a\\\\b\\c\\", 4, S_OK},
    {"\\\\aa\\bb\\cc\\", 8, S_OK},

    /* UNC */
    {"\\\\?\\UNC\\\\", 9, S_OK},
    {"\\\\?\\UNC\\a\\b", 11, S_OK},
    {"\\\\?\\UNC\\a\\b", 11, S_OK},
    {"\\\\?\\UNC\\a\\b\\", 12, S_OK},
    {"\\\\?\\UNC\\a\\b\\c", 12, S_OK},
    {"\\\\?\\UNC\\a\\b\\c\\", 12, S_OK},
    {"\\\\?\\UNC\\a\\b\\c\\d", 12, S_OK},
    {"\\\\?\\C:", 6, S_OK},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}", 48, S_OK},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", 49, S_OK},
    {"\\\\?\\unc\\a\\b", 11, S_OK},
    {"\\\\?\\volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", 49, S_OK},
    {"\\\\?\\volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\a", 49, S_OK},

    /* Malformed */
    {"C:", 2, S_OK},
    {":", 0, E_INVALIDARG},
    {":\\", 0, E_INVALIDARG},
    {"C\\", 0, E_INVALIDARG},
    {"\\?", 1, S_OK},
    {"\\?\\UNC", 1, S_OK},
    {"\\\\?\\", 0, E_INVALIDARG},
    {"\\\\?\\UNC", 0, E_INVALIDARG},
    {"\\\\?\\::\\", 0, E_INVALIDARG},
    {"\\\\?\\Volume", 0, E_INVALIDARG},
    {"\\.", 1, S_OK},
    {"\\\\..", 4, S_OK},
    {"\\\\..a", 5, S_OK}
};

static void test_PathCchSkipRoot(void)
{
    WCHAR pathW[MAX_PATH];
    const WCHAR *root_end;
    HRESULT hr;
    INT i;

    if (!pPathCchSkipRoot)
    {
        win_skip("PathCchSkipRoot() is not available.\n");
        return;
    }

    root_end = (const WCHAR *)0xdeadbeef;
    hr = pPathCchSkipRoot(NULL, &root_end);
    ok(hr == E_INVALIDARG, "Expect result %#x, got %#x\n", E_INVALIDARG, hr);
    ok(root_end == (const WCHAR *)0xdeadbeef, "Expect root_end 0xdeadbeef, got %p\n", root_end);

    MultiByteToWideChar(CP_ACP, 0, "C:\\", -1, pathW, ARRAY_SIZE(pathW));
    hr = pPathCchSkipRoot(pathW, NULL);
    ok(hr == E_INVALIDARG, "Expect result %#x, got %#x\n", E_INVALIDARG, hr);

    for (i = 0; i < ARRAY_SIZE(skiproot_tests); i++)
    {
        const struct skiproot_test *t = skiproot_tests + i;
        MultiByteToWideChar(CP_ACP, 0, t->path, -1, pathW, ARRAY_SIZE(pathW));
        hr = pPathCchSkipRoot(pathW, &root_end);
        ok(hr == t->hr, "path %s expect result %#x, got %#x\n", t->path, t->hr, hr);
        if (SUCCEEDED(hr))
            ok(root_end - pathW == t->root_offset, "path %s expect root offset %d, got %ld\n", t->path, t->root_offset,
               (INT_PTR)(root_end - pathW));
    }
}

struct stripprefix_test
{
    const CHAR *path;
    const CHAR *stripped_path;
    HRESULT hr;
    SIZE_T size;
};

static const struct stripprefix_test stripprefix_tests[] =
{
    {"\\\\?\\UNC\\", "\\\\", S_OK},
    {"\\\\?\\UNC\\a", "\\\\a", S_OK},
    {"\\\\?\\C:", "C:", S_OK},
    {"\\\\?\\C:\\", "C:\\", S_OK},
    {"\\\\?\\C:\\a", "C:\\a", S_OK},
    {"\\\\?\\unc\\", "\\\\", S_OK},
    {"\\\\?\\c:\\", "c:\\", S_OK},

    {"\\", "\\", S_FALSE},
    {"\\\\", "\\\\", S_FALSE},
    {"\\\\a", "\\\\a", S_FALSE},
    {"\\\\a\\", "\\\\a\\", S_FALSE},
    {"\\\\?\\a", "\\\\?\\a", S_FALSE},
    {"\\\\?\\UNC", "\\\\?\\UNC", S_FALSE},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", S_FALSE},

    /* Size Tests */
    {"C:\\", NULL, E_INVALIDARG, PATHCCH_MAX_CCH + 1},
    {"C:\\", "C:\\", S_FALSE, PATHCCH_MAX_CCH},
    /* Size < original path actual length + 1, read beyond size */
    {"\\\\?\\C:\\", "C:\\", S_OK, ARRAY_SIZE("\\\\?\\C:\\") - 1},
    /* Size < stripped path length + 1 */
    {"\\\\?\\C:\\", NULL, E_INVALIDARG, ARRAY_SIZE("C:\\") - 1},
    {"\\\\?\\UNC\\", NULL, E_INVALIDARG, ARRAY_SIZE("\\\\") - 1}
};

static void test_PathCchStripPrefix(void)
{
    WCHAR pathW[PATHCCH_MAX_CCH + 1] = {0};
    CHAR stripped_pathA[PATHCCH_MAX_CCH];
    SIZE_T size;
    HRESULT hr;
    INT i;

    if (!pPathCchStripPrefix)
    {
        win_skip("PathCchStripPrefix(() is not available.\n");
        return;
    }

    /* Null arguments */
    hr = pPathCchStripPrefix(NULL, PATHCCH_MAX_CCH);
    ok(hr == E_INVALIDARG, "expect %#x, got %#x\n", E_INVALIDARG, hr);

    hr = pPathCchStripPrefix(pathW, 0);
    ok(hr == E_INVALIDARG, "expect %#x, got %#x\n", E_INVALIDARG, hr);

    for (i = 0; i < ARRAY_SIZE(stripprefix_tests); i++)
    {
        const struct stripprefix_test *t = stripprefix_tests + i;

        MultiByteToWideChar(CP_ACP, 0, t->path, -1, pathW, ARRAY_SIZE(pathW));
        size = t->size ? t->size : PATHCCH_MAX_CCH;
        hr = pPathCchStripPrefix(pathW, size);
        ok(hr == t->hr, "path %s expect result %#x, got %#x\n", t->path, t->hr, hr);
        if (SUCCEEDED(hr))
        {
            WideCharToMultiByte(CP_ACP, 0, pathW, -1, stripped_pathA, ARRAY_SIZE(stripped_pathA), NULL, NULL);
            ok(!lstrcmpA(stripped_pathA, t->stripped_path), "path %s expect stripped path %s, got %s\n", t->path,
               t->stripped_path, stripped_pathA);
        }
    }
}

struct striptoroot_test
{
    const CHAR *path;
    const CHAR *root;
    HRESULT hr;
    SIZE_T size;
};

static const struct striptoroot_test striptoroot_tests[] =
{
    /* Invalid */
    {"", "", E_INVALIDARG},
    {"C", NULL, E_INVALIDARG},
    {"\\\\?\\UNC", NULL, E_INVALIDARG},

    /* Size */
    {"C:\\", NULL, E_INVALIDARG, PATHCCH_MAX_CCH + 1},
    {"C:\\", "C:\\", S_FALSE, PATHCCH_MAX_CCH},
    /* Size < original path length + 1, read beyond size */
    {"C:\\a", "C:\\", S_OK, ARRAY_SIZE("C:\\a") - 1},
    /* Size < stripped path length + 1 */
    {"C:\\a", "C:\\", E_INVALIDARG, ARRAY_SIZE("C:\\") - 1},
    {"\\\\a\\b\\c", NULL, E_INVALIDARG, ARRAY_SIZE("\\\\a\\b") - 1},

    /* X: */
    {"C:", "C:", S_FALSE},
    {"C:a", "C:", S_OK},
    {"C:a\\b", "C:", S_OK},
    {"C:a\\b\\c", "C:", S_OK},

    /* X:\ */
    {"C:\\", "C:\\", S_FALSE},
    {"C:\\a", "C:\\", S_OK},
    {"C:\\a\\b", "C:\\", S_OK},
    {"C:\\a\\b\\c", "C:\\", S_OK},

    /* \ */
    {"\\", "\\", S_FALSE},
    {"\\a", "\\", S_OK},
    {"\\a\\b", "\\", S_OK},
    {"\\a\\b\\c", "\\", S_OK},

    /* \\ */
    {"\\\\", "\\\\", S_FALSE},
    {"\\\\a", "\\\\a", S_FALSE},
    {"\\\\a\\b", "\\\\a\\b", S_FALSE},
    {"\\\\a\\b\\c", "\\\\a\\b", S_OK},

    /* UNC */
    {"\\\\?\\UNC\\", "\\\\?\\UNC\\", S_FALSE},
    {"\\\\?\\UNC\\a", "\\\\?\\UNC\\a", S_FALSE},
    {"\\\\?\\UNC\\a\\b", "\\\\?\\UNC\\a\\b", S_FALSE},
    {"\\\\?\\UNC\\a\\b\\", "\\\\?\\UNC\\a\\b", S_OK},
    {"\\\\?\\UNC\\a\\b\\c", "\\\\?\\UNC\\a\\b", S_OK},

    /* Prefixed X: */
    {"\\\\?\\C:", "\\\\?\\C:", S_FALSE},
    {"\\\\?\\C:a", "\\\\?\\C:", S_OK},
    {"\\\\?\\C:a\\b", "\\\\?\\C:", S_OK},
    {"\\\\?\\C:a\\b\\c", "\\\\?\\C:", S_OK},

    /* Prefixed X:\ */
    {"\\\\?\\C:\\", "\\\\?\\C:\\", S_FALSE},
    {"\\\\?\\C:\\a", "\\\\?\\C:\\", S_OK},
    {"\\\\?\\C:\\a\\b", "\\\\?\\C:\\", S_OK},
    {"\\\\?\\C:\\a\\b\\c", "\\\\?\\C:\\", S_OK},

    /* UNC Volume */
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}", S_FALSE},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}a",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}", S_OK},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}a\\b",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}", S_OK},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}a\\b\\c",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}", S_OK},

    /* UNC Volume with backslash */
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", S_FALSE},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\a",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", S_OK},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\a\\b",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", S_OK},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\a\\b\\c",
     "\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", S_OK},
};

static void test_PathCchStripToRoot(void)
{
    WCHAR pathW[PATHCCH_MAX_CCH];
    CHAR rootA[PATHCCH_MAX_CCH];
    SIZE_T size;
    HRESULT hr;
    INT i;

    if (!pPathCchStripToRoot)
    {
        win_skip("PathCchStripToRoot() is not available.\n");
        return;
    }

    /* Null arguments */
    hr = pPathCchStripToRoot(NULL, ARRAY_SIZE(pathW));
    ok(hr == E_INVALIDARG, "Expect result %#x, got %#x\n", E_INVALIDARG, hr);

    MultiByteToWideChar(CP_ACP, 0, "C:\\a", -1, pathW, ARRAY_SIZE(pathW));
    hr = pPathCchStripToRoot(pathW, 0);
    ok(hr == E_INVALIDARG, "Expect result %#x, got %#x\n", E_INVALIDARG, hr);

    for (i = 0; i < ARRAY_SIZE(striptoroot_tests); i++)
    {
        const struct striptoroot_test *t = striptoroot_tests + i;
        MultiByteToWideChar(CP_ACP, 0, t->path, -1, pathW, ARRAY_SIZE(pathW));
        size = t->size ? t->size : ARRAY_SIZE(pathW);
        hr = pPathCchStripToRoot(pathW, size);
        ok(hr == t->hr, "path %s expect result %#x, got %#x\n", t->path, t->hr, hr);
        if (SUCCEEDED(hr))
        {
            WideCharToMultiByte(CP_ACP, 0, pathW, -1, rootA, ARRAY_SIZE(rootA), NULL, NULL);
            ok(!lstrcmpA(rootA, t->root), "path %s expect stripped path %s, got %s\n", t->path, t->root, rootA);
        }
    }
}

struct isuncex_test
{
    const CHAR *path;
    INT server_offset;
    BOOL ret;
};

static const struct isuncex_test isuncex_tests[] =
{
    {"\\\\", 2, TRUE},
    {"\\\\a\\", 2, TRUE},
    {"\\\\.\\", 2, TRUE},
    {"\\\\?\\UNC\\", 8, TRUE},
    {"\\\\?\\UNC\\a", 8, TRUE},
    {"\\\\?\\unc\\", 8, TRUE},
    {"\\\\?\\unc\\a", 8, TRUE},

    {"", 0, FALSE},
    {"\\", 0, FALSE},
    {"C:\\", 0, FALSE},
    {"\\??\\", 0, FALSE},
    {"\\\\?\\", 0, FALSE},
    {"\\\\?\\UNC", 0, FALSE},
    {"\\\\?\\C:", 0, FALSE},
    {"\\\\?\\C:\\", 0, FALSE},
    {"\\\\?\\C:\\a", 0, FALSE},
    {"\\\\?\\Volume{e51a1864-6f2d-4019-b73d-f4e60e600c26}\\", 0, FALSE}
};

static void test_PathIsUNCEx(void)
{
    WCHAR pathW[MAX_PATH];
    const WCHAR *server;
    BOOL ret;
    INT i;

    if (!pPathIsUNCEx)
    {
        win_skip("PathIsUNCEx(() is not available.\n");
        return;
    }

    /* No NULL check for path pointers on Windows */
    if (0)
    {
        ret = pPathIsUNCEx(NULL, &server);
        ok(ret == FALSE, "expect FALSE\n");
    }

    MultiByteToWideChar(CP_ACP, 0, "C:\\", -1, pathW, ARRAY_SIZE(pathW));
    ret = pPathIsUNCEx(pathW, NULL);
    ok(ret == FALSE, "expect FALSE\n");

    for (i = 0; i < ARRAY_SIZE(isuncex_tests); i++)
    {
        const struct isuncex_test *t = isuncex_tests + i;

        MultiByteToWideChar(CP_ACP, 0, t->path, -1, pathW, ARRAY_SIZE(pathW));
        server = (const WCHAR *)0xdeadbeef;
        ret = pPathIsUNCEx(pathW, &server);
        ok(ret == t->ret, "path \"%s\" expect return %d, got %d\n", t->path, t->ret, ret);
        if (ret)
            ok(server == pathW + t->server_offset, "path \"%s\" expect server offset %d, got %ld\n", t->path,
               t->server_offset, (INT_PTR)(server - pathW));
        else
            ok(!server, "expect server is null, got %p\n", server);
    }
}

START_TEST(path)
{
    HMODULE hmod = LoadLibraryA("kernelbase.dll");

    pPathCchCombineEx = (void *)GetProcAddress(hmod, "PathCchCombineEx");
    pPathCchAddBackslash = (void *)GetProcAddress(hmod, "PathCchAddBackslash");
    pPathCchAddBackslashEx = (void *)GetProcAddress(hmod, "PathCchAddBackslashEx");
    pPathCchAddExtension = (void *)GetProcAddress(hmod, "PathCchAddExtension");
    pPathCchFindExtension = (void *)GetProcAddress(hmod, "PathCchFindExtension");
    pPathCchIsRoot = (void *)GetProcAddress(hmod, "PathCchIsRoot");
    pPathCchRemoveExtension = (void *)GetProcAddress(hmod, "PathCchRemoveExtension");
    pPathCchRenameExtension = (void *)GetProcAddress(hmod, "PathCchRenameExtension");
    pPathCchSkipRoot = (void *)GetProcAddress(hmod, "PathCchSkipRoot");
    pPathCchStripPrefix = (void *)GetProcAddress(hmod, "PathCchStripPrefix");
    pPathCchStripToRoot = (void *)GetProcAddress(hmod, "PathCchStripToRoot");
    pPathIsUNCEx = (void *)GetProcAddress(hmod, "PathIsUNCEx");

    test_PathCchCombineEx();
    test_PathCchAddBackslash();
    test_PathCchAddBackslashEx();
    test_PathCchAddExtension();
    test_PathCchFindExtension();
    test_PathCchIsRoot();
    test_PathCchRemoveExtension();
    test_PathCchRenameExtension();
    test_PathCchSkipRoot();
    test_PathCchStripPrefix();
    test_PathCchStripToRoot();
    test_PathIsUNCEx();
}
