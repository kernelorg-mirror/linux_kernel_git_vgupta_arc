ifeq ($(CONFIG_OF),y)

dtb-y	+= skeleton.dtb

# Built-in dtb
builtindtb-y				:= skeleton

ifneq ($(CONFIG_ARC_BUILTIN_DTB_NAME),"")
	builtindtb-y			:= $(CONFIG_ARC_BUILTIN_DTB_NAME)
endif

obj-y	+= $(patsubst "%",%,$(builtindtb-y)).dtb.o

clean-files += $(obj)/$(dtb-y) $(obj)/*.dtb.S $(obj)/*.dtb.o

# Rule to build device tree blobs
$(obj)/%.dtb: $(src)/../dts/%.dts FORCE
	$(call if_changed_dep,dtc)

$(obj)/dtbs: $(addprefix $(obj)/, $(dtb-y))

endif
