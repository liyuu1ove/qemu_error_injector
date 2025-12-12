/*
 * test_string.c - String/buffer corruption test
 * Tests how ADD corruption affects string operations (relevant for I/O)
 * 
 * Uses volatile + unrolled pattern for predictable injection targets
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define STRING_LEN 16

/*
 * Fill string buffer with known pattern using volatile + unrolled
 * TARGET for corruption injection
 */
__attribute__((noinline))
void fill_string_unrolled(char *buf)
{
    volatile char base = 'A';  /* Force runtime read */
    
    /* Unrolled: each character computed independently */
    buf[0]  = base + 0;   /* 'A' */
    buf[1]  = base + 1;   /* 'B' */
    buf[2]  = base + 2;   /* 'C' */
    buf[3]  = base + 3;   /* 'D' */
    buf[4]  = base + 4;   /* 'E' */
    buf[5]  = base + 5;   /* 'F' */
    buf[6]  = base + 6;   /* 'G' */
    buf[7]  = base + 7;   /* 'H' */
    buf[8]  = base + 8;   /* 'I' */
    buf[9]  = base + 9;   /* 'J' */
    buf[10] = base + 10;  /* 'K' */
    buf[11] = base + 11;  /* 'L' */
    buf[12] = base + 12;  /* 'M' */
    buf[13] = base + 13;  /* 'N' */
    buf[14] = base + 14;  /* 'O' */
    buf[15] = base + 15;  /* 'P' */
}

/* Expected string for verification */
static const char expected_string[STRING_LEN + 1] = "ABCDEFGHIJKLMNOP";

/*
 * Compute string checksum using volatile accumulator
 * Another TARGET for corruption
 */
__attribute__((noinline))
uint32_t string_checksum_unrolled(const char *str)
{
    volatile uint32_t sum = 0;
    
    /* Unrolled checksum */
    sum = sum + (uint8_t)str[0];
    sum = sum + (uint8_t)str[1];
    sum = sum + (uint8_t)str[2];
    sum = sum + (uint8_t)str[3];
    sum = sum + (uint8_t)str[4];
    sum = sum + (uint8_t)str[5];
    sum = sum + (uint8_t)str[6];
    sum = sum + (uint8_t)str[7];
    sum = sum + (uint8_t)str[8];
    sum = sum + (uint8_t)str[9];
    sum = sum + (uint8_t)str[10];
    sum = sum + (uint8_t)str[11];
    sum = sum + (uint8_t)str[12];
    sum = sum + (uint8_t)str[13];
    sum = sum + (uint8_t)str[14];
    sum = sum + (uint8_t)str[15];
    
    return sum;
}

/* Pre-computed: 'A'+'B'+...+'P' = 65+66+...+80 = 1160 */
#define EXPECTED_STRING_CHECKSUM 1160

int main(void)
{
    printf("=== String/Buffer Corruption Test ===\n");
    printf("Target functions: fill_string_unrolled, string_checksum_unrolled\n\n");
    
    /* Test 1: String fill */
    printf("[1] String Fill Test\n");
    printf("Expected: \"%s\"\n", expected_string);
    
    char buffer[STRING_LEN + 1] = {0};
    fill_string_unrolled(buffer);
    buffer[STRING_LEN] = '\0';
    
    printf("Got:      \"%s\"\n", buffer);
    
    /* Character-by-character comparison */
    int corrupted = 0;
    printf("Status:    ");
    for (int i = 0; i < STRING_LEN; i++) {
        if (buffer[i] == expected_string[i]) {
            printf(".");
        } else {
            printf("X");
            corrupted++;
        }
    }
    printf("\n");
    printf("Result: %d of %d characters corrupted\n", corrupted, STRING_LEN);
    
    /* Test 2: String checksum */
    printf("\n[2] String Checksum Test\n");
    printf("Input: \"%s\" (expected checksum: %u)\n", 
           expected_string, EXPECTED_STRING_CHECKSUM);
    
    uint32_t cksum1 = string_checksum_unrolled(expected_string);
    uint32_t cksum2 = string_checksum_unrolled(expected_string);
    
    printf("Checksum run 1: %u", cksum1);
    if (cksum1 == EXPECTED_STRING_CHECKSUM) printf(" (OK)\n");
    else printf(" (CORRUPT diff=%+d)\n", (int)cksum1 - EXPECTED_STRING_CHECKSUM);
    
    printf("Checksum run 2: %u", cksum2);
    if (cksum2 == EXPECTED_STRING_CHECKSUM) printf(" (OK)\n");
    else printf(" (CORRUPT diff=%+d)\n", (int)cksum2 - EXPECTED_STRING_CHECKSUM);
    
    /* Test 3: Corrupted string checksum */
    printf("\n[3] Checksum of Corrupted Buffer\n");
    uint32_t cksum_corrupted = string_checksum_unrolled(buffer);
    printf("Checksum of filled buffer: %u\n", cksum_corrupted);
    
    if (corrupted > 0) {
        printf("(Buffer was corrupted, so checksum may differ from expected)\n");
    }
    
    printf("\n=== Done ===\n");
    return 0;
}
