#ifndef _HBS_H
#define _HBS_H

#include <zlx.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#if HBS_STATIC
#define HBS_API
#elif HBS_DYNAMIC
#define HBS_API ZLX_LIB_EXPORT
#else
#define HBS_API ZLX_LIB_IMPORT
#endif

typedef uint8_t (ZLX_CALL * hbs_main_func_t) 
    (unsigned int argc, uint8_t const * const * argv);

#if _WIN32
HBS_API int hbs_win_main (int argc, wchar_t const * const * argv, hbs_main_func_t main_func);
#define HBS_MAIN(_func) \
    uint8_t ZLX_CALL _func (unsigned int argc, uint8_t const * const * argv); \
    int wmain (int argc, wchar_t const * const * argv) { \
        return hbs_win_main(argc, argv, _func); }
#else
HBS_API int hbs_posix_main (int argc, char const * const * argv, 
                            hbs_main_func_t main_func);
#define HBS_MAIN(_func) \
    uint8_t ZLX_CALL _func (unsigned int argc, uint8_t const * const * argv); \
    int main (int argc, char const * const * argv) { \
        return hbs_posix_main(argc, argv, _func); }
#endif

typedef enum hbs_status_enum hbs_status_t;

enum hbs_status_enum
{
    HBS_OK = 0,
    HBS_FAILED,
    HBS_NO_MEM,
    HBS_NO_RES,
    HBS_DEADLOCK,
    HBS_ALREADY_JOINING,
    HBS_NO_THREAD,
    HBS_BAD_PATH,
    HBS_BAD_FILE_DESC,

    HBS_TODO = 0x7E,
    HBS_BUG = 0x7F
};

extern HBS_API char const * const hbs_lib_name;
extern HBS_API zlx_file_t * hbs_in;
extern HBS_API zlx_file_t * hbs_out;
extern HBS_API zlx_file_t * hbs_err;
extern HBS_API zlx_ma_t * hbs_ma;

/* hbs_init *****************************************************************/
/**
 *  Inits the library.
 *  This can be safely called multiple times from all the modules in a process
 *  that use its functionality.
 *  This function should be called before any other hbs_XXX() function.
 */
HBS_API hbs_status_t ZLX_CALL hbs_init ();

HBS_API void ZLX_CALL hbs_finish ();

/* hbs_alloc ****************************************************************/
/**
 *  Allocates memory using the allocator defined by this library.
 */
#define hbs_alloc(_size, _info) (zlx_alloc(hbs_ma, (_size), (_info)))

/* hbs_realloc **************************************************************/
/**
 *  Reallocates memory using the allocator defined by this library.
 */
#define hbs_realloc(_old_ptr, _old_size, _new_size) \
    (zlx_realloc(hbs_ma, (_old_ptr), (_old_size), (_new_size)))

/* hbs_free *****************************************************************/
/**
 *  Frees memory using the allocator defined by this library.
 */
#define hbs_free(_ptr, _size) (zlx_free(hbs_ma, (_ptr), (_size)))

/****************************************************************************/
/* multi-threading                                                          */
/****************************************************************************/

typedef uint32_t (* ZLX_CALL hbs_thread_func_t) (void * arg);

typedef uintptr_t hbs_thread_t;
typedef struct hbs_mutex_s hbs_mutex_t;
typedef struct hbs_cond_s hbs_cond_t;

typedef struct hbs_thread_start_s hbs_thread_start_t;
struct hbs_thread_start_s
{
    hbs_thread_func_t func;
    void * arg;
};

/* hbs_thread_create ********************************************************/
/**
 *  Creates a thread.
 */
HBS_API hbs_status_t ZLX_CALL hbs_thread_create
(
    hbs_thread_t * id_p,
    hbs_thread_func_t func,
    void * arg
);

/* hbs_thread_join **********************************************************/
/**
 *  Waits for a thread to finish.
 */
HBS_API hbs_status_t ZLX_CALL hbs_thread_join
(
    hbs_thread_t thread_id,
    uint32_t * ret_p
);

/* hbs_mutex_size ***********************************************************/
/**
 *  Returns the size of a mutex.
 */
extern HBS_API size_t hbs_mutex_size;

/* hbs_mutex_init ***********************************************************/
/**
 *  Initializes a mutex.
 */
HBS_API void ZLX_CALL hbs_mutex_init
(
    hbs_mutex_t * mutex_p
);

/* hbs_mutex_finish *********************************************************/
/**
 *  Finishes a mutex.
 */
HBS_API void ZLX_CALL hbs_mutex_finish
(
    hbs_mutex_t * mutex_p
);

/* hbs_mutex_create *********************************************************/
/**
 *  Allocates and initializes a mutex.
 */
ZLX_INLINE hbs_status_t ZLX_CALL hbs_mutex_create
(
    hbs_mutex_t * * mutex_pp
)
{
    *mutex_pp = hbs_alloc(hbs_mutex_size, "hbs.mutex");
    if (!*mutex_pp) return HBS_NO_MEM;
    hbs_mutex_init(*mutex_pp);
    return HBS_OK;
}

/* hbs_mutex_destroy ********************************************************/
/**
 *  Finishes and frees memory for a given mutex.
 */
ZLX_INLINE void ZLX_CALL hbs_mutex_destroy
(
    hbs_mutex_t * mutex_p
)
{
    hbs_mutex_finish(mutex_p);
    hbs_free(mutex_p, hbs_mutex_size);
}

