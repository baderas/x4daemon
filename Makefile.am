AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip
bin_PROGRAMS	= x4daemon
x4daemon_SOURCES	= src/x4daemon.c
EXTRA_DIST =  src/x4daemon.init src/x4daemon.service LICENSE
docdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)
doc_DATA = README ChangeLog AUTHORS NEWS LICENSE
README: README.md