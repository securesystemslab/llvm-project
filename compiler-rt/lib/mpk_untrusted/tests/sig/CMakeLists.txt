#project (sig)
#add_executable(sig src/sig.c)

set(MPK_UNIT_SOURCES
	sig.cpp
	mpk_untrusted_unit_test_main.cpp
	)

set(MPK_UNIT_HEADERS
	../util/mpk_untrusted_test_config.h)

add_mpk_unittest(MPKUntrustedUnitTest
	SOURCES ${MPK_UNIT_SOURCES}
	HEADERS ${MPK_UNIT_HEADERS})
