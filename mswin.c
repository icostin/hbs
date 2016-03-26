#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <zlx.h>
#include "hbs.h"
#include "intern.h"

#if _DEBUG
#define L(...) (fprintf(stderr, "%s:%u:in %s(): ", __FILE__, __LINE__, __FUNCTION__), fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"), fflush(stderr))
#else
#define L(...) ((void) 0)
#endif

typedef struct file_s file_t;
struct file_s
{
    zlx_file_t base;
    HANDLE h;
};

typedef struct mswin_ma_s mswin_ma_t;
struct mswin_ma_s
{
    zlx_ma_t base;
    HANDLE heap_hnd;
};

static void * ZLX_CALL mswin_realloc
(
    void * old_ptr,
    size_t old_size,
    size_t new_size,
    zlx_ma_t * ma
);

static ptrdiff_t ZLX_CALL file_read
(
    zlx_file_t * f,
    uint8_t * data,
    size_t size
);

static ptrdiff_t ZLX_CALL file_write
(
    zlx_file_t * f,
    uint8_t const * data,
    size_t size
);

static int64_t ZLX_CALL file_seek64
(
    zlx_file_t * f,
    int64_t offset,
    int anchor
);

static zlx_file_status_t ZLX_CALL file_truncate
(
    zlx_file_t * f
);

static zlx_file_status_t ZLX_CALL file_close
(
    zlx_file_t * f,
    unsigned int flags // ZLXF_READ | ZLXF_WRITE
);

/*
 * nice read: http://www.cs.wustl.edu/~schmidt/win32-cv-1.html
 * for now I implemented the "unfair" combination of:
 * - critical section for mutex &
 * - auto-reset events for condition variables.
 * The unfairness described in the article applies to broadcasting which
 * this implemention does not offer.
 */
//static uint8_t got_cond_var = 0;
static mswin_ma_t mswin_ma =
{
    {
        mswin_realloc,
        zlx_ma_nop_info_set,
        zlx_ma_nop_check
    },
    NULL
};

static zlx_file_class_t const file_class =
{
    file_read,
    file_write,
    file_seek64,
    file_truncate,
    file_close,
    "mswin-file"
};

static volatile int inited = 0;

HBS_API zlx_ma_t * hbs_ma = &mswin_ma.base;
HBS_API size_t hbs_mutex_size = sizeof(CRITICAL_SECTION);
HBS_API size_t hbs_cond_size = 0;
HBS_API zlx_file_t * hbs_in = NULL;
HBS_API zlx_file_t * hbs_out = NULL;
HBS_API zlx_file_t * hbs_err = NULL;


/* hbs_init *****************************************************************/
HBS_API hbs_status_t ZLX_CALL hbs_init ()
{
    HANDLE h;
    hbs_status_t hs;

    if (inited) return HBS_OK;
    mswin_ma.heap_hnd = GetProcessHeap();
    L("heap=%p", mswin_ma.heap_hnd);
    //got_cond_var = ((uint8_t) GetVersion() >= 6);
    hbs_cond_size =
        //got_cond_var ? sizeof(CONDITION_VARIABLE) :
        sizeof(HANDLE);

    h = GetStdHandle(STD_INPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) hbs_in = &zlx_null_file;
    else
    {
        hs = hbs_file_from_windows_handle(&hbs_in, h, ZLXF_READ);
        if (hs) return hs;
    }

    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) hbs_out = &zlx_null_file;
    else
    {
        hs = hbs_file_from_windows_handle(&hbs_out, h, ZLXF_WRITE);
        if (hs)
        {
            hbs_file_free(hbs_in);
            return hs;
        }
    }

    h = GetStdHandle(STD_ERROR_HANDLE);
    if (h == INVALID_HANDLE_VALUE) hbs_err = &zlx_null_file;
    else
    {
        hs = hbs_file_from_windows_handle(&hbs_err, h, ZLXF_WRITE);
        if (hs)
        {
            hbs_file_free(hbs_in);
            hbs_file_free(hbs_out);
            return hs;
        }
    }

    hbs_log_init(hbs_err, 
#if _DEBUG
                 ZLX_LL_DEBUG
#else
                 ZLX_LL_ERROR
#endif
                );
    zlx_abort = &abort;
    inited = 1;
    return HBS_OK;
}

