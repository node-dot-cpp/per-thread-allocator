
g++ ../src/test_common.cpp ../src/random_test.cpp ../src/page_allocator_linux.cpp ../src/bucket_allocator_linux.cpp -std=c++11 -g -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-empty-body -DNDEBUG -O3 -flto -lpthread -o alloc.bin
