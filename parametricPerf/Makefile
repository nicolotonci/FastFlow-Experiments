# ---------------------------------------------------------------------------
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as 
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#  As a special exception, you may use this file as part of a free software
#  library without restriction.  Specifically, if other files instantiate
#  templates or use macros or inline functions from this file, or you compile
#  this file and link it with other files to produce an executable, this
#  file does not by itself cause the resulting executable to be covered by
#  the GNU General Public License.  This exception does not however
#  invalidate any other reasons why the executable file might be covered by
#  the GNU General Public License.
#
# ---------------------------------------------------------------------------

CXXFLAGS            += -std=c++17
ifdef DEBUG
    OPTIMIZE_FLAGS  += -g -fno-inline-functions -DDEBUG
else
    OPTIMIZE_FLAGS  += -O3 -finline-functions -DNDEBUG
endif
ifdef DISABLE_FF_DISTRIBUTED
    CXXFLAGS += -DDISABLE_FF_DISTRIBUTED
endif
ifdef EXCLUDE_BLOCKING
    CXXFLAGS        += -DDFF_EXCLUDE_BLOCKING
endif
ifdef PRINT_IR
    CXXFLAGS += -DDFF_PRINT_IR
endif
ifdef EXCLUDE_MPI
    CXXFLAGS	    += -DDFF_EXCLUDE_MPI
else
ifdef MPI_HOME
    CXX = mpicxx
else
    CXXFLAGS	    += -DDFF_EXCLUDE_MPI
endif
endif
ifdef FF_HOME
    INCS            += -I$(FF_HOME)
else
    INCS            += -I ~/fastflow
endif
ifdef CEREAL_HOME
    INCS            += -I$(CEREAL_HOME)/include
else
    INCS            += -I ~/cereal
endif

OS                   = $(shell uname)

ifeq ($(strip $(OS)),Darwin)
	CXX += -DNO_DEFAULT_MAPPING
endif


CXXFLAGS            += -Wall
LIBS                += -pthread
INCLUDES             = $(INCS)

SOURCES              = $(wildcard *.cpp)
TARGET               = $(SOURCES:.cpp=)

.PHONY: all clean cleanall 
.SUFFIXES: .c .cpp .o

%.d: %.cpp
	set -e; $(CXX) -MM $(INCLUDES) $(CXXFLAGS) $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
		[ -s $@ ] || rm -f $@
%.d: %.c
	set -e; $(CC) -MM $(INCLUDES) $(CFLAGS)  $< \
		| sed 's/\($*\)\.o[ :]*/\1.o $@ : /g' > $@; \
		[ -s $@ ] || rm -f $@
%.o: %.c
	$(CC) $(INCLUDES) $(CFLAGS) -c -o $@ $<
%: %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) $(OPTIMIZE_FLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

all: $(TARGET)

clean: 
	-rm -fr *.o *~
cleanall: clean
	-rm -fr $(TARGET) *.d ./socket*

include $(OBJS:.o=.d)