/* hbs_finish ***************************************************************/
HBS_API void ZLX_CALL hbs_finish ()
{
    hbs_ma = &mswin_ma.base;
    hbs_file_free(hbs_in);
    hbs_file_free(hbs_out);
    hbs_file_free(hbs_err);
    inited = 0;
}

static void * ZLX_CALL mswin_realloc
(
    void * old_ptr,
    size_t old_size,
    size_t new_size,
    zlx_ma_t * ma
)
{
    mswin_ma_t * ZLX_RESTRICT hma = (mswin_ma_t *) ma;
    (void) old_size;
#if 0
    L("op=%p, os=%lu, ns=%lu, heap=%p",
      old_ptr, (long) old_size, (long) new_size, hma->heap_hnd);
#endif
    if (old_ptr) {
        if (!new_size) { HeapFree(hma->heap_hnd, 0, old_ptr); return NULL; }
        return HeapReAlloc(hma->heap_hnd, 0, old_ptr, new_size);
    }
    return HeapAlloc(hma->heap_hnd, 0, new_size);
}

/* thread_stub **************************************************************/
static DWORD WINAPI thread_stub (void * ts_ptr)
{
    hbs_thread_start_t * ts = ts_ptr;
    hbs_thread_func_t func = ts->func;
    void * arg = ts->arg;

    HeapFree(GetProcessHeap(), 0, ts);
    return func(arg);
}

/* hbs_thread_create ********************************************************/
HBS_API hbs_status_t ZLX_CALL hbs_thread_create
(
    hbs_thread_t * id_p,
    hbs_thread_func_t func,
    void * arg
)
{
    hbs_thread_start_t * ts;

    ts = HeapAlloc(GetProcessHeap(), 0, sizeof(*ts));
    if (!ts) return HBS_NO_MEM;
    ts->func = func;
    ts->arg = arg;
    *id_p = (uintptr_t) CreateThread(NULL, 0, thread_stub, ts, 0, NULL);
    if (!*id_p) return HBS_FAILED;
    return HBS_OK;
}

/* hbs_thread_join **********************************************************/
HBS_API hbs_status_t ZLX_CALL hbs_thread_join
(
    hbs_thread_t thread_id,
    uint32_t * ret_p
)
{
    DWORD d;
    if (WaitForSingleObject((HANDLE) thread_id, INFINITE)) return HBS_FAILED;
    if (!GetExitCodeThread((HANDLE) thread_id, &d)) return HBS_FAILED;
    if (ret_p) *ret_p = d;
    return HBS_OK;
}

/* hbs_mutex_init ***********************************************************/
HBS_API void ZLX_CALL hbs_mutex_init
(
    hbs_mutex_t * mutex_p
)
{
    InitializeCriticalSection((CRITICAL_SECTION *) mutex_p);
}

/* hbs_mutex_finish *********************************************************/
HBS_API void ZLX_CALL hbs_mutex_finish
(
    hbs_mutex_t * mutex_p
)
{
    DeleteCriticalSection((CRITICAL_SECTION *) mutex_p);
}

/* hbs_mutex_lock ***********************************************************/
HBS_API void ZLX_CALL hbs_mutex_lock
(
    hbs_mutex_t * mutex_p
)
{
    EnterCriticalSection((CRITICAL_SECTION *) mutex_p);
}

/* hbs_mutex_unlock *********************************************************/
HBS_API void ZLX_CALL hbs_mutex_unlock
(
    hbs_mutex_t * mutex_p
)
{
    LeaveCriticalSection((CRITICAL_SECTION *) mutex_p);
}

