
// RUN: %check_clang_tidy %s bugprone-misaligned-pointer-cast %t

#include <cstdint>
#include <cstdlib>

//===----------------------------------------------------------------------===//
// Positive: casts that increase alignment requirements.
//===----------------------------------------------------------------------===//

// C-style cast: char (alignment 1) -> uint32_t (typically alignment 4).
void test1() {
  alignas(4) char buffer[16] = {};

  // CHECK-MESSAGES: :[[@LINE+1]]:{{[0-9]+}}: warning: pointer cast increases required alignment
  uint32_t *p = (uint32_t *)&buffer[1];
  (void)p;
}

// C++ reinterpret_cast: same alignment escalation.
void test2() {
  alignas(4) char buffer[16] = {};

  // CHECK-MESSAGES: :[[@LINE+1]]:{{[0-9]+}}: warning: pointer cast increases required alignment
  uint32_t *p = reinterpret_cast<uint32_t *>(&buffer[1]);
  (void)p;
}

// Pointer arithmetic: the actual offset is unknown to the checker.
void test3(char *data, unsigned offset) {
  // CHECK-MESSAGES: :[[@LINE+1]]:{{[0-9]+}}: warning: pointer cast increases required alignment
  uint32_t *p = reinterpret_cast<uint32_t *>(data + offset);
  (void)p;
}

// Explicitly over-aligned destination type.
struct alignas(16) Header {
  uint64_t value;
};

void test4(char *data) {
  // CHECK-MESSAGES: :[[@LINE+1]]:{{[0-9]+}}: warning: pointer cast increases required alignment
  Header *header = reinterpret_cast<Header *>(data);
  (void)header;
}

//===----------------------------------------------------------------------===//
// Negative: cases that should not be diagnosed.
//===----------------------------------------------------------------------===//

// No alignment escalation.
void test5() {
  char buffer[16];

  char *p = &buffer[1];
  (void)p;
}

// void* source: its original pointee type and alignment are unknown.
// The current checker intentionally skips this case.
void test6(void *data) {
  int *p = static_cast<int *>(data);
  (void)p;
}

// Incomplete source type.
struct Incomplete;

void test7(Incomplete *data) {
  int *p = reinterpret_cast<int *>(data);
  (void)p;
}

// Function pointer cast is not an object-pointer alignment cast.
using Function = int (*)();

int function();

void test8() {
  Function p = reinterpret_cast<Function>(&function);
  (void)p;
}

// Same alignment requirements: this is not an alignment issue.
// It may be an object representation / aliasing issue, but that is
// outside the scope of this checker.
void test9(int *data) {
  float *p = reinterpret_cast<float *>(data);
  (void)p;
}

// Different object types, but both have alignment 1.
// Size/layout compatibility is outside the scope of this checker.
struct Bytes {
  char data[8];
};

void test10(char *data) {
  Bytes *p = reinterpret_cast<Bytes *>(data);
  (void)p;
}

//===----------------------------------------------------------------------===//
// Known limitations of the current PoC.
//===----------------------------------------------------------------------===//

// FIXME: False positive.
// The buffer is explicitly aligned, and its base address is suitable
// for uint32_t. The current checker only compares pointee type alignment.
void test11() {
  alignas(16) char buffer[16];

  // CHECK-MESSAGES: :[[@LINE+1]]:{{[0-9]+}}: warning: pointer cast increases required alignment
  uint32_t *p = reinterpret_cast<uint32_t *>(buffer);
  (void)p;
}

// FIXME: False negative / unknown provenance.
// malloc returns suitably aligned storage for types with fundamental
// alignment requirements, but the checker skips void* casts.
void test12() {
  void *raw = std::malloc(sizeof(uint32_t));
  uint32_t *p = static_cast<uint32_t *>(raw);
  (void)p;
  std::free(raw);
}
