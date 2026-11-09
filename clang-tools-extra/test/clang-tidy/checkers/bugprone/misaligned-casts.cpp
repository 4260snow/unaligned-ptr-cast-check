#include <cstdint>

__attribute__((noinline))
void test1() {
  alignas(4) char buffer[16] = {};

  uint32_t *p = (uint32_t *)&buffer[1];

  volatile uint32_t value = *p;
}

__attribute__((noinline))
void test2() {
  alignas(4) char buffer[16] = {};

  uint32_t *p =
      reinterpret_cast<uint32_t *>(&buffer[1]);

  volatile uint32_t value = *p;
}

__attribute__((noinline))
void test3(char *data, unsigned offset) {
  uint32_t *p =
      reinterpret_cast<uint32_t *>(data + offset);

  volatile uint32_t value = *p;
}

struct alignas(16) Header {
  uint64_t value;
};

__attribute__((noinline))
void test4(char *data) {
  Header *header =
      reinterpret_cast<Header *>(data);

  volatile uint64_t value = header->value;
}

__attribute__((noinline))
void test5(void *data) {
  int *p = static_cast<int *>(data);

  volatile int value = *p;
}
