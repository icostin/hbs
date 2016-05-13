#ifndef _HBS_INTERN_H
#define _HBS_INTERN_H

typedef struct hbs_thread_start_s hbs_thread_start_t;
struct hbs_thread_start_s
{
    zlx_thread_func_t func;
    void * arg;
};

extern uint8_t error_buffer[];

uint8_t ZLX_CALL main_wrap
(
    unsigned int argc,
    uint8_t const * const * argv,
    hbs_main_func_t main_func
);

#endif /* _HBS_INTERN_H */

