
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

// Qualified byte pointer: same alignment escalation.
void test5(const volatile unsigned char *data) {
  // CHECK-MESSAGES: :[[@LINE+1]]:{{[0-9]+}}: warning: pointer cast increases required alignment
  const volatile uint32_t *p = reinterpret_cast<const volatile uint32_t *>(data);
  (void)p;
}

struct alignas(8) AlignedBlock {
  char data[16];
};

// Same size, but alignment increases from 8 to 16.
void test6(AlignedBlock *data) {
  // CHECK-MESSAGES: :[[@LINE+1]]:{{[0-9]+}}: warning: pointer cast increases required alignment from 8 to 16 bytes
  Header *p = reinterpret_cast<Header *>(data);
  (void)p;
}

// Array destination: alignment follows the element type.
void test7(char *data) {
  // CHECK-MESSAGES: :[[@LINE+1]]:{{[0-9]+}}: warning: pointer cast increases required alignment
  uint32_t (*p)[4] = reinterpret_cast<uint32_t (*)[4]>(data);
  (void)p;
}

// Misaligned address hidden behind void*.
void test20() {
  alignas(Header) char buffer[2 * sizeof(Header)];

  void *raw = &buffer[1];
  Header *p = static_cast<Header *>(raw);
  (void)p;
}

//===----------------------------------------------------------------------===//
// Negative: cases that should not be diagnosed.
//===----------------------------------------------------------------------===//

// No alignment escalation.
void test8() {
  char buffer[16];

  char *p = &buffer[1];
  (void)p;
}

// void* source: its original pointee type and alignment are unknown.
// The current checker intentionally skips this case.
void test9(void *data) {
  int *p = static_cast<int *>(data);
  (void)p;
}

// Incomplete source type.
struct Incomplete;

void test10(Incomplete *data) {
  int *p = reinterpret_cast<int *>(data);
  (void)p;
}

// Function pointer cast is not an object-pointer alignment cast.
using Function = int (*)();

int function();

void test11() {
  Function p = reinterpret_cast<Function>(&function);
  (void)p;
}

// Same alignment requirements: this is not an alignment issue.
// It may be an object representation / aliasing issue, but that is
// outside the scope of this checker.
void test12(int *data) {
  float *p = reinterpret_cast<float *>(data);
  (void)p;
}

// Different object types, but both have alignment 1.
// Size/layout compatibility is outside the scope of this checker.
struct Bytes {
  char data[8];
};

void test13(char *data) {
  Bytes *p = reinterpret_cast<Bytes *>(data);
  (void)p;
}

// Decreasing alignment requirements.
void test14(Header *data) {
  AlignedBlock *p = reinterpret_cast<AlignedBlock *>(data);
  (void)p;
}

// Incomplete destination type.
void test15(char *data) {
  Incomplete *p = reinterpret_cast<Incomplete *>(data);
  (void)p;
}

// Conversion to void*.
void test16(Header *data) {
  void *p = static_cast<void *>(data);
  (void)p;
}

// const_cast: no alignment change.
void test17(const uint32_t *data) {
  uint32_t *p = const_cast<uint32_t *>(data);
  (void)p;
}

// The buffer is explicitly aligned, and its base address is suitable
// for uint32_t. The current checker only compares pointee type alignment.
void test18() {
  alignas(16) char buffer[16];

  // CHECK-MESSAGES: :[[@LINE+1]]:{{[0-9]+}}: warning: pointer cast increases required alignment
  uint32_t *p = reinterpret_cast<uint32_t *>(buffer);
  (void)p;
}

//===----------------------------------------------------------------------===//
// Known limitations of the current PoC.
//===----------------------------------------------------------------------===//

// FIXME: False positive.
// malloc returns suitably aligned storage for types with fundamental
// alignment requirements, but the checker skips void* casts.
void test19() {
  void *raw = std::malloc(sizeof(uint32_t));
  uint32_t *p = static_cast<uint32_t *>(raw);
  (void)p;
  std::free(raw);
}

// FIXME: False negative.
// Misaligned address produced through integer arithmetic.
void test21() {
  alignas(Header) char buffer[2 * sizeof(Header)];

  __UINTPTR_TYPE__ address = reinterpret_cast<__UINTPTR_TYPE__>(buffer);
  Header *p = reinterpret_cast<Header *>(address + 1);
  (void)p;
}
