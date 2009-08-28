#define CIRCULAR_A(foo) CIRCULAR_B(foo*A)
#define CIRCULAR_B(foo) CIRCULAR_A(foo*B)
"circular_a(100): "; CIRCULAR_A(100);
"circular_a(circular_b(50)): "; CIRCULAR_A(CIRCULAR_B(50));
