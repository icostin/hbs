.PHONY: all clean cleanall install uninstall nop

builders :=
bldenv_mk := $(or $(bldenv_mk),$(wildcard $(HOME)/.config/icobldenv.mk),$(wildcard ../bldenv.mk))

ifneq ($(bldenv_mk),)
$(info using build env from file: $(bldenv_mk)...)
include $(bldenv_mk)
else
$(info using built-in build env...)
endif

make_deps := GNUmakefile icobld.mk $(bldenv_mk)

products := slib dlib exe
configs := release checked debug

$(info generating make rules for projects: $(projects)...)

# set defaults for options not explicitly defined
ifeq ($(builders),)
c := host
builders := $c
$c_cc := $(CC)
$c_ar := $(AR)
$c_strip := strip
$c_dlib_cflags := -fPIC
$c_dlib_ldflags := -shared
endif

ifeq ($(PREFIX_DIR),)
PREFIX_DIR := $(HOME)/.local
endif

ifeq ($(OUT_DIR),)
OUT_DIR := ../_out
endif

ifeq ($(BLD_DIR),)
BLD_DIR := ../_tmp
endif

slib_out_dir ?= lib
slib_pfx ?= lib
slib_sfx ?= .a
slib_mod := 644

dlib_out_dir ?= lib
dlib_pfx ?= lib
dlib_sfx ?= .so
dlib_mod := 755

exe_out_dir ?= bin
exe_pfx ?=
exe_sfx ?=
exe_mod := 755

release_sfx ?=
checked_sfx ?= -checked
debug_sfx ?= -debug

release_cflags ?= -DNDEBUG
checked_cflags ?= -D_CHECKED
debug_cflags ?= -D_DEBUG

release_run_strip ?= yes
checked_run_strip ?= yes
debug_run_strip ?= no

# normalize_target(tgt_string)
normalize_target = $(firstword \
	$(subst i386-,ia32_386-,\
	$(subst i486-,ia32_486-,\
	$(subst i586-,ia32_586-,\
	$(subst i686-,ia32_686-,\
	$(subst x86_64-,amd64-,\
	$(subst -w64-mingw32,-windows,\
	$(subst -pc-,-,\
	$(subst -unknown-,-,\
	$1)))))))))

# gen_builder_vars (1: bld)
define gen_builder_vars

$1_target ?= $$(call normalize_target,$$(shell $($1_cc) -dumpmachine))
$1_compiler ?=$(or $(findstring clang,$($1_cc)),$(findstring gcc,$($1_cc),unknown_compiler))_$(subst .,_,$(shell $($1_cc) -dumpversion))

endef

$(eval $(foreach b,$(builders),$(call gen_builder_vars,$b)))

all: $(projects)

clean:
	-rm -f $(patsubst %,$(BLD_DIR)/%_*,$(projects))
	-rm -f $(foreach p,$(projects),$(foreach q,$($p_prod),$(foreach c,$($p_cfg),$(foreach b,$($p_bld),$(call prod_path,$p,$q,$c,$b)))))
	-rm -f $(foreach p,$(projects),$(foreach b,$($p_bld),$(patsubst %,$(OUT_DIR)/$b/include/%,$($p_chdr))))

cleanall:
	rm -rf $(BLD_DIR)
	rm -rf $(OUT_DIR)

# gen_prj_vars (1: prj)
define gen_prj_vars

$1_cfg ?= release checked debug
$1_prod ?= exe
$1_bld := $(builders)
$1_chdr_inc_dirs := $(sort $(dir $(patsubst %,include/%,$($1_chdr))))

endef

#$(info $(foreach p,$(projects),$(call gen_prj_vars,$p)))
$(eval $(foreach p,$(projects),$(call gen_prj_vars,$p)))

# gen_prj_prod_vars (1: prj, 2: prod)
define gen_prj_prod_vars

$1_$2_prj := $1
$1_$2_prod := $2

endef

$(eval $(foreach p,$(projects),$(foreach q,$($p_prod),$(call gen_prj_prod_vars,$p,$q))))

# gen_bld_prod_cfg_vars (1: bld, 2: prod, 3: cfg)
define gen_bld_prod_cfg_vars

$1_$2_$3_cflags := $($1_cflags) $($2_cflags) $($3_cflags) $($1_$2_cflags) $($1_$3_cflags) $($2_$3_cflags) $($1_$2_$3_cflags)

endef

# gen_bld_prod_vars(1:bld, 2: prod)
define gen_bld_prod_vars

$1_$2_out_dir ?= $($2_out_dir)

$(foreach c,$(configs),$(call gen_bld_prod_cfg_vars,$1,$2,$c))

endef

#$(info $(foreach b,$(builders),$(foreach q,$(products),$(call gen_bld_prod_vars,$b,$q))))
$(eval $(foreach b,$(builders),$(foreach q,$(products),$(call gen_bld_prod_vars,$b,$q))))

cprod = $(BLD_DIR)/$1__$(subst /,_,$(patsubst %.c,%.$3,$2))

