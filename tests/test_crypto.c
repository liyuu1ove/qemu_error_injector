/*
 * test_crypto.c - Crypto/hash corruption test
 * Tests how ADD corruption affects hash computation and encryption
 * 
 * Uses volatile + unrolling for reliable corruption.
 * Compile: gcc -O0 -g -static -o test_crypto test_crypto.c
 * Find PC: objdump -d test_crypto | grep '<compute_hash>:' -A40
 *          objdump -d test_crypto | grep '<xor_round>:' -A30
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define DATA_SIZE 8

/* Pre-computed expected hash for "ABCDEFGH" = sum of ASCII values */
static const uint32_t expected_hash = 0x41+0x42+0x43+0x44+0x45+0x46+0x47+0x48; /* 548 */

/* Pre-computed expected XOR result with key=0x55 */
static const uint8_t expected_xor[DATA_SIZE] = {
    0x41^0x55, 0x42^0x56, 0x43^0x57, 0x44^0x58,
    0x45^0x59, 0x46^0x5A, 0x47^0x5B, 0x48^0x5C
};

/*
 * TARGET FUNCTION 1: Simple checksum hash using ADDs
 */
__attribute__((noinline))
uint32_t compute_hash(const uint8_t *data)
{
    volatile uint32_t hash = 0;
    
    hash = hash + data[0];
    hash = hash + data[1];
    hash = hash + data[2];
    hash = hash + data[3];
    hash = hash + data[4];
    hash = hash + data[5];
    hash = hash + data[6];
    hash = hash + data[7];
    
    return hash;
}

/*
 * TARGET FUNCTION 2: XOR encryption with key schedule using ADDs
 */
__attribute__((noinline))
void xor_round(uint8_t *data, uint8_t base_key)
{
    volatile uint8_t key = base_key;
    
    data[0] ^= (key + 0);
    data[1] ^= (key + 1);
    data[2] ^= (key + 2);
    data[3] ^= (key + 3);
    data[4] ^= (key + 4);
    data[5] ^= (key + 5);
    data[6] ^= (key + 6);
    data[7] ^= (key + 7);
}

int main(void)
{
    uint8_t input[DATA_SIZE] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'};
    uint8_t encrypted[DATA_SIZE];
    
    printf("=== Crypto/Hash Corruption Test ===\n\n");
    
    /* Test 1: Hash computation */
    printf("[1] Hash Computation:\n");
    printf("  Input: ");
    for (int i = 0; i < DATA_SIZE; i++) printf("%c", input[i]);
    printf("\n");
    
    uint32_t actual_hash = compute_hash(input);
    printf("  Expected hash: %u (0x%X)\n", expected_hash, expected_hash);
    printf("  Actual hash:   %u (0x%X)\n", actual_hash, actual_hash);
    
    if (actual_hash != expected_hash) {
        printf("  STATUS: CORRUPT (diff=%+d)\n", (int)actual_hash - (int)expected_hash);
    } else {
        printf("  STATUS: OK\n");
    }
    
    /* Test 2: XOR encryption */
    printf("\n[2] XOR Encryption (key=0x55):\n");
    memcpy(encrypted, input, DATA_SIZE);
    xor_round(encrypted, 0x55);
    
    printf("  Index  Input  Expected  Actual    Status\n");
    printf("  -----  -----  --------  ------    ------\n");
    int xor_errors = 0;
    for (int i = 0; i < DATA_SIZE; i++) {
        if (encrypted[i] != expected_xor[i]) {
            printf("  [%d]    0x%02X   0x%02X      0x%02X      CORRUPT (diff=%+d)\n",
                   i, input[i], expected_xor[i], encrypted[i],
                   (int)encrypted[i] - (int)expected_xor[i]);
            xor_errors++;
        } else {
            printf("  [%d]    0x%02X   0x%02X      0x%02X      OK\n",
                   i, input[i], expected_xor[i], encrypted[i]);
        }
    }
    
    /* Test 3: Hash consistency (same input twice) */
    printf("\n[3] Hash Consistency Check:\n");
    uint32_t hash1 = compute_hash(input);
    uint32_t hash2 = compute_hash(input);
    printf("  Hash run 1: %u\n", hash1);
    printf("  Hash run 2: %u\n", hash2);
    if (hash1 != hash2) {
        printf("  STATUS: INCONSISTENT - hash varies between runs!\n");
    } else {
        printf("  STATUS: CONSISTENT\n");
    }
    
    printf("\n=== Summary: Hash %s, XOR errors: %d of %d ===\n",
           actual_hash == expected_hash ? "OK" : "CORRUPT", xor_errors, DATA_SIZE);
    
    return 0;
}
