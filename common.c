#include "hbs.h"
#include "intern.h"

HBS_API char const * const hbs_lib_name = "hbs"
#if HBS_STATIC
    "-static"
#else
    "-dynamic"
#endif
    "-" HBS_CONFIG
    "-" HBS_TARGET
    "-" HBS_COMPILER
    ;

/* hbs_default_log **********************************************************/
zlx_log_t hbs_default_log =
{
    zlx_nop_write,
    NULL,
    ZLX_LL_NONE
};

HBS_API zlx_log_t * hbs_log = &hbs_default_log;


uint8_t error_buffer[0x1000];

/* main_wrap ****************************************************************/
uint8_t ZLX_CALL main_wrap
(
    unsigned int argc,
    uint8_t const * const * argv,
    hbs_main_func_t main_func
)
{
    zlx_sbw_t ebw;
    hbs_status_t hs_init;
    uint8_t rv = 127;
    zlx_ma_t * ma_trk = NULL;
    uint8_t opt_track_allocs =
#if _CHECKED || _DEBUG
        1
#else
        0
#endif
        ;

    do
    {
        zlx_sbw_init(&ebw, error_buffer, sizeof(error_buffer) - 1);
        hs_init = hbs_init();
        if (hs_init)
        {
            zlx_fmt(zlx_sbw_write, &ebw, zlx_utf8_term_width, NULL,
                    "error: failed to init basic services (code $u)\n",
                    hs_init);
            break;
        }

        if (opt_track_allocs)
        {
            ma_trk = zlx_alloctrk_create(hbs_ma, hbs_log);
            if (!ma_trk)
            {
                zlx_fmt(zlx_sbw_write, &ebw, zlx_utf8_term_width, NULL,
                        "error: failed to create mem alloc tracker\n");
                break;
            }
            hbs_ma = ma_trk;
        }

        rv = main_func(argc, argv);
        if (rv > 125) rv = 126;
    }
    while (0);

    if (ma_trk)
    {
        zlx_alloctrk_dump(ma_trk);
        zlx_alloctrk_destroy(ma_trk);
    }

    if (!hs_init) hbs_finish();

    if (rv == 127)
    {
        if (ebw.size > ebw.limit) ebw.size = ebw.limit;
        error_buffer[ebw.size] = 0;
    }

    return rv;
}


/* hbs_file_close ***********************************************************/
HBS_API zlx_file_status_t ZLX_CALL hbs_file_close
(
    zlx_file_t * f
)
{
    zlx_file_status_t zfs;
    zfs = zlx_close(f);
    hbs_file_free(f);
    return zfs;
}

/* hbs_log_init *************************************************************/
HBS_API void hbs_log_init (zlx_file_t * restrict file, unsigned int level)
{
    hbs_default_log.write = (zlx_write_func_t) file->fcls->write;
    hbs_default_log.obj = file;
    hbs_default_log.level = level;
    zlx_default_log = &hbs_default_log;
}

