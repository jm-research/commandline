#include <gtest/gtest.h>
#include <pthread.h>

#include "ManagedStatic.h"
#include "llvm/Support/Allocator.h"

namespace Commandline {

namespace {

namespace test1 {

Commandline::ManagedStatic<int> ms;

auto helper(void*) -> void* {
  *ms;
  return nullptr;
}

auto allocate_stack(pthread_attr_t& a, size_t n = 65536) -> void* {
  void* stack = llvm::safe_malloc(n);
  pthread_attr_init(&a);
  pthread_attr_setstack(&a, stack, n);
  return stack;
}

}  // namespace test1

TEST(Initialize, MultipleThreads) {
  pthread_attr_t a1, a2;
  void* p1 = test1::allocate_stack(a1);
  void* p2 = test1::allocate_stack(a2);

  pthread_t t1, t2;
  pthread_create(&t1, &a1, test1::helper, nullptr);
  pthread_create(&t2, &a2, test1::helper, nullptr);
  pthread_join(t1, nullptr);
  pthread_join(t2, nullptr);
  free(p1);
  free(p2);
}

namespace NestedStatics {
static ManagedStatic<int> ms1;
struct Nest {
  Nest() { ++(*ms1); }

  ~Nest() {
    assert(ms1.isConstructed());
    ++(*ms1);
  }
};

static ManagedStatic<Nest> ms2;

TEST(ManagedStaticTest, NestedStatics) {
  EXPECT_FALSE(ms1.isConstructed());
  EXPECT_FALSE(ms2.isConstructed());

  *ms2;
  EXPECT_TRUE(ms1.isConstructed());
  EXPECT_TRUE(ms2.isConstructed());
}
}  // namespace NestedStatics

namespace CustomCreatorDeletor {

static int destructor_count = 0;

struct CustomCreate {
  static auto call() -> void* {
    void* mem = llvm::safe_malloc(sizeof(int));
    *((int*)mem) = 42;
    return mem;
  }
};

struct CustomDelete {
  static void call(void* p) {
    std::free(p);
    destructor_count = 123;
  }
};

TEST(ManagedStaticTest, CustomCreatorDeletor) {
  {
    static ManagedStatic<int, CustomCreate, CustomDelete> custom;
    CommandlineShutdownObj shutdown;

    EXPECT_EQ(42, *custom);
  }

  EXPECT_EQ(destructor_count, 123);
}

}  // namespace CustomCreatorDeletor

}  // namespace

}  // namespace Commandline