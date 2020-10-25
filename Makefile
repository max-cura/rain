CC=gcc
CPP=g++

CFLAGS = -Wall -Wextra -g3 -O3 -ffreestanding -fPIC -fcolor-diagnostics -pipe \
    -fdollars-in-identifiers -finline-functions -fno-ms-compatibility -fno-rtti -I. \
    -Wno-unused-parameter \
    -Wno-unused-command-line-argument -Wno-nullability-completeness -Wno-expansion-to-defined -Wall -Wextra

ldflags = -g3 -O3 -ffreestanding -fPIC -fcolor-diagnostics -pipe \
    -flto=thin -flto-jobs=6 -fsplit-lto-unit -fwhole-program-vtables \
    -fdollars-in-identifiers -finline-functions -fno-ms-compatibility -fno-rtti \
    -nobuiltininc -Wno-unused-command-line-argument -lstdc++

OBJECTS=build/main.o build/nova.o build/nova-os.o build/nova-tid.o build/nova-fast.o build/sync.o

build/%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -MMD -MP
build/%.cc.o: %.cc
	$(CPP) $(CFLAGS) -c $< -o $@ -MMD -MP

build/nova: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@
