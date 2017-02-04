#ifndef _WIN32
#define _LARGEFILE64_SOURCE
#define _BSD_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "hbs.h"
#include "intern.h"

typedef struct file_s file_t;
struct file_s
{
    zlx_file_t base;
    int fd;
};

static void * ZLX_CALL posix_realloc
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

static zlx_ma_t posix_ma =
{
    posix_realloc,
    zlx_ma_nop_info_set,
    zlx_ma_nop_check
};

static zlx_file_class_t const file_class =
{
    file_read,
    file_write,
    file_seek64,
    file_truncate,
    file_close,
    "posix-file"
};

static volatile int inited = 0;

HBS_API size_t hbs_mutex_size = sizeof(pthread_mutex_t);
HBS_API size_t hbs_cond_size = sizeof(pthread_cond_t);
HBS_API zlx_file_t * hbs_in = NULL;
HBS_API zlx_file_t * hbs_out = NULL;
HBS_API zlx_file_t * hbs_err = NULL;
HBS_API zlx_ma_t * hbs_ma = &posix_ma;

HBS_API zlx_mth_xfc_t hbs_mth_xfc =
{
    /* thread */
    {
        hbs_thread_create,
        hbs_thread_join
    },
    /* mutex */
    {
        hbs_mutex_init,
        hbs_mutex_finish,
        hbs_mutex_lock,
        hbs_mutex_unlock,
        sizeof(pthread_mutex_t)
    },
    /* cond */
    {
        hbs_cond_init,
        hbs_cond_finish,
        hbs_cond_signal,
        hbs_cond_wait,
        sizeof(pthread_cond_t)
    }
};

/* hbs_init *****************************************************************/
HBS_API hbs_status_t ZLX_CALL hbs_init ()
{
    hbs_status_t hs;
    if (inited) return HBS_OK;

    zlx_abort = &abort;

    hs = hbs_file_from_posix_fd(&hbs_in, 0, ZLXF_READ);
    if (hs) return hs;
    hs = hbs_file_from_posix_fd(&hbs_out, 1, ZLXF_WRITE);
    if (hs) return hs;
    hs = hbs_file_from_posix_fd(&hbs_err, 2, ZLXF_WRITE);
    if (hs) return hs;
    hbs_log_init(hbs_err, 
#if _DEBUG
                 ZLX_LL_DEBUG
#else
                 ZLX_LL_ERROR
#endif
                );
    inited = 1;
    return HBS_OK;
}

/* hbs_finish ***************************************************************/
HBS_API void ZLX_CALL hbs_finish ()
{
    if (hbs_in) { hbs_file_free(hbs_in); hbs_in = NULL; }
    if (hbs_out) { hbs_file_free(hbs_out); hbs_out = NULL; }
    if (hbs_err) { hbs_file_free(hbs_err); hbs_err = NULL; }
    inited = 0;
}

/* posix_realloc ************************************************************/
static void * ZLX_CALL posix_realloc
(
    void * old_ptr,
    size_t old_size,
    size_t new_size,
    zlx_ma_t * ma
)
{
    (void) old_size; (void) ma;
    return realloc(old_ptr, new_size);
}

/* thread_stub **************************************************************/
static void * thread_stub (void * ts_ptr)
{
    hbs_thread_start_t * ts = ts_ptr;
    zlx_thread_func_t func = ts->func;
    void * arg = ts->arg;
    free(ts);
    return (void *) (uintptr_t) func(arg);
}

/* hbs_thread_create ********************************************************/
HBS_API zlx_mth_status_t ZLX_CALL hbs_thread_create
    (
        zlx_tid_t * tid_p,
        zlx_thread_func_t func,
        void * arg
    )
{
    int r;
    hbs_thread_start_t * ts;

    ts = malloc(sizeof(*ts));
    if (!ts) return ZLX_MTH_NO_MEM;
    ts->func = func;
    ts->arg = arg;
    r = pthread_create((pthread_t *) tid_p, NULL, thread_stub, ts);
    switch (r)
    {
    case 0: return ZLX_MTH_OK;
    case EAGAIN: return ZLX_MTH_NO_RES;
    default: return ZLX_MTH_FAILED;
    }
}

