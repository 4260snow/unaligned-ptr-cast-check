// RUN: %check_clang_tidy %s bugprone-misaligned-pointer-cast %t
// CHECK-MESSAGES: warning: pointer cast increases required alignment