# bld_cflags (1: prj, 2: prod, 3: cfg, 4: bld, 5: csrc)
bld_cflags = -I$(OUT_DIR)/$4/include $($4_$2_$3_cflags) $($1_$2_cflags) $(call $1_cflags,$1,$2,$3,$4,$5)

# gen_cobj_rule (1: prj, 2: prod, 3: cfg, 4: bld, 5: csrc)
define gen_cobj_rule


$(call cprod,$1_$2_$3_$4,$5,o): $5 $(call cprod,$1_$2_$3_$4,$5,d) $(make_deps) $(patsubst %,$(OUT_DIR)/$4/include/%,$(foreach d,$($1_idep),$($($d_prj)_chdr))) | $(BLD_DIR)
	$($4_cc) -MT $$@ -MMD -MF $(call cprod,$1_$2_$3_$4,$5,tmpd) -c $(call bld_cflags,$1,$2,$3,$4) $5 -o$$@
	mv -f $(call cprod,$1_$2_$3_$4,$5,tmpd) $(call cprod,$1_$2_$3_$4,$5,d)

$(call cprod,$1_$2_$3_$4,$5,d): ;

-include $(call cprod,$1_$2_$3_$4,$5,d)

endef


# prod_path (1: prj, 2: prod, 3: cfg, 4: bld)
prod_path = $(subst //,/,$(OUT_DIR)/$4/$($4_$2_out_dir)/$(or $($4_$2_pfx),$($2_pfx))$(or $($1_name),$1)$($3_sfx)$($4_sfx)$(or $($4_$2_sfx),$($2_sfx)))

inst_path = $(subst //,/,$(PREFIX_DIR)/$($4_$2_out_dir)/$(or $($4_$2_pfx),$($2_pfx))$(or $($1_name),$1)$(or $($4_$3_isfx),$($4_isfx))$(or $($4_$2_sfx),$($2_sfx)))

# gen_slib_rule (1: prj, 2: cfg, 3: bld)
define gen_slib_rule

.PHONY: $1_slib_$2_$3
$1_slib_$2_$3: $(call prod_path,$1,slib,$2,$3)
$(call prod_path,$1,slib,$2,$3): $(foreach s,$($1_csrc),$(call cprod,$1_slib_$2_$3,$s,o)) $(patsubst %,$(OUT_DIR)/$3/include/%,$($1_chdr)) $(foreach d,$($1_idep),$(call prod_path,$($d_prj),$($d_prod),$2,$3)) | $(OUT_DIR)/$3/$($3_slib_out_dir)
	$($3_ar) rcs $$@ $(foreach s,$($1_csrc),$(call cprod,$1_slib_$2_$3,$s,o))

$(foreach s,$($1_csrc),$(call gen_cobj_rule,$1,slib,$2,$3,$s))

endef

# bld_ldflags (1: prj, 2: prod, 3: cfg, 4: bld)
bld_ldflags = -L$(OUT_DIR)/$4/lib $($4_ldflags) $($2_ldflags) $($3_ldflags) $(call $4_$2_ldflags,$1,$2,$3,$4) $($4_$3_ldflags) $($2_$3_ldflags) $($4_$2_$3_ldflags) $($1_$2_ldflags) $(call $1_ldflags,$1,$2,$3,$4)

# gen_dlib_rule (1: prj, 2: cfg, 3: bld)
define gen_dlib_rule

.PHONY: $1_dlib_$2_$3
$1_dlib_$2_$3: $(call prod_path,$1,dlib,$2,$3)
$(call prod_path,$1,dlib,$2,$3): $(foreach s,$($1_csrc),$(call cprod,$1_dlib_$2_$3,$s,o))  $(patsubst %,$(OUT_DIR)/$3/include/%,$($1_chdr)) $(foreach d,$($1_idep),$(call prod_path,$($d_prj),$($d_prod),$2,$3)) $(call $1_ldep,$1,dlib,$2,$3) | $(OUT_DIR)/$3/$($3_dlib_out_dir)
	$($3_cc) -o $$@ $(foreach s,$($1_csrc),$(call cprod,$1_dlib_$2_$3,$s,o)) $(call bld_ldflags,$1,dlib,$2,$3)
	$(and $(filter yes,$($2_run_strip)),$($3_strip) $$@)

$(foreach s,$($1_csrc),$(call gen_cobj_rule,$1,dlib,$2,$3,$s))

endef

# gen_exe_rule (1: prj, 2: cfg, 3: bld)
define gen_exe_rule

.PHONY: $1_exe_$2_$3
$1_exe_$2_$3: $(call prod_path,$1,exe,$2,$3)
$(call prod_path,$1,exe,$2,$3): $(foreach s,$($1_csrc),$(call cprod,$1_exe_$2_$3,$s,o)) $(foreach d,$($1_idep),$(call prod_path,$($d_prj),$($d_prod),$2,$3)) $(call $1_ldep,$1,exe,$2,$3) | $(OUT_DIR)/$3/$($3_exe_out_dir)
	$($3_cc) -o $$@ $(foreach s,$($1_csrc),$(call cprod,$1_exe_$2_$3,$s,o)) $(call bld_ldflags,$1,exe,$2,$3)
	$(and $(filter yes,$($2_run_strip)),$($3_strip) $$@)

$(foreach s,$($1_csrc),$(call gen_cobj_rule,$1,exe,$2,$3,$s))

endef


# 1: prj, 2: prod, 3: cfg
define gen_prj_prod_cfg_rule

.PHONY: $1_$2_$3

$1_$2_$3: $(foreach b,$($1_bld),$1_$2_$3_$b)

$(foreach b,$($1_bld),$(call gen_$2_rule,$1,$3,$b))

endef

# 1: prj, 2: prod
define gen_prj_prod_rule

.PHONY: $1_$2
$1_$2: $(foreach c,$($1_cfg),$1_$2_$c)

$(foreach c,$($1_cfg),$(call gen_prj_prod_cfg_rule,$1,$2,$c))

endef

# gen_prj_bld_chdr_rule (1: prj, 2: bld, 3: chdr)
define gen_prj_bld_chdr_rule


$(OUT_DIR)/$2/include/$3: $3 | $(dir $(OUT_DIR)/$2/include/$3)
	cp -f $$< $$@


endef

define gen_inst_chdr_file_rule

$(PREFIX_DIR)/include/$1: $1
	install -D -m 644 -T $$< $$@

endef

# 1: prj, 2: prod, 3: cfg, 4: bld
define gen_inst_prod_rule

$(call inst_path,$1,$2,$3,$4): $(call prod_path,$1,$2,$3,$4)
	install -D -m $($2_mod) -T $$< $$@

endef

# gen_prj_bld_rule (1: prj, 2: bld)
define gen_prj_bld_rule

.PHONY: $1_$2
$1_$2: $(foreach q,$($1_prod),$(foreach c,$($1_cfg),$1_$q_$c_$2))

endef

# 1: prj
define gen_prj_rule

.PHONY: $1 install_chdr_$1
$1: $(foreach q,$($1_prod),$1_$q)

$(foreach q,$($1_prod),$(call gen_prj_prod_rule,$1,$q))

$(foreach b,$($1_bld),$(foreach h,$($1_chdr),$(call gen_prj_bld_chdr_rule,$1,$b,$h)))

install_chdr_$1: $(patsubst %,$(PREFIX_DIR)/include/%,$($1_chdr))


$(foreach h,$($1_chdr),$(call gen_inst_chdr_file_rule,$h))

$(foreach b,$($1_bld),$(call gen_prj_bld_rule,$1,$b))

endef

# gen_bld_rule (1: bld)
define gen_bld_rule

.PHONY: $1
$1: $(foreach p,$(projects),$(and $(filter $1,$($p_bld)),$p_$1))

endef

#$(info $(foreach p,$(projects),$(call gen_prj_rule,$p)))
$(eval $(foreach p,$(projects),$(call gen_prj_rule,$p)))

#$(info DIRS: $(sort $(BLD_DIR) $(foreach b,$(builders),$(foreach q,$(products),$(OUT_DIR)/$b/$($b_$q_out_dir)) $(OUT_DIR)/$b/include) $(foreach p,$(projects),$(foreach b,$($p_bld), $(patsubst %,$(OUT_DIR)/$b/%,$($p_chdr_inc_dirs))))):)

$(sort $(BLD_DIR) $(foreach b,$(builders),$(foreach q,$(products),$(OUT_DIR)/$b/$($b_$q_out_dir)) $(OUT_DIR)/$b/include) $(foreach p,$(projects),$(foreach b,$($p_bld), $(patsubst %,$(OUT_DIR)/$b/%,$($p_chdr_inc_dirs))))):
	mkdir -p $@

#$(info install: $(sort $(foreach p,$(projects),$(foreach q,$($p_prod),$(foreach c,$($p_cfg),$(foreach b,$($p_bld),$(and $($b_$c_install),$(call inst_path,$p,$q,$c,$b) install_chdr_$p)))))))
install: $(sort $(foreach p,$(projects),$(foreach q,$($p_prod),$(foreach c,$($p_cfg),$(foreach b,$($p_bld),$(and $($b_$c_install),$(call inst_path,$p,$q,$c,$b) install_chdr_$p))))))

uninstall:
	-rm -f $(foreach p,$(projects),$(patsubst %,$(PREFIX_DIR)/include/%,$($p_chdr)) $(foreach q,$($p_prod),$(foreach c,$($p_cfg),$(foreach b,$($p_bld),$(and $($b_$c_install),$(call inst_path,$p,$q,$c,$b))))))
	

#$(info $(foreach p,$(projects),$(foreach q,$($p_prod),$(foreach c,$($p_cfg),$(foreach b,$($p_bld),$(and $($b_$c_install),$(call gen_inst_prod_rule,$p,$q,$c,$b)))))))
$(eval $(foreach p,$(projects),$(foreach q,$($p_prod),$(foreach c,$($p_cfg),$(foreach b,$($p_bld),$(and $($b_$c_install),$(call gen_inst_prod_rule,$p,$q,$c,$b)))))))

$(eval $(foreach b,$(builders),$(call gen_bld_rule,$b)))

nop:

$(info processing rules...)

