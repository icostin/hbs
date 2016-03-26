projects := hbs

hbs_prod := slib dlib

hbs_csrc := common.c mswin.c posix.c
hbs_chdr := hbs.h

# xxx_cflags (1: prj, 2: prod, 3: cfg, 4: bld, 5: src)
hbs_cflags = -DHBS_TARGET='"$($4_target)"' -DHBS_CONFIG='"$3"' -DHBS_COMPILER='"$($4_compiler)"'
hbs_slib_cflags := -DHBS_STATIC -DZLX_STATIC
hbs_dlib_cflags := -DHBS_DYNAMIC
hbs_ldflags = -lzlx$($3_sfx)
hbs_ldep = $(call prod_path,zlx,$2,$3,$4)

include icobld.mk

