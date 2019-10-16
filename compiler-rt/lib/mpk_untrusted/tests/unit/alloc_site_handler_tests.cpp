// TODO: This should not need a relative path, either use an include dir or
// relocate in lib
#include "../util/mpk_untrusted_test_config.h"
#include "alloc_site_handler.h"
#include "mpk.h"
#include "mpk_fault_handler.h"
#include "gtest/gtest.h"
#include <limits>
#include <memory>
#include <pthread.h>

namespace __mpk_untrusted {

const uint32_t PKEY_DEFAULT_VALUE = 1;

TEST(AllocSite, invalidAllocSite) {
  EXPECT_DEATH_IF_SUPPORTED(AllocSite(nullptr, 0, -1), "");
}

// For this test to work properly, we have to ensure all other tests
// remove all allocations that they have added to the map.
TEST(getAllocSite, EmptyMap) {
  auto handle = AllocSiteHandler::getOrInit();
  auto allocSite = handle->getAllocSite(nullptr);
  EXPECT_EQ(allocSite.get(), AllocSite::error().get());
}

TEST(getAllocSite, InvalidPreAddress) {
  auto handle = AllocSiteHandler::getOrInit();
  auto ptr = (rust_ptr)malloc(sizeof(uint64_t));
  handle->insertAllocSite(ptr, std::make_shared<AllocSite>(
                                   ptr, sizeof(uint64_t), PKEY_DEFAULT_VALUE));
  auto decPtr = (rust_ptr)((uintptr_t)ptr - 1);
  auto allocSite = handle->getAllocSite(decPtr);
  EXPECT_EQ(allocSite.get(), AllocSite::error().get());
  handle->removeAllocSite(ptr);
}

TEST(getAllocSite, InvalidPostAddress) {
  auto handle = AllocSiteHandler::getOrInit();
  auto ptr = (rust_ptr)malloc(sizeof(uint64_t));
  handle->insertAllocSite(ptr, std::make_shared<AllocSite>(
                                   ptr, sizeof(uint64_t), PKEY_DEFAULT_VALUE));
  auto incPtr = (rust_ptr)((uintptr_t)ptr + sizeof(uint64_t));
  auto allocSite = handle->getAllocSite(incPtr);
  EXPECT_EQ(allocSite.get(), AllocSite::error().get());
  handle->removeAllocSite(ptr);
}

TEST(getAllocSite, InvalidBetweenAddress) {
  auto handle = AllocSiteHandler::getOrInit();
  auto ptr = (rust_ptr)malloc(sizeof(uint64_t));
  auto ptr2 = (rust_ptr)((uintptr_t)ptr + (sizeof(uint64_t) * 2));
  handle->insertAllocSite(ptr, std::make_shared<AllocSite>(
                                   ptr, sizeof(uint64_t), PKEY_DEFAULT_VALUE));
  handle->insertAllocSite(ptr2, std::make_shared<AllocSite>(
                                    ptr, sizeof(uint64_t), PKEY_DEFAULT_VALUE));
  auto decPtr = (rust_ptr)((uintptr_t)ptr + (sizeof(uint64_t) * 1));
  auto allocSite = handle->getAllocSite(decPtr);
  EXPECT_EQ(allocSite.get(), AllocSite::error().get());
  handle->removeAllocSite(ptr);
  handle->removeAllocSite(ptr2);
}

TEST(getAllocSite, ValidBetweenAddress) {
  auto handle = AllocSiteHandler::getOrInit();
  auto ptr = (rust_ptr)malloc(sizeof(uint64_t));
  auto ptr2 = (rust_ptr)((uintptr_t)ptr + (sizeof(uint64_t) * 4));
  auto newAllocSite = std::make_shared<AllocSite>(ptr, sizeof(uint64_t) * 2,
                                                  PKEY_DEFAULT_VALUE);
  handle->insertAllocSite(ptr, newAllocSite);
  handle->insertAllocSite(ptr2, std::make_shared<AllocSite>(
                                    ptr, sizeof(uint64_t), PKEY_DEFAULT_VALUE));
  auto decPtr = (rust_ptr)((uintptr_t)ptr + (sizeof(uint64_t) * 1));
  auto getAllocSite = handle->getAllocSite(decPtr);
  EXPECT_EQ(getAllocSite.get(), newAllocSite.get());
  handle->removeAllocSite(ptr);
  handle->removeAllocSite(ptr2);
}

// Note: No Allocation Site should have a nullptr, thus nullptr should not map
// to any allocation site. However, in this contrived example, if an Allocation
// Site without a nullptr in its metadata were assigned to nullptr in the map,
// it would retrieve that AllocSite.
TEST(getAllocSite, nullptrAddress) {
  auto handle = AllocSiteHandler::getOrInit();
  auto randomPtr = (rust_ptr)malloc(sizeof(uint64_t));
  auto errorSite = std::make_shared<AllocSite>(randomPtr, sizeof(uint64_t),
                                               PKEY_DEFAULT_VALUE);
  handle->insertAllocSite(nullptr, errorSite);
  auto nullSite = handle->getAllocSite(nullptr);
  EXPECT_EQ(errorSite.get(), nullSite.get());
}

TEST(getAllocSite, PointerArithmeticOverflowWraps) {
  auto ptr = (uintptr_t)(-1); // max pointer size
  auto rPtr = (rust_ptr)ptr;
  auto newAllocSite =
      std::make_shared<AllocSite>(rPtr, sizeof(uint64_t), PKEY_DEFAULT_VALUE);
  EXPECT_FALSE(newAllocSite->containsPtr(++rPtr));
}

TEST(faultingAllocs, addFaultAlloc) {
  auto handle = AllocSiteHandler::getOrInit();
  auto ptr = (rust_ptr)malloc(sizeof(uint64_t));
  auto newAllocSite =
      std::make_shared<AllocSite>(ptr, sizeof(uint64_t), PKEY_DEFAULT_VALUE);
  handle->insertAllocSite(ptr, newAllocSite);
  handle->addFaultAlloc(ptr, 1);
  auto fault_set = handle->faultingAllocs();
  if (fault_set.empty())
    FAIL() << "Error Adding AllocSite to error set!\n";

  EXPECT_EQ((*fault_set.begin())->getPtr(), newAllocSite->getPtr());
  EXPECT_EQ((*fault_set.begin())->id(), newAllocSite->id());
  handle->removeAllocSite(ptr);
  fault_set.clear();
}

TEST(pkeyMap, negativeThreadID) {
  auto handle = AllocSiteHandler::getOrInit();
  PendingPKeyInfo pkinf(1, PKEY_DISABLE_ACCESS);
  handle->storePendingPKeyInfo(-1, pkinf);
  EXPECT_EQ(pkinf.access_rights,
            handle->getAndRemove(-1).getValue().access_rights);
}

TEST(pkeyMap, getAndRemoveEmptyMapReturnsNone) {
  auto handle = AllocSiteHandler::getOrInit();
  if (handle->getAndRemove(gettid()))
    FAIL() << "Getting pkey info while map is empty should return Null "
              "optional.\n";
}

// TODO : How to test thread locks are working as intended?

} // namespace __mpk_untrusted
