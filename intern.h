#ifndef _HBS_INTERN_H
#define _HBS_INTERN_H

extern uint8_t error_buffer[];

uint8_t ZLX_CALL main_wrap
(
    unsigned int argc,
    uint8_t const * const * argv,
    hbs_main_func_t main_func
);

#endif /* _HBS_INTERN_H */

