/*
 * test_io_buffer.c - I/O buffer corruption test
 * Tests how ADD corruption affects buffer handling and data integrity
 * 
 * Uses volatile + unrolled pattern to ensure ADDs happen at runtime
 * and can be targeted by PC range filtering for payload-only corruption.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define BUFFER_SIZE 16

/*
 * Fill buffer with sequential pattern using volatile + unrolled ADDs
 * This is the TARGET for corruption injection
 * Use: objdump -d test_io_buffer | grep '<fill_buffer_unrolled>:' -A50
 */
__attribute__((noinline))
void fill_buffer_unrolled(uint8_t *buf)
{
    volatile uint8_t base = 0x41;  /* 'A' - force runtime read */
    
    /* Unrolled: each ADD is independent, no cascading */
    buf[0]  = base + 0;   /* 'A' = 0x41 */
    buf[1]  = base + 1;   /* 'B' = 0x42 */
    buf[2]  = base + 2;   /* 'C' = 0x43 */
    buf[3]  = base + 3;   /* 'D' = 0x44 */
    buf[4]  = base + 4;   /* 'E' = 0x45 */
    buf[5]  = base + 5;   /* 'F' = 0x46 */
    buf[6]  = base + 6;   /* 'G' = 0x47 */
    buf[7]  = base + 7;   /* 'H' = 0x48 */
    buf[8]  = base + 8;   /* 'I' = 0x49 */
    buf[9]  = base + 9;   /* 'J' = 0x4A */
    buf[10] = base + 10;  /* 'K' = 0x4B */
    buf[11] = base + 11;  /* 'L' = 0x4C */
    buf[12] = base + 12;  /* 'M' = 0x4D */
    buf[13] = base + 13;  /* 'N' = 0x4E */
    buf[14] = base + 14;  /* 'O' = 0x4F */
    buf[15] = base + 15;  /* 'P' = 0x50 */
}

/* Expected values for verification (computed at compile time) */
static const uint8_t expected_buffer[BUFFER_SIZE] = {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50
};

/*
 * Compute checksum using volatile accumulator
 * Another TARGET for corruption injection
 */
__attribute__((noinline))
uint16_t compute_checksum_unrolled(uint8_t *data)
{
    volatile uint16_t sum = 0;  /* Force runtime accumulation */
    
    /* Unrolled checksum - each ADD can be independently corrupted */
    sum = sum + data[0];
    sum = sum + data[1];
    sum = sum + data[2];
    sum = sum + data[3];
    sum = sum + data[4];
    sum = sum + data[5];
    sum = sum + data[6];
    sum = sum + data[7];
    sum = sum + data[8];
    sum = sum + data[9];
    sum = sum + data[10];
    sum = sum + data[11];
    sum = sum + data[12];
    sum = sum + data[13];
    sum = sum + data[14];
    sum = sum + data[15];
    
    return sum;
}

/* Pre-computed expected checksum: 0x41+0x42+...+0x50 = 0x0488 */
#define EXPECTED_CHECKSUM 0x0488

/*
 * Test 1: Buffer fill corruption
 * Shows which individual bytes get corrupted
 */
void test_buffer_fill(void)
{
    printf("[1] Buffer Fill Test (target: fill_buffer_unrolled)\n");
    printf("Expected: ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        printf("%02X ", expected_buffer[i]);
    }
    printf("\n");
    
    uint8_t buf[BUFFER_SIZE] = {0};
    fill_buffer_unrolled(buf);
    
    printf("Got:      ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
    
    /* Check each byte */
    int corrupted = 0;
    printf("Status:   ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (buf[i] == expected_buffer[i]) {
            printf("OK ");
        } else {
            int diff = (int)buf[i] - (int)expected_buffer[i];
            printf("%+d ", diff);
            corrupted++;
        }
    }
    printf("\n");
    
    printf("Result: %d of %d bytes corrupted\n", corrupted, BUFFER_SIZE);
}

/*
 * Test 2: Checksum corruption
 * Shows how accumulated sums drift with injected errors
 */
void test_checksum(void)
{
    printf("\n[2] Checksum Test (target: compute_checksum_unrolled)\n");
    
    /* Use known good data */
    uint8_t data[BUFFER_SIZE];
    for (int i = 0; i < BUFFER_SIZE; i++) {
        data[i] = expected_buffer[i];  /* Copy without ADD corruption */
    }
    
    printf("Data: ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    printf("Expected checksum: 0x%04X\n", EXPECTED_CHECKSUM);
    
    uint16_t computed = compute_checksum_unrolled(data);
    printf("Computed checksum: 0x%04X\n", computed);
    
    if (computed == EXPECTED_CHECKSUM) {
        printf("Result: OK - checksum matches\n");
    } else {
        int diff = (int)computed - (int)EXPECTED_CHECKSUM;
        printf("Result: CORRUPT (diff=%+d)\n", diff);
    }
}

/*
 * Test 3: End-to-end buffer + checksum
 * If we corrupt the fill AND the checksum, do errors cancel out?
 */
void test_end_to_end(void)
{
    printf("\n[3] End-to-End Test (both functions targeted)\n");
    
    uint8_t buf[BUFFER_SIZE] = {0};
    fill_buffer_unrolled(buf);
    
    /* Compute checksum of potentially corrupted buffer */
    uint16_t checksum1 = compute_checksum_unrolled(buf);
    uint16_t checksum2 = compute_checksum_unrolled(buf);
    
    printf("Checksum run 1: 0x%04X\n", checksum1);
    printf("Checksum run 2: 0x%04X\n", checksum2);
    
    if (checksum1 == checksum2) {
        printf("Consistency: OK - both runs match\n");
    } else {
        printf("Consistency: CORRUPT - runs differ by %+d\n", 
               (int)checksum2 - (int)checksum1);
    }
    
    /* Count actual corrupted bytes */
    int corrupted = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (buf[i] != expected_buffer[i]) corrupted++;
    }
    printf("Buffer corruption: %d bytes\n", corrupted);
}

int main(void)
{
    printf("=== I/O Buffer Corruption Test ===\n");
    printf("Target functions: fill_buffer_unrolled, compute_checksum_unrolled\n\n");
    
    test_buffer_fill();
    test_checksum();
    test_end_to_end();
    
    printf("\n=== Done ===\n");
    return 0;
}