/* hbs_cond_init ************************************************************/
HBS_API hbs_status_t ZLX_CALL hbs_cond_init
(
    hbs_cond_t * cond_p
)
{
    HANDLE h;
    h = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!h) return HBS_FAILED;
    *(HANDLE *) cond_p = h;
    return HBS_OK;
}

/* hbs_cond_finish **********************************************************/
HBS_API void ZLX_CALL hbs_cond_finish
(
    hbs_cond_t * cond_p
)
{
    CloseHandle(*(HANDLE *) cond_p);
}

/* hbs_cond_signal **********************************************************/
HBS_API void ZLX_CALL hbs_cond_signal
(
    hbs_cond_t * cond_p
)
{
    SetEvent(*(HANDLE *) cond_p);
}

/* hbs_cond_wait ************************************************************/
HBS_API void ZLX_CALL hbs_cond_wait
(
    hbs_cond_t * cond_p,
    hbs_mutex_t * mutex_p
)
{
    hbs_mutex_unlock(mutex_p);
    WaitForSingleObject(*(HANDLE *) cond_p, INFINITE);
    hbs_mutex_lock(mutex_p);
}

/* hbs_file_from_windows_handle *********************************************/
HBS_API hbs_status_t ZLX_CALL hbs_file_from_windows_handle
(
    zlx_file_t * * fp,
    void * file_hnd,
    uint32_t flags
)
{
    file_t * f;
    LARGE_INTEGER p;
    p.QuadPart = 0;

    if (SetFilePointerEx(file_hnd, p, NULL, FILE_CURRENT)) flags |= ZLXF_SEEK;

    f = hbs_alloc(sizeof(file_t), "hbs.mswin.file");
    if (!f) return HBS_NO_MEM;
    f->base.fcls = &file_class;
    f->base.flags = flags;
    f->h = file_hnd;
    *fp = &f->base;

    return HBS_OK;
}

/* hbs_file_open_ro *********************************************************/
HBS_API hbs_status_t ZLX_CALL hbs_file_open_ro
(
    zlx_file_t * * fp,
    uint8_t const * path // UTF8 encoded NUL terminated string
)
{
    HANDLE h;
    WCHAR buf[0x104];
    WCHAR *wp;
    ptrdiff_t l;
    size_t path_len;
    hbs_status_t hs;

    path_len = strlen((char const *) path) + 1;

    l = zlx_uconv(path, path_len,
                  ZLX_UTF8_DEC | ZLX_UTF16LE_ENC
                  | ZLX_UTF8_DEC_TWO_BYTE_NUL | ZLX_UTF8_DEC_SURROGATES,
                  (uint8_t *) buf, sizeof(buf), NULL);
    if (l < 0) return HBS_BAD_PATH;
    if ((size_t) l <= sizeof(buf)) wp = &buf[0];
    else
    {
        wp = hbs_alloc(l, "temp path");
        if (!wp) return HBS_NO_MEM;
        l = zlx_uconv(path, path_len,
                      ZLX_UTF8_DEC | ZLX_UTF16LE_ENC
                      | ZLX_UTF8_DEC_TWO_BYTE_NUL | ZLX_UTF8_DEC_SURROGATES,
                      (uint8_t *) wp, l, NULL);
    }
    h = CreateFileW(wp, GENERIC_READ,
                    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, 0, NULL);
    if (wp != &buf[0]) hbs_free(wp, l);
    if (h == INVALID_HANDLE_VALUE)
    {
        return HBS_FAILED;
    }

    hs = hbs_file_from_windows_handle(fp, h, ZLXF_READ);
    if (hs) CloseHandle(h);

    return hs;
}

/* hbs_file_free ************************************************************/
HBS_API void ZLX_CALL hbs_file_free
(
    zlx_file_t * f
)
{
    hbs_free(f, sizeof(file_t));
}

