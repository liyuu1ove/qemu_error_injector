/*
 * test_file_io.c - File I/O corruption test
 * Demonstrates how ADD corruption affects data written to/read from files
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define TEST_FILE "/tmp/corruption_test.dat"
#define DATA_SIZE 32

/* Compute a simple checksum using ADDs */
uint32_t compute_checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum = sum + data[i];  /* ADD operation - may be corrupted */
    }
    return sum;
}

/* Fill buffer with pattern using ADDs */
void fill_pattern(uint8_t *buf, size_t len, uint8_t start)
{
    for (size_t i = 0; i < len; i++) {
        buf[i] = start + (uint8_t)i;  /* ADD operation - may be corrupted */
    }
}

int main(void)
{
    uint8_t write_buf[DATA_SIZE];
    uint8_t read_buf[DATA_SIZE];
    FILE *fp;
    
    printf("=== File I/O Corruption Test ===\n\n");
    
    /* Step 1: Generate test data with known pattern */
    printf("[1] Generating test data...\n");
    fill_pattern(write_buf, DATA_SIZE, 0x41);  /* Start with 'A' */
    
    printf("    Original data: ");
    for (int i = 0; i < DATA_SIZE; i++) {
        printf("%c", write_buf[i]);
    }
    printf("\n");
    
    uint32_t original_checksum = compute_checksum(write_buf, DATA_SIZE);
    printf("    Original checksum: 0x%08X\n\n", original_checksum);
    
    /* Step 2: Write to file */
    printf("[2] Writing to file: %s\n", TEST_FILE);
    fp = fopen(TEST_FILE, "wb");
    if (!fp) {
        perror("    Failed to open file for writing");
        return 1;
    }
    fwrite(write_buf, 1, DATA_SIZE, fp);
    fclose(fp);
    printf("    Written %d bytes\n\n", DATA_SIZE);
    
    /* Step 3: Read back from file */
    printf("[3] Reading from file...\n");
    fp = fopen(TEST_FILE, "rb");
    if (!fp) {
        perror("    Failed to open file for reading");
        return 1;
    }
    memset(read_buf, 0, DATA_SIZE);
    size_t bytes_read = fread(read_buf, 1, DATA_SIZE, fp);
    fclose(fp);
    printf("    Read %zu bytes\n\n", bytes_read);
    
    /* Step 4: Compare data */
    printf("[4] Comparing data...\n");
    printf("    Read data:     ");
    for (int i = 0; i < DATA_SIZE; i++) {
        printf("%c", read_buf[i]);
    }
    printf("\n");
    
    uint32_t read_checksum = compute_checksum(read_buf, DATA_SIZE);
    printf("    Read checksum: 0x%08X\n\n", read_checksum);
    
    /* Step 5: Check for corruption */
    printf("[5] Corruption Analysis:\n");
    int corrupted_bytes = 0;
    for (int i = 0; i < DATA_SIZE; i++) {
        if (write_buf[i] != read_buf[i]) {
            printf("    CORRUPT at offset %d: wrote 0x%02X '%c', read 0x%02X '%c'\n",
                   i, write_buf[i], write_buf[i], read_buf[i], read_buf[i]);
            corrupted_bytes++;
        }
    }
    
    if (corrupted_bytes == 0) {
        printf("    Data integrity: OK (no corruption in file data)\n");
    } else {
        printf("    Data integrity: FAILED (%d bytes corrupted)\n", corrupted_bytes);
    }
    
    if (original_checksum != read_checksum) {
        printf("    Checksum mismatch! Original=0x%08X, Read=0x%08X (diff=%d)\n",
               original_checksum, read_checksum, 
               (int)read_checksum - (int)original_checksum);
    } else {
        printf("    Checksums match: 0x%08X\n", original_checksum);
    }
    
    /* Cleanup */
    remove(TEST_FILE);
    
    printf("\n=== Test Complete ===\n");
    return corrupted_bytes > 0 ? 1 : 0;
}
