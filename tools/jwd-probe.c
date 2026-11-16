/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * jwd-probe - probe a few behaviours that the JustSystems Wine fork ("jwd")
 * changed, to compare them with the Wine you are running.
 *
 * Build:  i686-w64-mingw32-gcc -O1 -o jwd-probe.exe jwd-probe.c -lgdi32 -luser32
 *         (or "make probe")
 * Run:    wine jwd-probe.exe [unix-path-of-a-file-to-test-for-READONLY]
 *
 * Output is UTF-8.  It reports: the ANSI code page; how the JS/JL/DF fonts are
 * enumerated and what CreateFont(SHIFTJIS_CHARSET) really selects; the values
 * of GlobalMemoryStatus(); file attributes and write access for argv[1];
 * WideCharToMultiByte(932) for the characters whose mapping differs between
 * Unicode tables (U+301C etc.); and the current clipboard as CF_UNICODETEXT
 * and CF_TEXT.  See docs/jwd-vs-current-wine.md for the results on Wine 10.0.
 */
#include <windows.h>
#include <stdio.h>

static void pw(const WCHAR *w) { char b[512]; WideCharToMultiByte(CP_UTF8, 0, w, -1, b, sizeof(b), NULL, NULL); fputs(b, stdout); }

static const WCHAR *want[] = {
    L"ＪＳゴシック", L"ＪＳＰゴシック", L"ＪＳ明朝", L"ＪＳ平成明朝体W3", L"JSSGothic-Md", L"ＪＳＳゴシック",
    L"JLゴシック", L"JL明朝", L"ＤＦゴシック体", L"JustEditMark", L"JustHalfMark", NULL };

static int CALLBACK enum_cb(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp)
{
    const WCHAR *n = lf->lfFaceName;
    if (n[0] == '@') return 1;
    if (wcsstr(n, L"ＪＳ") || wcsstr(n, L"JL") || wcsstr(n, L"ＤＦ") || wcsstr(n, L"ＤＨ") || wcsstr(n, L"Just") || wcsstr(n, L"JS"))
    { printf("  enum: "); pw(n); printf("  charset=%d pitch&family=0x%02x\n", lf->lfCharSet, lf->lfPitchAndFamily); }
    return 1;
}

static void try_font(HDC dc, const WCHAR *face, int charset)
{
    LOGFONTW lf = {0}; WCHAR got[LF_FACESIZE]; HFONT f, old; SIZE sz; TEXTMETRICW tm;
    lf.lfHeight = -16; lf.lfCharSet = charset; wcscpy(lf.lfFaceName, face);
    f = CreateFontIndirectW(&lf); old = SelectObject(dc, f);
    GetTextFaceW(dc, LF_FACESIZE, got); GetTextMetricsW(dc, &tm);
    GetTextExtentPoint32A(dc, "\x88\xea\x91\xbe\x98\x59", 6, &sz);   /* "一太郎" in CP932 via the A API */
    printf("  request "); pw(face); printf(" charset=%d -> face ", charset); pw(got);
    printf(" charset=%d, A-API width of 3 kanji = %ld px (expect ~48)\n", tm.tmCharSet, sz.cx);
    SelectObject(dc, old); DeleteObject(f);
}

int main(int argc, char **argv)
{
    HDC dc = CreateCompatibleDC(NULL); LOGFONTW lf = {0}; MEMORYSTATUS ms; int i;

    printf("ACP=%u\n", GetACP());
    printf("[fonts] EnumFontFamiliesEx(DEFAULT_CHARSET):\n");
    lf.lfCharSet = DEFAULT_CHARSET; EnumFontFamiliesExW(dc, &lf, enum_cb, 0, 0);
    printf("[fonts] EnumFontFamiliesEx(SHIFTJIS_CHARSET):\n");
    lf.lfCharSet = SHIFTJIS_CHARSET; EnumFontFamiliesExW(dc, &lf, enum_cb, 0, 0);
    printf("[fonts] CreateFont with SHIFTJIS_CHARSET:\n");
    for (i = 0; want[i]; i++) try_font(dc, want[i], SHIFTJIS_CHARSET);

    ms.dwLength = sizeof(ms); GlobalMemoryStatus(&ms);
    printf("[mem] dwTotalPhys=%lu dwAvailPhys=%lu dwTotalPageFile=%lu\n", ms.dwTotalPhys, ms.dwAvailPhys, ms.dwTotalPageFile);

    if (argc > 1)
    {
        WCHAR path[MAX_PATH]; DWORD a; HANDLE h;
        MultiByteToWideChar(CP_ACP, 0, argv[1], -1, path, MAX_PATH);
        a = GetFileAttributesW(path);
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        printf("[file] %s attrs=0x%lx READONLY=%d, open for write: %s (err %lu)\n", argv[1], a, !!(a & FILE_ATTRIBUTE_READONLY),
               h == INVALID_HANDLE_VALUE ? "FAILED" : "ok", h == INVALID_HANDLE_VALUE ? GetLastError() : 0);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }

    {
        static const WCHAR t[] = { 0x301c, 0xff5e, 0x2212, 0xff0d, 0x2016, 0x2225, 0x00a2, 0xffe0, 0x00a3, 0xffe1, 0x00ac, 0xffe2, 0x2014, 0x2015, 0 };
        char out[8]; BOOL used; int n, k;
        printf("[cp932] WideCharToMultiByte per char (used default char?):\n ");
        for (i = 0; t[i]; i++) { used = FALSE; n = WideCharToMultiByte(932, 0, &t[i], 1, out, sizeof(out), NULL, &used);
            printf(" U+%04X->", t[i]); for (k = 0; k < n; k++) printf("%02X", (unsigned char)out[k]); printf("%s", used ? "(?)" : ""); }
        printf("\n");
    }

    if (OpenClipboard(NULL))
    {
        HANDLE hu = GetClipboardData(CF_UNICODETEXT), ha = GetClipboardData(CF_TEXT);
        printf("[clipboard] CF_UNICODETEXT:");
        if (hu) { const WCHAR *p = GlobalLock(hu); for (; p && *p; p++) printf(" U+%04X", *p); } else printf(" (none)");
        printf("\n[clipboard] CF_TEXT:");
        if (ha) { const unsigned char *p = GlobalLock(ha); for (; p && *p; p++) printf(" %02X", *p); } else printf(" (none)");
        printf("\n");
        CloseClipboard();
    }
    return 0;
}
