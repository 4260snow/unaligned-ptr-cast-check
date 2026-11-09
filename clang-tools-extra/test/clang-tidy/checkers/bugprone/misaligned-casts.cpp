#include <cstdint>

void test1() {
    alignas(4) char buffer[16] = {};

    uint32_t *p = (uint32_t *)&buffer[1];

    (void)*p;
}

void test2() {
    alignas(4) char buffer[16] = {};

    uint32_t *p =
        reinterpret_cast<uint32_t *>(&buffer[1]);

    (void)*p;
}

void test3(char *data, unsigned offset) {
    uint32_t *p =
        reinterpret_cast<uint32_t *>(data + offset);

    (void)*p;
}

struct alignas(16) Header {
    uint64_t value;
};

void test4(char *data) {
    Header *header =
        reinterpret_cast<Header *>(data);

    (void)header->value;
}

void test5(void *data) {
    int *p = static_cast<int *>(data);

    (void)*p;
}

int main() {
    alignas(4) char buffer1[16] = {};
    alignas(16) char buffer2[32] = {};

    test1();
    test2();
    test3(buffer1, 1);
    test4(buffer2 + 1);
    test5(buffer1 + 1);

    return 0;
}