/* hbs_mutex_lock ***********************************************************/
/**
 *  Locks a mutex.
 */
HBS_API void ZLX_CALL hbs_mutex_lock
(
    hbs_mutex_t * mutex_p
);

/* hbs_mutex_unlock *********************************************************/
/**
 *  Unlocks a mutex.
 */
HBS_API void ZLX_CALL hbs_mutex_unlock
(
    hbs_mutex_t * mutex_p
);

/* hbs_cond_size ************************************************************/
/**
 *  The size of a condition variable.
 *  This is guaranteed to be initialized after a call hbs_init().
 */
extern HBS_API size_t hbs_cond_size;

/* hbs_cond_init ************************************************************/
/**
 *  Inits a conditional variable.
 */
HBS_API hbs_status_t ZLX_CALL hbs_cond_init
(
    hbs_cond_t * cond_p
);

/* hbs_cond_finish **********************************************************/
/**
 *  Frees resources used by the condition variable.
 */
HBS_API void ZLX_CALL hbs_cond_finish
(
    hbs_cond_t * cond_p
);

/* hbs_cond_signal **********************************************************/
/**
 *  Signals a condition variable.
 */
HBS_API void ZLX_CALL hbs_cond_signal
(
    hbs_cond_t * cond_p
);

/* hbs_cond_wait ************************************************************/
/**
 *  Waits for the condition variable.
 */
HBS_API void ZLX_CALL hbs_cond_wait
(
    hbs_cond_t * cond_p,
    hbs_mutex_t * mutex
);

/* hbs_cond_create **********************************************************/
ZLX_INLINE hbs_status_t hbs_cond_create (hbs_cond_t * * cond_pp)
{
    hbs_status_t hs;
    *cond_pp = hbs_alloc(hbs_cond_size, "hbs.cond");
    if (!*cond_pp) return HBS_NO_MEM;
    hs = hbs_cond_init(*cond_pp);
    if (hs) hbs_free(*cond_pp, hbs_cond_size); 
    return hs;
}

/* hbs_cond_destroy *********************************************************/
ZLX_INLINE void hbs_cond_destroy (hbs_cond_t * cond_p)
{
    hbs_cond_finish(cond_p);
    hbs_free(cond_p, hbs_cond_size);
}

/****************************************************************************/
/* host file system                                                         */
/****************************************************************************/

/* hbs_file_from_posix_fd ***************************************************/
/**
 *  Generates a file object from a file descriptor.
 *  @param fp [out]
 *      pointer to be filled with the pointer to the newly created file object
 *  @param fd [in]
 *      existing file descriptor
 *  @param flags [in]
 *      bitmask of any combination of: #ZLXF_READ, #ZLXF_WRITE, #ZLXF_NONBLOCK
 *  @note
 *      the file descriptor will be tested for seek support and if it has 
 *      support zlx_file_t#flags gets #ZLXF_SEEK bit set.
 */
HBS_API hbs_status_t ZLX_CALL hbs_file_from_posix_fd
(
    zlx_file_t * * fp,
    int fd,
    uint32_t flags
);

/* hbs_file_from_windows_handle *********************************************/
HBS_API hbs_status_t ZLX_CALL hbs_file_from_windows_handle
(
    zlx_file_t * * fp,
    void * file_hnd,
    uint32_t flags
);


/* hbs_file_open_ro *********************************************************/
/**
 *  Opens a file in read-only mode.
 *  @note
 *      on Windows the file is open with all FILE_SHARE_xxx flags
 */
HBS_API hbs_status_t ZLX_CALL hbs_file_open_ro
(
    zlx_file_t * * fp,
    uint8_t const * path // UTF8 encoded NUL terminated string
);

/* hbs_file_open_rw *********************************************************/
/**
 *  Opens a file in read-write mode.
 */
HBS_API hbs_status_t ZLX_CALL hbs_file_open_rw
(
    zlx_file_t * * fp,
    uint8_t const * path // UTF8 encoded NUL terminated string
);

/* hbs_file_free ************************************************************/
/**
 *  Frees memory used by the file object.
 *  @param f [in]
 *      file obtained by one of the APIs from this library that open/create
 *      files
 *  @note
 *      this function does not "close" the file.
 */
HBS_API void ZLX_CALL hbs_file_free
(
    zlx_file_t * f
);

/* hbs_file_close ***********************************************************/
HBS_API zlx_file_status_t ZLX_CALL hbs_file_close
(
    zlx_file_t * f
);

HBS_API void hbs_log_init (zlx_file_t * ZLX_RESTRICT file, unsigned int level);

extern HBS_API zlx_log_t * hbs_log;

#define HBS_LF(...) (ZLX_LF(hbs_log, __VA_ARGS__))
#define HBS_LE(...) (ZLX_LE(hbs_log, __VA_ARGS__))
#define HBS_LW(...) (ZLX_LW(hbs_log, __VA_ARGS__))
#define HBS_LI(...) (ZLX_LI(hbs_log, __VA_ARGS__))
#define HBS_LD(...) (ZLX_LD(hbs_log, __VA_ARGS__))

#define HBS_DM(_fmt, ...) \
    (ZLX_LD(hbs_log, "$s:$i:$s(): " _fmt "\n", \
            __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__))

#define HBS_DMX(_fmt, ...) ((void) 0)

#endif

