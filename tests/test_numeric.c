/*
 * test_numeric.c - Numeric computation corruption test
 * Tests how ADD corruption affects arithmetic operations
 * 
 * Uses volatile + unrolled pattern for predictable injection targets
 */
#include <stdio.h>
#include <stdint.h>

/*
 * Compute sum of 1..16 using volatile accumulator
 * This is the TARGET for corruption injection
 * Use: objdump -d test_numeric | grep '<compute_sum_unrolled>:' -A50
 */
__attribute__((noinline))
uint32_t compute_sum_unrolled(void)
{
    volatile uint32_t sum = 0;  /* Force runtime accumulation */
    
    /* Unrolled: each ADD is independent */
    sum = sum + 1;
    sum = sum + 2;
    sum = sum + 3;
    sum = sum + 4;
    sum = sum + 5;
    sum = sum + 6;
    sum = sum + 7;
    sum = sum + 8;
    sum = sum + 9;
    sum = sum + 10;
    sum = sum + 11;
    sum = sum + 12;
    sum = sum + 13;
    sum = sum + 14;
    sum = sum + 15;
    sum = sum + 16;
    
    return sum;
}

/* Pre-computed expected: 1+2+...+16 = 136 */
#define EXPECTED_SUM 136

/*
 * Compute product-like value using repeated addition
 * Another TARGET for corruption
 */
__attribute__((noinline))
uint32_t compute_scaled_sum(void)
{
    volatile uint32_t result = 0;
    
    /* Compute 5 * (1+2+3+4) = 5 * 10 = 50 using only ADDs */
    /* Value 1: add 5 times */
    result = result + 1;
    result = result + 1;
    result = result + 1;
    result = result + 1;
    result = result + 1;
    /* Value 2: add 5 times */
    result = result + 2;
    result = result + 2;
    result = result + 2;
    result = result + 2;
    result = result + 2;
    /* Value 3: add 5 times */
    result = result + 3;
    result = result + 3;
    result = result + 3;
    result = result + 3;
    result = result + 3;
    /* Value 4: add 5 times */
    result = result + 4;
    result = result + 4;
    result = result + 4;
    result = result + 4;
    result = result + 4;
    
    return result;
}

/* Pre-computed expected: 5*1 + 5*2 + 5*3 + 5*4 = 50 */
#define EXPECTED_SCALED 50

int main(void)
{
    printf("=== Numeric Corruption Test ===\n");
    printf("Target functions: compute_sum_unrolled, compute_scaled_sum\n\n");
    
    /* Test 1: Simple sum */
    printf("[1] Sum of 1..16 (expected: %u)\n", EXPECTED_SUM);
    
    uint32_t sum1 = compute_sum_unrolled();
    uint32_t sum2 = compute_sum_unrolled();
    uint32_t sum3 = compute_sum_unrolled();
    
    printf("Run 1: %u", sum1);
    if (sum1 == EXPECTED_SUM) printf(" (OK)\n");
    else printf(" (CORRUPT diff=%+d)\n", (int)sum1 - EXPECTED_SUM);
    
    printf("Run 2: %u", sum2);
    if (sum2 == EXPECTED_SUM) printf(" (OK)\n");
    else printf(" (CORRUPT diff=%+d)\n", (int)sum2 - EXPECTED_SUM);
    
    printf("Run 3: %u", sum3);
    if (sum3 == EXPECTED_SUM) printf(" (OK)\n");
    else printf(" (CORRUPT diff=%+d)\n", (int)sum3 - EXPECTED_SUM);
    
    /* Test 2: Scaled sum */
    printf("\n[2] Scaled sum 5*(1+2+3+4) (expected: %u)\n", EXPECTED_SCALED);
    
    uint32_t scaled1 = compute_scaled_sum();
    uint32_t scaled2 = compute_scaled_sum();
    
    printf("Run 1: %u", scaled1);
    if (scaled1 == EXPECTED_SCALED) printf(" (OK)\n");
    else printf(" (CORRUPT diff=%+d)\n", (int)scaled1 - EXPECTED_SCALED);
    
    printf("Run 2: %u", scaled2);
    if (scaled2 == EXPECTED_SCALED) printf(" (OK)\n");
    else printf(" (CORRUPT diff=%+d)\n", (int)scaled2 - EXPECTED_SCALED);
    
    /* Summary */
    printf("\n[3] Consistency check\n");
    int inconsistent = 0;
    if (sum1 != sum2 || sum2 != sum3) {
        printf("Sum runs inconsistent: %u, %u, %u\n", sum1, sum2, sum3);
        inconsistent++;
    }
    if (scaled1 != scaled2) {
        printf("Scaled runs inconsistent: %u, %u\n", scaled1, scaled2);
        inconsistent++;
    }
    
    if (inconsistent) {
        printf("Result: UNSTABLE - corruption varies between runs\n");
    } else {
        printf("Result: STABLE - all runs consistent\n");
    }
    
    printf("\n=== Done ===\n");
    return 0;
}