/* file_read ****************************************************************/
static ptrdiff_t ZLX_CALL file_read
(
    zlx_file_t * zf,
    uint8_t * data,
    size_t size
)
{
    file_t * ZLX_RESTRICT f = (file_t * ZLX_RESTRICT) zf;
    DWORD r, e;

    if (size >= ((size_t) 1 << 31)) return -ZLXF_SIZE_LIMIT;
    if (ReadFile(f->h, data, (uint32_t) size, &r, NULL)) return r;
    e = GetLastError();
    switch (e)
    {
    default:
        return -ZLXF_FAILED;
    }
}

/* file_write ***************************************************************/
static ptrdiff_t ZLX_CALL file_write
(
    zlx_file_t * zf,
    uint8_t const * data,
    size_t size
)
{
    file_t * ZLX_RESTRICT f = (file_t * ZLX_RESTRICT) zf;
    DWORD w, e;

    if (size >= ((size_t) 1 << 31)) return -ZLXF_SIZE_LIMIT;
    if (WriteFile(f->h, data, (uint32_t) size, &w, NULL)) return w;
    e = GetLastError();
    switch (e)
    {
    default:
        return -ZLXF_FAILED;
    }
}

/* file_seek64 **************************************************************/
static int64_t ZLX_CALL file_seek64
(
    zlx_file_t * zf,
    int64_t offset,
    int anchor
)
{
    file_t * f = (file_t *) zf;
    LARGE_INTEGER p;
    DWORD e;

    p.QuadPart = offset;

    if (SetFilePointerEx(f->h, p, &p, anchor)) return (int64_t) p.QuadPart;
    e = GetLastError();
    switch (e)
    {
    default:
        return -ZLXF_FAILED;
    }
}


/* file_truncate ************************************************************/
static zlx_file_status_t ZLX_CALL file_truncate
(
    zlx_file_t * zf
)
{
    file_t * f = (file_t *) zf;
    DWORD e;

    if (SetEndOfFile(f->h)) return ZLXF_OK;
    e = GetLastError();
    switch (e)
    {
    default:
        return -ZLXF_FAILED;
    }
}

/* file_close ***************************************************************/
static zlx_file_status_t ZLX_CALL file_close
(
    zlx_file_t * zf,
    unsigned int flags
)
{
    file_t * f = (file_t *) zf;
    DWORD e;

    flags = f->base.flags & ~flags;
    if ((flags & (ZLXF_READ | ZLXF_WRITE)) || CloseHandle(f->h))
    {
        f->base.flags = flags;
        return ZLXF_OK;
    }
    e = GetLastError();
    L("close error: %u", (int) e);
    switch (e)
    {
    default:
        return ZLXF_FAILED;
    }
}

/* hbs_win_main *************************************************************/
HBS_API int hbs_win_main (int argc, wchar_t const * const * argv,
                  hbs_main_func_t main_func)
{
    uint8_t * * av;
    unsigned int ac, r, i;
    ptrdiff_t l;
    size_t il;

    ac = argc;
    av = malloc(argc * sizeof(uint8_t const *));
    if (!av)
    {
        fprintf(stderr, "hbs error: no mem for argv conversion!\n");
        return 127;
    }

    for (i = 0; i < ac; ++i)
    {
        il = wcslen(argv[i]) * 2;
        l = zlx_utf16le_to_utf8_len((uint8_t const *) argv[i], il,
                                    ZLX_UTF16_DEC_UNPAIRED_SURROGATES);
        if (l < 0) 
        {
            fprintf(stderr, "hbs error: failed converting arg strings (%ld)!\n",
                    (long) l);
            return 127;
        }
        av[i] = malloc(l + 1);
        zlx_utf16le_to_utf8((uint8_t const *) argv[i], il,
                            ZLX_UTF16_DEC_UNPAIRED_SURROGATES, av[i]);
        av[i][l] = 0;
    }

    r = main_wrap(ac, (uint8_t const * const *) av, main_func);

    for (i = 0; i < ac; ++i) free(av[i]);
    free(av);

    return r;
}

#endif

