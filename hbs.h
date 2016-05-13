#ifndef _HBS_H
#define _HBS_H

/** @mainpage Host Basic Services
 *
 *  This is a library implementing interfaces defined in Zalmoxis library.
 *  Here is all OS-specific functionality implemented, allowing a wide set of portable
 *  tools to depend only on zlx and hbs.
 *
 *  @section License
 *
 *  Copyright (c) 2016, Costin Ionescu <costin.ionescu@gmail.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  
 *  Note: ISC license (functionally equivalent to simplified BSD and MIT/Expat)
 */

/** @defgroup hbs Host Basic Services
 *  @{
 */

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

#ifdef __cplusplus
extern "C" {
#endif

/* hbs_main_func_t **********************************************************/
/**
 *  Function pointer type for the @a main function of a program.
 */
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

/*  HBS_MAIN  */
/**
 *  Macro that implements the target environment's execution stub that calls the user provided
 *  main function.
 */
#define HBS_MAIN(_func) \
    uint8_t ZLX_CALL _func (unsigned int argc, uint8_t const * const * argv); \
    int main (int argc, char const * const * argv) { \
        return hbs_posix_main(argc, argv, _func); }
#endif

/*  hbs_status_t  */
/**
 *  Status code for hbs library-specific functions.
 */
typedef enum hbs_status_enum hbs_status_t;

enum hbs_status_enum
{
    /** Success */
    HBS_OK = 0,

    /** Generic failure status */
    HBS_FAILED,

    /** Not enough memory */
    HBS_NO_MEM,

    /** Not enough resources */
    HBS_NO_RES,

    /** Deadlock detected */
    HBS_DEADLOCK,

    /** Thread already joining another thread */
    HBS_ALREADY_JOINING,

    /** No such thread */
    HBS_NO_THREAD,

    /** Invalid path */
    HBS_BAD_PATH,

    /** Invalid file descriptor */
    HBS_BAD_FILE_DESC,

    /** Functionality not implemented yet */
    HBS_TODO = 0x7E,

    /** Bug detected. On debug and checked builds an assert usually trips 
     *  instead of receiving this status code, but in some cases this 
     *  can be obtained even in release. */
    HBS_BUG = 0x7F
};

/*  hbs_lib_name  */
/**
 *  Static string describing the flavour of hbs library.
 */
extern HBS_API char const * const hbs_lib_name;

/*  hbs_in  */
/**
 *  File object for application's standard input.
 *  This is valid after hbs_init().
 */
extern HBS_API zlx_file_t * hbs_in;

/*  hbs_out  */
/**
 *  File object for application's standard output.
 *  This is valid after hbs_init().
 */
extern HBS_API zlx_file_t * hbs_out;

/*  hbs_err  */
/**
 *  File object for application's standard error.
 *  This is valid after hbs_init().
 */
extern HBS_API zlx_file_t * hbs_err;


/*  hbs_ma  */
/**
 *  The default memory allocator.
 */
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

/* hbs_thread_create ********************************************************/
/**
 *  Creates a thread.
 */
HBS_API zlx_mth_status_t ZLX_CALL hbs_thread_create
    (
        zlx_tid_t * tid_p,
        zlx_thread_func_t func,
        void * arg
    );

/* hbs_thread_join **********************************************************/
/**
 *  Waits for a thread to finish.
 */
HBS_API zlx_mth_status_t ZLX_CALL hbs_thread_join
    (
        zlx_tid_t tid,
        uint8_t * ret_val_p
    );

/* hbs_mutex_size ***********************************************************/
/**
 *  Gives the size of a mutex.
 */
extern HBS_API size_t hbs_mutex_size;

/* hbs_mutex_init ***********************************************************/
/**
 *  Initializes a mutex.
 */
HBS_API void ZLX_CALL hbs_mutex_init
(
    zlx_mutex_t * mutex_p
);

/* hbs_mutex_finish *********************************************************/
/**
 *  Finishes a mutex.
 */
HBS_API void ZLX_CALL hbs_mutex_finish
(
    zlx_mutex_t * mutex_p
);

/* hbs_mutex_create *********************************************************/
/**
 *  Allocates and initializes a mutex.
 */
#define hbs_mutex_create(_info) \
    (zlx_mutex_create(hbs_ma, &hbs_mth_xfc.mutex, (_info)))

/* hbs_mutex_destroy ********************************************************/
/**
 *  Finishes and frees memory for a given mutex.
 */
#define hbs_mutex_destroy(_mutex) \
    (zlx_mutex_destroy((_mutex), hbs_ma, &hbs_mth_xfc.mutex))

/* hbs_mutex_lock ***********************************************************/
/**
 *  Locks a mutex.
 */
HBS_API void ZLX_CALL hbs_mutex_lock
(
    zlx_mutex_t * mutex_p
);

/* hbs_mutex_unlock *********************************************************/
/**
 *  Unlocks a mutex.
 */
HBS_API void ZLX_CALL hbs_mutex_unlock
(
    zlx_mutex_t * mutex_p
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
HBS_API zlx_mth_status_t ZLX_CALL hbs_cond_init
(
    zlx_cond_t * cond_p
);

/* hbs_cond_finish **********************************************************/
/**
 *  Frees resources used by the condition variable.
 */
HBS_API void ZLX_CALL hbs_cond_finish
(
    zlx_cond_t * cond_p
);

/* hbs_cond_signal **********************************************************/
/**
 *  Signals a condition variable.
 */
HBS_API void ZLX_CALL hbs_cond_signal
(
    zlx_cond_t * cond_p
);

/* hbs_cond_wait ************************************************************/
/**
 *  Waits for the condition variable.
 */
HBS_API void ZLX_CALL hbs_cond_wait
(
    zlx_cond_t * cond,
    zlx_mutex_t * mutex
);

/* hbs_cond_create **********************************************************/
/**
 *  Allocates and initializes a condition variable.
 */
#define hbs_cond_create(_status_p, _info) \
    (zlx_cond_create(hbs_ma, &hbs_mth_xfc.cond, (_status_p), (_info)))

/*  hbs_cond_destroy  */
/**
 *  Uninitializes and deallocates a condition variable.
 */
#define hbs_cond_destroy(_cond) \
    (zlx_cond_destroy((_cond), hbs_ma, &hbs_mth_xfc.cond))

/****************************************************************************/
/* host file system                                                         */
/****************************************************************************/

/* hbs_file_from_posix_fd ***************************************************/
/**
 *  Generates a file object from a POSIX file descriptor.
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
/**
 *  Generates a file object from a Windows file handle.
 *  @warning this function is available only on Windows.
 */
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

/* hbs_log_init *************************************************************/
/**
 *  Initializes the global logger of this library.
 *  The global logger is exposed as #hbs_log.
 */
HBS_API void hbs_log_init (zlx_file_t * ZLX_RESTRICT file, unsigned int level);

/* hbs_log ******************************************************************/
/**
 *  Global logger maintained by this library.
 */
extern HBS_API zlx_log_t * hbs_log;

/*  HBS_LF  */
/**
 *  Logs a fault message.
 */
#define HBS_LF(...) (ZLX_LF(hbs_log, __VA_ARGS__))

/*  HBS_LE  */
/**
 *  Logs an error message.
 */
#define HBS_LE(...) (ZLX_LE(hbs_log, __VA_ARGS__))

/*  HBS_LW  */
/**
 *  Logs a warning message.
 */
#define HBS_LW(...) (ZLX_LW(hbs_log, __VA_ARGS__))

/*  HBS_LI  */
/**
 *  Logs an informational message.
 */
#define HBS_LI(...) (ZLX_LI(hbs_log, __VA_ARGS__))


/*  HBS_LD  */
/**
 *  Logs a debug message.
 *  This macro generates code only on debug builds (_DEBUG defined as non-zero)
 */
#define HBS_LD(...) (ZLX_LD(hbs_log, __VA_ARGS__))

/*  HBS_DM  */
/**
 *  Decorated debug message.
 *  This macro generates debug messages prefixed by the code location
 */
#define HBS_DM(_fmt, ...) \
    (ZLX_LD(hbs_log, "$s:$i:$s(): " _fmt "\n", \
            __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__))

/*  HBS_DMX  */
/**
 *  Provides a quick and convenient way of disabling instances of #HBS_DM.
 */
#define HBS_DMX(_fmt, ...) ((void) 0)


HBS_API zlx_mth_xfc_t hbs_mth_xfc;


#ifdef __cplusplus
}
#endif

/** @} */

#endif

