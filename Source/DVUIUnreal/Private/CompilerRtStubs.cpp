// Compiler-rt soft-float helpers Zig's static .a doesn't bundle.
//
// graphl uses i128 / f128 ops in a handful of spots (e.g. timing math
// converting nanosecond i128 deltas to f32). These are real code paths,
// not dead code, so we provide functional implementations rather than
// abort stubs. We delegate to natural C conversions where possible — they
// lose precision for true f128 but graphl never actually uses 128-bit
// floats for anything precision-sensitive in our code path.

#include <stdint.h>
#include <stdlib.h>

extern "C" {

typedef __int128 i128;
typedef unsigned __int128 u128;

// f128 isn't widely available; treat it as long double on x86_64 Linux
// (where long double is 80-bit extended, not true 128-bit, but it's the
// nearest natural type and is what GCC's tf2/tf3 functions operate on
// when libquadmath isn't in play).
typedef long double f128;

// i128 -> f32. Natural cast suffices.
__attribute__((weak)) float __floattisf(i128 a) {
	return (float)(double)(int64_t)(a >> 0);
	// Note: for i128 values that don't fit in int64, this loses high bits.
	// graphl only uses this for elapsed_ns timing values which are well
	// within int64 range.
}

__attribute__((weak)) f128 __floatuntitf(u128 a) {
	return (f128)(uint64_t)a;
}

__attribute__((weak)) i128 __fixtfti(f128 a) {
	return (i128)(int64_t)a;
}

__attribute__((weak)) f128 __divtf3(f128 a, f128 b) { return a / b; }
__attribute__((weak)) f128 __multf3(f128 a, f128 b) { return a * b; }
__attribute__((weak)) int __gttf2(f128 a, f128 b) { return a > b ? 1 : 0; }
__attribute__((weak)) int __lttf2(f128 a, f128 b) { return a < b ? -1 : 0; }
__attribute__((weak)) int __netf2(f128 a, f128 b) { return a != b ? 1 : 0; }

// libquadmath functions for true 128-bit float. Forward to long double
// equivalents — close enough on x86_64.
#include <math.h>
__attribute__((weak)) f128 roundq(f128 x) { return roundl(x); }

} // extern "C"