/* hbs_thread_join **********************************************************/
HBS_API zlx_mth_status_t ZLX_CALL hbs_thread_join
    (
        zlx_tid_t tid,
        uint8_t * ret_p
    )
{
    void * ret;
    int r;

    r = pthread_join(tid, &ret);
    switch (r)
    {
    case 0: if (ret_p) *ret_p = (uint32_t) (uintptr_t) ret; return ZLX_MTH_OK;
    case EDEADLK: return ZLX_MTH_DEADLOCK;
    case EINVAL: return ZLX_MTH_ALREADY_JOINING;
    case ESRCH: return ZLX_MTH_NO_THREAD;
    default: return ZLX_MTH_FAILED;
    }
}

/* hbs_mutex_init ***********************************************************/
HBS_API void ZLX_CALL hbs_mutex_init
(
    zlx_mutex_t * mutex_p
)
{
    pthread_mutex_init((pthread_mutex_t *) mutex_p, NULL);
}


/* hbs_mutex_finish *********************************************************/
HBS_API void ZLX_CALL hbs_mutex_finish
(
    zlx_mutex_t * mutex_p
)
{
    pthread_mutex_destroy((pthread_mutex_t *) mutex_p);
}

/* hbs_mutex_lock ***********************************************************/
HBS_API void ZLX_CALL hbs_mutex_lock
(
    zlx_mutex_t * mutex_p
)
{
    pthread_mutex_lock((pthread_mutex_t *) mutex_p);
}

/* hbs_mutex_unlock *********************************************************/
HBS_API void ZLX_CALL hbs_mutex_unlock
(
    zlx_mutex_t * mutex_p
)
{
    pthread_mutex_unlock((pthread_mutex_t *) mutex_p);
}

/* hbs_cond_init ************************************************************/
HBS_API zlx_mth_status_t ZLX_CALL hbs_cond_init
(
    zlx_cond_t * cond_p
)
{
    pthread_cond_init((pthread_cond_t *) cond_p, NULL);
    return ZLX_MTH_OK;
}

/* hbs_cond_finish **********************************************************/
HBS_API void ZLX_CALL hbs_cond_finish
(
    zlx_cond_t * cond_p
)
{
    pthread_cond_destroy((pthread_cond_t *) cond_p);
}

/* hbs_cond_signal **********************************************************/
HBS_API void ZLX_CALL hbs_cond_signal
(
    zlx_cond_t * cond_p
)
{
    pthread_cond_signal((pthread_cond_t *) cond_p);
}

/* hbs_cond_wait ************************************************************/
HBS_API void ZLX_CALL hbs_cond_wait
(
    zlx_cond_t * cond_p,
    zlx_mutex_t * mutex_p
)
{
    pthread_cond_wait((pthread_cond_t *) cond_p, (pthread_mutex_t *) mutex_p);
}

/* hbs_file_from_posix_fd ***************************************************/
HBS_API hbs_status_t ZLX_CALL hbs_file_from_posix_fd
(
    zlx_file_t * * fp,
    int fd,
    uint32_t flags
)
{
    file_t * f;

    if (lseek(fd, 0, SEEK_CUR) < 0)
    {
        if (errno == EBADF) return HBS_BAD_FILE_DESC;
    }
    else flags |= ZLXF_SEEK;
    f = malloc(sizeof(file_t));
    if (!f) return HBS_NO_MEM;
    f->base.fcls = &file_class;
    f->base.flags |= flags;
    f->fd = fd;
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
    hbs_status_t hs;
    int fd;

    fd = open((char const *) path, O_RDONLY);
    if (fd < 0)
    {
        return HBS_FAILED;
    }
    hs = hbs_file_from_posix_fd(fp, fd, ZLXF_READ | ZLXF_WRITE);
    if (hs)
    {
        close(fd);
    }
    return hs;
}

/* hbs_file_free ************************************************************/
HBS_API void ZLX_CALL hbs_file_free
(
    zlx_file_t * f
)
{
    free(f);
}

