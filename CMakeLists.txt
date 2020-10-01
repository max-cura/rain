cmake_minimum_required(VERSION 3.18)

project(nova
        LANGUAGES C CXX)

add_executable(nova nova.c nova.h nova-os.c nova-tid.c nova-fast.c)
target_compile_options(nova
        PUBLIC
        -Wall -Wextra -g3 -O3 -ffreestanding -fPIC -fcolor-diagnostics -pipe
        -fdollars-in-identifiers -finline-functions -fno-ms-compatibility -fno-rtti -I.
        -Wno-unused-parameter
        -Wno-unused-command-line-argument -Wno-nullability-completeness -Wno-expansion-to-defined -Wall -Wextra)
target_link_options(nova
        PUBLIC
        -g3 -O3 -ffreestanding -fPIC -fcolor-diagnostics -pipe
        -flto=thin -flto-jobs=6 -fsplit-lto-unit -fwhole-program-vtables -Wl,-cache_path_lto,${CMAKE_CURRENT_BINARY_DIR}/__ltocache
        -fdollars-in-identifiers -finline-functions -fno-ms-compatibility -fno-rtti
        -fvisibility=hidden -Wno-unused-command-line-argument)
set_target_properties(
    nova
    PROPERTIES
        RULE_LAUNCH_COMPILE ccache
        RULE_LAUNCH_LINK ccache
)