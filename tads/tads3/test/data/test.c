#define A   1
#define D(x) defined(x)
#define D2(x) defined(A)

#if D(A)
#pragma message("D(A) is true")
#else
#pragma message("D(A) is false")
#endif

#if D(B)
#pragma message("D(B) is true")
#else
#pragma message("D(B) is false")
#endif


#if D2(A)
#pragma message("D2(A) is true")
#else
#pragma message("D2(A) is false")
#endif

#if D2(B)
#pragma message("D2(B) is true")
#else
#pragma message("D2(B) is false")
#endif


