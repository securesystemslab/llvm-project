#project (alloc_test)
#add_executable(alloc_test src/alloc_site_handler_tests.cpp)

set(MPK_UNIT_SOURCES
       alloc_site_handler_tests.cpp
       mpk_untrusted_unit_test_main.cpp
       )

set(MPK_UNIT_HEADERS
       ../util/mpk_untrusted_test_config.h)

add_mpk_unittest(UnitTests
       SOURCES ${MPK_UNIT_SOURCES}
       HEADERS ${MPK_UNIT_HEADERS})
