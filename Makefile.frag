# provide headers in builddir, so they do not end up in /usr/include/ext/raphf/src

PHP_RAPHF_HEADERS := $(addprefix $(PHP_RAPHF_BUILDDIR)/,$(PHP_RAPHF_HEADERS))

$(PHP_RAPHF_BUILDDIR)/%.h: $(PHP_RAPHF_SRCDIR)/src/%.h
	@cat >$@ <$<

all: raphf-build-headers
clean: raphf-clean-headers

.PHONY: raphf-build-headers
raphf-build-headers: $(PHP_RAPHF_HEADERS)

.PHONY: raphf-clean-headers
raphf-clean-headers:
	-rm -f $(PHP_RAPHF_HEADERS)
