/*
 * test_file_io_hex.c - File I/O corruption test with hex dump
 * Shows byte-level corruption in file data
 * 
 * This version uses unrolled assignments to isolate the ADD corruption
 * to ONLY the value calculation, not loop indices or addresses.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TEST_FILE "/tmp/corruption_test.dat"
#define DATA_SIZE 16

/*
 * Unrolled pattern fill using volatile to prevent compiler optimization.
 * The ADD between base and offset MUST happen at runtime.
 */
__attribute__((noinline))
void fill_pattern_unrolled(uint8_t *buf)
{
    volatile uint8_t base = 0x41;  /* 'A' - volatile prevents optimization */
    
    buf[0]  = base + 0;
    buf[1]  = base + 1;
    buf[2]  = base + 2;
    buf[3]  = base + 3;
    buf[4]  = base + 4;
    buf[5]  = base + 5;
    buf[6]  = base + 6;
    buf[7]  = base + 7;
    buf[8]  = base + 8;
    buf[9]  = base + 9;
    buf[10] = base + 10;
    buf[11] = base + 11;
    buf[12] = base + 12;
    buf[13] = base + 13;
    buf[14] = base + 14;
    buf[15] = base + 15;
}

/* Keep hex_dump OUTSIDE the corruption range */
__attribute__((noinline))
void hex_dump(const char *label, const uint8_t *data, size_t len)
{
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n           ");
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        printf(" %c ", (c >= 32 && c < 127) ? c : '.');
    }
    printf("\n");
}

int main(void)
{
    uint8_t write_buf[DATA_SIZE];
    uint8_t read_buf[DATA_SIZE];
    /* Pre-computed expected values - no ADD operations used */
    uint8_t expected[DATA_SIZE] = {
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50
    };  /* ABCDEFGHIJKLMNOP */
    FILE *fp;
    
    printf("=== File I/O Hex Dump Test (Unrolled) ===\n\n");
    
    /* Generate data - THIS is where corruption happens */
    fill_pattern_unrolled(write_buf);
    
    printf("Expected (clean):\n");
    hex_dump("  Bytes", expected, DATA_SIZE);
    
    printf("\nGenerated (possibly corrupted):\n");
    hex_dump("  Bytes", write_buf, DATA_SIZE);
    
    /* Write to file */
    fp = fopen(TEST_FILE, "wb");
    fwrite(write_buf, 1, DATA_SIZE, fp);
    fclose(fp);
    
    /* Read back */
    fp = fopen(TEST_FILE, "rb");
    fread(read_buf, 1, DATA_SIZE, fp);
    fclose(fp);
    
    printf("\nFrom file:\n");
    hex_dump("  Bytes", read_buf, DATA_SIZE);
    
    /* Show differences byte by byte */
    printf("\n=== Byte-by-Byte Comparison ===\n");
    printf("Index  Expected  Got       Status\n");
    printf("-----  --------  --------  ------\n");
    int diffs = 0;
    for (int i = 0; i < DATA_SIZE; i++) {
        if (expected[i] != write_buf[i]) {
            int diff = (int)write_buf[i] - (int)expected[i];
            printf(" [%2d]  0x%02X '%c'   0x%02X '%c'   CORRUPT (diff=%+d)\n",
                   i, expected[i], expected[i], 
                   write_buf[i], (write_buf[i] >= 32 && write_buf[i] < 127) ? write_buf[i] : '.',
                   diff);
            diffs++;
        } else {
            printf(" [%2d]  0x%02X '%c'   0x%02X '%c'   OK\n",
                   i, expected[i], expected[i], 
                   write_buf[i], write_buf[i]);
        }
    }
    
    remove(TEST_FILE);
    printf("\n=== Summary: %d of %d bytes corrupted ===\n", diffs, DATA_SIZE);
    return 0;
}