/* file_read ****************************************************************/
static ptrdiff_t ZLX_CALL file_read
(
    zlx_file_t * zf,
    uint8_t * data,
    size_t size
)
{
    file_t * restrict f = (file_t *) zf;
    ssize_t z;

    z = read(f->fd, data, size);
    if (z < 0)
    {
        switch (errno)
        {
        case EAGAIN:
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return -ZLXF_WOULD_BLOCK;
        case EFAULT:
            return -ZLXF_BAD_BUFFER;
        case EINTR:
            return -ZLXF_INTERRUPTED;
        case EINVAL:
            return -ZLXF_BAD_OPERATION;
        case EIO:
            return -ZLXF_IO_ERROR;
        case EBADF:
            return -ZLXF_BAD_FILE_DESC;
        default:
            return -ZLXF_FAILED;
        }
    }
    return (ptrdiff_t) z;
}

/* file_write ***************************************************************/
static ptrdiff_t ZLX_CALL file_write
(
    zlx_file_t * zf,
    uint8_t const * data,
    size_t size
)
{
    file_t * restrict f = (file_t *) zf;
    ssize_t z;
    z = write(f->fd, data, size);
    if (z < 0)
    {
        switch (errno)
        {
        case EBADF:
            return -ZLXF_BAD_FILE_DESC;
        case EAGAIN:
#if EWOULDBLOCK != EAGAIN
        case EWOULDBLOCK:
#endif
            return -ZLXF_WOULD_BLOCK;
        case EFAULT:
            return -ZLXF_BAD_BUFFER;
        case EINTR:
            return -ZLXF_INTERRUPTED;
        case EINVAL:
        case EPIPE:
            return -ZLXF_BAD_OPERATION;
        case EIO:
            return -ZLXF_IO_ERROR;
        case ENOSPC:
            return -ZLXF_NO_SPACE;
        case EDQUOT:
            return -ZLXF_QUOTA_EXHAUSTED;
        case EFBIG:
            return -ZLXF_SIZE_LIMIT;
        default:
            return -ZLXF_FAILED;
        }
    }
    return (ptrdiff_t) z;
}

/* file_seek64 **************************************************************/
static int64_t ZLX_CALL file_seek64
(
    zlx_file_t * zf,
    int64_t offset,
    int anchor
)
{
    file_t * restrict f = (file_t *) zf;
    int64_t o;

    o = lseek64(f->fd, offset, anchor);
    if (o < 0)
    {
        switch (errno)
        {
        case EBADF:
            return -ZLXF_BAD_FILE_DESC;
        case EINVAL:
        case ESPIPE:
            return -ZLXF_BAD_OPERATION;
        case EOVERFLOW:
            return -ZLXF_OVERFLOW;
        default:
            return -ZLXF_FAILED;
        }
    }
    return o;
}


/* file_truncate ************************************************************/
static zlx_file_status_t ZLX_CALL file_truncate
(
    zlx_file_t * zf
)
{
    file_t * restrict f = (file_t *) zf;
    int64_t o;

    o = file_seek64(zf, 0, ZLXF_CUR);
    if (o < 0) return (zlx_file_status_t) -o;

    if (!ftruncate(f->fd, o)) return ZLXF_OK;
    switch (errno)
    {
    case EFBIG:
        return ZLXF_SIZE_LIMIT;
    case EINTR:
        return ZLXF_INTERRUPTED;
    case EINVAL:
        return ZLXF_BAD_OPERATION;
    case EIO:
        return ZLXF_IO_ERROR;
    case EBADF:
        return ZLXF_BAD_FILE_DESC;
    default:
        return ZLXF_FAILED;
    }
}

/* file_close ***************************************************************/
static zlx_file_status_t ZLX_CALL file_close
(
    zlx_file_t * zf,
    unsigned int flags
)
{
    file_t * restrict f = (file_t *) zf;
    flags = f->base.flags & ~flags;
    if ((flags & (ZLXF_READ | ZLXF_WRITE)) || !close(f->fd))
    {
        f->base.flags = flags;
        return ZLXF_OK;
    }
    switch (errno)
    {
    case EBADF:
        return ZLXF_BAD_FILE_DESC;
    case EIO:
        return ZLXF_IO_ERROR;
    case EINTR:
        return ZLXF_INTERRUPTED;
    default:
        return ZLXF_FAILED;
    }
}

/* hbs_posix_main ***********************************************************/
HBS_API int hbs_posix_main (int argc, char const * const * argv, 
                            hbs_main_func_t main_func)
{
    unsigned int r;

    r = main_wrap(argc, (uint8_t const * const *) argv, main_func);
    return r;
}

#endif

