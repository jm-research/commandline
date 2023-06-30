#include "ManagedStatic.h"

#include <cassert>
#include <mutex>

#include "llvm/Support/Allocator.h"
#include "llvm/Support/Threading.h"

namespace Commandline {

static const ManagedStaticBase* static_list = nullptr;

static auto getManagedStaticMutex() -> std::recursive_mutex* {
  static std::recursive_mutex m;
  return &m;
}

void ManagedStaticBase::RegisterManagedStatic(void* (*creator)(),
                                              void (*deleter)(void*)) const {
  assert(creator);
  if (commandline_is_multithreaded()) {
    std::lock_guard<std::recursive_mutex> lock(*getManagedStaticMutex());

    if (!Ptr.load(std::memory_order_relaxed)) {
      void* tmp = creator();

      Ptr.store(tmp, std::memory_order_release);
      DeleterFn = deleter;

      // Add to list of managed statics.
      Next = static_list;
      static_list = this;
    }
  } else {
    assert(!Ptr && !DeleterFn && !Next &&
           "Partially initialized ManagedStatic!?");
    Ptr = creator();
    DeleterFn = deleter;

    // Add to list of managed statics.
    Next = static_list;
    static_list = this;
  }
}

void ManagedStaticBase::destroy() const {
  assert(DeleterFn && "ManagedStatic not initialized correctly!");
  assert(static_list == this &&
         "Not destroyed in reverse order of construction?");
  // Unlink from list.
  static_list = Next;
  Next = nullptr;

  // Destroy memory.
  DeleterFn(Ptr);

  // Cleanup.
  Ptr = nullptr;
  DeleterFn = nullptr;
}

void commandline_shutdown() {
  while (static_list) {
    static_list->destroy();
  }
}

}  // namespace Commandline