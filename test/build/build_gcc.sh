g++-10 ../test_common.cpp ../random_test.cpp ../../src/foundation/src/page_allocator.cpp ../../src/foundation/src/log.cpp ../../src/foundation/src/nodecpp_assert.cpp ../../src/iibmalloc.cpp ../../src/foundation/src/std_error.cpp ../../src/foundation/src/safe_memory_error.cpp ../../src/foundation/src/tagged_ptr_impl.cpp ../../src/foundation/src/stack_info.cpp ../../src/foundation/3rdparty/fmt/src/format.cc -I../../src/foundation/3rdparty/fmt/include -I../../src/foundation/include -I../../src -std=c++17 -rdynamic -ldl -g -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-empty-body -DNDEBUG -O2 -flto -lpthread -o alloc.bin