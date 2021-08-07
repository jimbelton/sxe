shell:
ifeq ($(OS_class),any-winnt)
	@cmd.exe
else
	@$(REMOTE_SHELL)
endif

