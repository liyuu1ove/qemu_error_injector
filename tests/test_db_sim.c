/*
 * test_db_sim.c - Database query corruption test
 * Simulates how ADD corruption affects index lookups and aggregations
 * 
 * Uses volatile + unrolling to ensure ADDs happen at runtime.
 * Compile: gcc -O0 -g -static -o test_db_sim test_db_sim.c
 * Find PC: objdump -d test_db_sim | grep '<fill_records>:' -A60
 *          objdump -d test_db_sim | grep '<sum_values>:' -A30
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define NUM_RECORDS 8

typedef struct {
    uint32_t id;
    uint32_t value;
} Record;

static Record table[NUM_RECORDS];

/* Pre-computed expected values */
static const uint32_t expected_ids[NUM_RECORDS] = {1, 2, 3, 4, 5, 6, 7, 8};
static const uint32_t expected_values[NUM_RECORDS] = {100, 200, 300, 400, 500, 600, 700, 800};
static const uint32_t expected_sum = 3600;

/*
 * TARGET FUNCTION 1: Fill database records using ADDs
 */
__attribute__((noinline))
void fill_records(void)
{
    volatile uint32_t base_id = 1;
    volatile uint32_t base_val = 100;
    
    table[0].id = base_id + 0;   table[0].value = base_val + 0;
    table[1].id = base_id + 1;   table[1].value = base_val + 100;
    table[2].id = base_id + 2;   table[2].value = base_val + 200;
    table[3].id = base_id + 3;   table[3].value = base_val + 300;
    table[4].id = base_id + 4;   table[4].value = base_val + 400;
    table[5].id = base_id + 5;   table[5].value = base_val + 500;
    table[6].id = base_id + 6;   table[6].value = base_val + 600;
    table[7].id = base_id + 7;   table[7].value = base_val + 700;
}

/*
 * TARGET FUNCTION 2: SUM aggregation (SELECT SUM(value) FROM table)
 */
__attribute__((noinline))
uint32_t sum_values(void)
{
    volatile uint32_t total = 0;
    total = total + table[0].value;
    total = total + table[1].value;
    total = total + table[2].value;
    total = total + table[3].value;
    total = total + table[4].value;
    total = total + table[5].value;
    total = total + table[6].value;
    total = total + table[7].value;
    return total;
}

int main(void)
{
    printf("=== Database Query Corruption Test ===\n\n");
    
    /* Populate table */
    fill_records();
    
    /* Check IDs */
    printf("[1] Record IDs:\n");
    int id_errs = 0;
    for (int i = 0; i < NUM_RECORDS; i++) {
        if (table[i].id != expected_ids[i]) {
            printf("  [%d] ID=%u (expected %u)  CORRUPT (diff=%+d)\n",
                   i, table[i].id, expected_ids[i], (int)table[i].id - (int)expected_ids[i]);
            id_errs++;
        } else {
            printf("  [%d] ID=%u  OK\n", i, table[i].id);
        }
    }
    
    /* Check values */
    printf("\n[2] Record Values:\n");
    int val_errs = 0;
    for (int i = 0; i < NUM_RECORDS; i++) {
        if (table[i].value != expected_values[i]) {
            printf("  [%d] Value=%u (expected %u)  CORRUPT (diff=%+d)\n",
                   i, table[i].value, expected_values[i], (int)table[i].value - (int)expected_values[i]);
            val_errs++;
        } else {
            printf("  [%d] Value=%u  OK\n", i, table[i].value);
        }
    }
    
    /* Check SUM */
    printf("\n[3] SUM Aggregation:\n");
    uint32_t actual_sum = sum_values();
    printf("  Expected: %u\n", expected_sum);
    printf("  Actual:   %u\n", actual_sum);
    if (actual_sum != expected_sum) {
        printf("  STATUS: CORRUPT (diff=%+d)\n", (int)actual_sum - (int)expected_sum);
    } else {
        printf("  STATUS: OK\n");
    }
    
    printf("\n=== Summary: %d ID errors, %d Value errors ===\n", id_errs, val_errs);
    return 0;
}

