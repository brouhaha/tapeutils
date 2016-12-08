# Makefile for tapeutils
# Copyright 1998, 1999, 2000 Eric Smith
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.  Note that permission is not granted
# to redistribute this program under the terms of any later version of the
# General Public License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# -----------------------------------------------------------------------------
# options
# -----------------------------------------------------------------------------

# CFLAGS = -O2 -Wall
# LDFLAGS = 

CFLAGS = -g -Wall
LDFLAGS = -g

# -----------------------------------------------------------------------------
# You shouldn't have to change anything below this point, but if you do please
# let me know why so I can improve this Makefile.
# -----------------------------------------------------------------------------

PACKAGE = tapeutils
VERSION = 0.6
DSTNAME = $(PACKAGE)-$(VERSION)

PROGRAMS = tapecopy tapedump taperead tapewrite t10backup read20

HEADERS = tapeio.h t10backup.h dumper.h
SOURCES = tapeio.c tapecopy.c tapedump.c taperead.c tapewrite.c t10backup.c read20.c
MISC = COPYING

DISTFILES = $(MISC) Makefile $(HEADERS) $(SOURCES)

all: $(PROGRAMS) $(MISC_TARGETS)

dist: $(DISTFILES)
	-rm -rf $(DSTNAME)
	mkdir $(DSTNAME)
	for f in $(DISTFILES); do ln $$f $(DSTNAME)/$$f; done
	tar --gzip -chf $(DSTNAME).tar.gz $(DSTNAME)
	-rm -rf $(DSTNAME)

clean:
	rm -f $(PROGRAMS) $(MISC_TARGETS) *.o


tapecopy: tapecopy.o tapeio.o

tapedump: tapedump.o tapeio.o

taperead: taperead.o tapeio.o

tapewrite: tapewrite.o tapeio.o

t10backup: t10backup.o tapeio.o

read20: read20.o tapeio.o


include $(SOURCES:.c=.d)

%.d: %.c
	$(CC) -M -MG $(CFLAGS) $< | sed -e 's@ /[^ ]*@@g' -e 's@^\(.*\)\.o:@\1.d \1.o:@' > $@
