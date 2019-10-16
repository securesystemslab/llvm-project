# Build for MPK Untrusted Dynamic Analysis

include_directories(..)

set(MPK_UNTRUSTED_SOURCES
    alloc_site_handler.cpp
    mpk.cpp
    mpk_fault_handler.cpp
    mpk_formatter.cpp
    mpk_untrusted.cpp
    )

set(MPK_UNTRUSTED_HEADERS
    alloc_site_handler.h
    mpk.h
    mpk_common.h
    mpk_fault_handler.h
    mpk_formatter.h
    mpk_untrusted.h
    )

include_directories(${LLVM_INCLUDE_DIRS})
set(MPK_UNTRUSTED_CFLAGS ${SANITIZER_COMMON_CFLAGS})
set(MPK_UNTRUSTED_LINK_FLAGS ${SANITIZER_COMMON_LINK_FLAGS})
set(MPK_UNTRUSTED_DYNAMIC_LIBS ${SANITIZER_CXX_ABI_LIBRARY} ${SANITIZER_COMMON_LINK_LIBS} dl pthread rt m tinfo)

add_compiler_rt_component(mpk_untrusted)
foreach(arch ${MPK_UNTRUSTED_SUPPORTED_ARCH})
    add_compiler_rt_object_libraries(RTMPKUntrusted
    ARCHS ${arch}
    SOURCES ${MPK_UNTRUSTED_SOURCES}
            $<TARGET_OBJECTS:RTSanitizerCommon.${arch}>
            $<TARGET_OBJECTS:RTSanitizerCommonLibc.${arch}>
    ADDITIONAL_HEADERS ${MPK_UNTRUSTED_HEADERS}
    CFLAGS ${MPK_UNTRUSTED_CFLAGS})

    add_compiler_rt_runtime(clang_rt.mpk_untrusted
        STATIC
        ARCHS ${arch}
        SOURCES ${MPK_UNTRUSTED_SOURCES}
                $<TARGET_OBJECTS:RTSanitizerCommon.${arch}>
                $<TARGET_OBJECTS:RTSanitizerCommonLibc.${arch}>
        ADDITIONAL_HEADERS ${MPK_UNTRUSTED_HEADERS}
        CFLAGS ${MPK_UNTRUSTED_CFLAGS}
        PARENT_TARGET mpk_untrusted)

    add_compiler_rt_runtime(clang_rt.mpk_untrusted
        SHARED
        ARCHS ${arch}
        SOURCES ${MPK_UNTRUSTED_SOURCES}
        ADDITIONAL_HEADERS ${MPK_UNTRUSTED_HEADERS}
        OBJECT_LIBS RTSanitizerCommon
                    RTSanitizerCommonLibc
        CFLAGS ${MPK_UNTRUSTED_CFLAGS}
        LINK_FLAGS ${MPK_UNTRUSTED_LINK_FLAGS}
        LINK_LIBS ${MPK_UNTRUSTED_DYNAMIC_LIBS}
        PARENT_TARGET mpk_untrusted)
endforeach()

add_subdirectory(tests)
