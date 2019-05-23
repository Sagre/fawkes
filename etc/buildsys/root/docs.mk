#*****************************************************************************
#     Makefile Build System for Fawkes: Documentation config and targets
#                            -------------------
#   Created on Wed Mar 31 14:31:57 2010
#   Copyright (C) 2006-2010 by Tim Niemueller, AllemaniACs RoboCup Team
#
#*****************************************************************************
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#*****************************************************************************

ifndef __buildsys_config_mk_
$(error config.mk must be included before docs.mk)
endif

ifndef __buildsys_root_docs_mk_
__buildsys_root_docs_mk_ := 1

.PHONY: apidoc quickdoc tracdoc
apidoc: api.doxygen fawkes.luadoc skiller.luadoc
quickdoc: api-quick.doxygen
tracdoc: api-trac.doxygen

%.doxygen:
	$(SILENT) echo -e "[DOC] Building documentation ($(TBOLDGRAY)$@$(TNORMAL)). This may take a while..."
	$(SILENT) rm -rf doc/api
	$(SILENT) mkdir -p doc/api
	$(SILENT) $(DOXYGEN) $(DOCDIR)/doxygen/$*$(if $(SUBMODULE_EXTERN),-submodule).doxygen >/dev/null 2>&1
	$(SILENT) git ls-files --full-name *.{h,cpp} | awk "{ print \"$$(git rev-parse --show-toplevel)/\"\$$1 }" > git-files.txt
	$(SILENT) grep -E -e "^[^[:space:]:;,<>?\"*|\\]*:[0-9]+:" warnings.txt -o | grep -E -e "^[^[:space:]:;,<>?\"*|\\]*" warnings.txt -o > warning-files.txt || exit 0
	$(SILENT) if [ "`grep -Fx -f git-files.txt warning-files.txt | wc -l`" != "0" ]; then \
		echo -e "$(TRED)--> Warnings have been generated:$(TNORMAL)"; \
		cat warnings.txt; \
		exit 1; \
	else \
		echo -e "$(TGREEN)--> No warnings. Nice job.$(TNORMAL)"; \
	fi

%.luadoc:
	$(SILENT) echo "[DOC] Generating luadoc for the skiller..."
	$(SILENT) rm -rf $(DOCDIR)/lua/$*
	$(SILENT) mkdir -p $(DOCDIR)/lua/$*
	$(SILENT) cd src/lua; $(LUADOC) -d $(DOCDIR)/lua/$* $*

endif # __buildsys_root_docs_mk_

