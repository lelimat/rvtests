.PHONY: release debug
all: release

ROOT?=..
include $(ROOT)/Makefile.common
include $(ROOT)/Makefile.lib

BASE = $(wildcard *.cpp)
OBJ = $(BASE:%.cpp=%.o)
OBJ_DBG = $(BASE:%.cpp=%_dbg.o)

release: CXX_FLAGS = -O2 -DNDEBUG -I$(ROOT) -I$(ZLIB_INC)
release: lib-goncalo.a
lib-goncalo.a: $(OBJ)
	-[ -f $@ ] && rm -f $@ || echo -n
	$(AR) -cqs lib-goncalo.a $^
%.o : %.cpp $(EIGEN_INC)
	$(CXX) $(CXX_FLAGS) -c $< -o $@

debug: lib-dbg-goncalo.a
debug: CXX_FLAGS = -g -O0 -I$(ROOT) -I$(ZLIB_INC)
lib-dbg-goncalo.a: $(OBJ_DBG)
	-[ -f $@ ] && rm -f $@ || echo -n
	$(AR) -cqs lib-dbg-goncalo.a $^
%_dbg.o: %.cpp
	$(CXX) $(CXX_FLAGS) -c $< -o $@

profile: lib-dbg-goncalo.a
profile: CXX_FLAGS = -pg -O0 -I$(ROOT) -I$(ZLIB_INC)

clean:
	-rm -rf *.o *.a
