app = champsim

srcExt = cc
srcDir = src branch replacement prefetcher
objDir = obj
binDir = bin
inc = inc

debug = 1

CFlags = -Wall -g -std=c++11 -O3
LDFlags = -static
ifeq ($(shell uname -s),Darwin)
LDFlags =
endif
libs =
libDir =


#************************ DO NOT EDIT BELOW THIS LINE! ************************

ifeq ($(debug),1)
	debug=-g
else
	debug=
endif
inc := $(addprefix -I,$(inc))
libs := $(addprefix -l,$(libs))
libDir := $(addprefix -L,$(libDir))
CFlags += -c $(debug) $(inc) $(libDir) $(libs)
# Policy templates are copied into replacement/*_replacement.cc by the build script.
sources := $(filter-out replacement/%,$(shell find $(srcDir) -name '*.$(srcExt)')) $(wildcard replacement/*_replacement.cc)
objects := $(patsubst %.$(srcExt),$(objDir)/%.o,$(sources))

ifeq ($(srcExt),cc)
	CC = $(CXX)
else
	CFlags += -std=gnu99
endif

.PHONY: all clean distclean run


all: $(binDir)/$(app)

run:
	@./build_champsim.sh

$(binDir)/$(app): $(objects) Makefile build_champsim.sh
	@mkdir -p $(dir $@)
	@echo "Linking $@..."
	@$(CC) $(objects) $(LDFlags) -o $@

$(objDir)/%.o: %.$(srcExt) Makefile
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CC) $(CFlags) -MMD -MP -MF $(@:.o=.d) -MT $@ $< -o $@

clean:
	$(RM) -r $(objDir)

distclean: clean
	$(RM) -r $(binDir)/$(app)

# Rebuild users of changed headers. Missing dependency files are normal on the first build.
-include $(objects:.o=.d)
