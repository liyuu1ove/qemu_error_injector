/*
 * test_sqlite.c - Real SQLite database corruption test
 * 
 * Tests how ADD corruption affects actual database operations:
 * - INSERT operations
 * - SELECT queries
 * - SUM aggregation
 * - Data integrity
 *
 * Compile: gcc -O0 -g -o test_sqlite test_sqlite.c -lsqlite3
 * Or static: gcc -O0 -g -static -o test_sqlite test_sqlite.c -lsqlite3 -lpthread -ldl -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#define DB_FILE ":memory:"  /* In-memory database for testing */
#define NUM_RECORDS 10

/* Expected values for verification */
static const int expected_ids[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
static const int expected_values[] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
static const int expected_sum = 5500;  /* 100+200+...+1000 */

/* Global database handle */
static sqlite3 *db = NULL;

int init_database(void)
{
    int rc;
    char *err_msg = NULL;
    
    /* Open in-memory database */
    rc = sqlite3_open(DB_FILE, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    /* Create table */
    const char *create_sql = 
        "CREATE TABLE records ("
        "  id INTEGER PRIMARY KEY,"
        "  value INTEGER NOT NULL"
        ");";
    
    rc = sqlite3_exec(db, create_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }
    
    return 0;
}

/* Insert records - this is where ADD corruption can affect data */
int insert_records(void)
{
    int rc;
    char sql[256];
    char *err_msg = NULL;
    
    for (int i = 0; i < NUM_RECORDS; i++) {
        int id = i + 1;           /* ADD operation */
        int value = (i + 1) * 100; /* ADD and MUL operations */
        
        snprintf(sql, sizeof(sql), 
                 "INSERT INTO records (id, value) VALUES (%d, %d);",
                 id, value);
        
        rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "INSERT error: %s\n", err_msg);
            sqlite3_free(err_msg);
            return -1;
        }
    }
    
    return 0;
}

/* Query and verify individual records */
int verify_records(void)
{
    sqlite3_stmt *stmt;
    int rc;
    int errors = 0;
    
    const char *sql = "SELECT id, value FROM records ORDER BY id;";
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SELECT prepare error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    printf("[1] Record Verification (SELECT * FROM records):\n");
    printf("    ID    Expected    Got         Status\n");
    printf("    ---   --------    ---         ------\n");
    
    int row = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        int value = sqlite3_column_int(stmt, 1);
        
        int id_ok = (row < NUM_RECORDS && id == expected_ids[row]);
        int val_ok = (row < NUM_RECORDS && value == expected_values[row]);
        
        if (id_ok && val_ok) {
            printf("    [%d]   %4d        %4d        OK\n", 
                   id, expected_values[row], value);
        } else {
            printf("    [%d]   %4d        %4d        CORRUPT", 
                   id, row < NUM_RECORDS ? expected_values[row] : -1, value);
            if (!id_ok) printf(" (id diff=%+d)", id - expected_ids[row]);
            if (!val_ok && row < NUM_RECORDS) 
                printf(" (val diff=%+d)", value - expected_values[row]);
            printf("\n");
            errors++;
        }
        row++;
    }
    
    sqlite3_finalize(stmt);
    
    if (row != NUM_RECORDS) {
        printf("    WARNING: Expected %d rows, got %d\n", NUM_RECORDS, row);
        errors++;
    }
    
    return errors;
}

/* Test SUM aggregation */
int verify_sum(void)
{
    sqlite3_stmt *stmt;
    int rc;
    
    const char *sql = "SELECT SUM(value) FROM records;";
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SUM prepare error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    printf("\n[2] Aggregation Test (SELECT SUM(value) FROM records):\n");
    
    int sum = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sum = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    
    printf("    Expected SUM: %d\n", expected_sum);
    printf("    Actual SUM:   %d\n", sum);
    
    if (sum == expected_sum) {
        printf("    Status: OK\n");
        return 0;
    } else {
        printf("    Status: CORRUPT (diff=%+d)\n", sum - expected_sum);
        return 1;
    }
}

/* Test specific record lookup */
int verify_lookup(int target_id)
{
    sqlite3_stmt *stmt;
    int rc;
    
    const char *sql = "SELECT value FROM records WHERE id = ?;";
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Lookup prepare error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, target_id);
    
    printf("\n[3] Lookup Test (SELECT value FROM records WHERE id=%d):\n", target_id);
    
    int value = -1;
    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        value = sqlite3_column_int(stmt, 0);
        found = 1;
    }
    
    sqlite3_finalize(stmt);
    
    int expected = (target_id >= 1 && target_id <= NUM_RECORDS) 
                   ? expected_values[target_id - 1] : -1;
    
    printf("    Expected: %d\n", expected);
    printf("    Got:      %d\n", value);
    
    if (!found) {
        printf("    Status: CORRUPT (record not found!)\n");
        return 1;
    } else if (value == expected) {
        printf("    Status: OK\n");
        return 0;
    } else {
        printf("    Status: CORRUPT (diff=%+d)\n", value - expected);
        return 1;
    }
}

/* Test COUNT */
int verify_count(void)
{
    sqlite3_stmt *stmt;
    int rc;
    
    const char *sql = "SELECT COUNT(*) FROM records WHERE value > 500;";
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "COUNT prepare error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    printf("\n[4] Range Query (SELECT COUNT(*) FROM records WHERE value > 500):\n");
    
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    
    int expected_count = 5;  /* 600, 700, 800, 900, 1000 */
    
    printf("    Expected: %d\n", expected_count);
    printf("    Got:      %d\n", count);
    
    if (count == expected_count) {
        printf("    Status: OK\n");
        return 0;
    } else {
        printf("    Status: CORRUPT (diff=%+d)\n", count - expected_count);
        return 1;
    }
}

void cleanup(void)
{
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
}

int main(void)
{
    int errors = 0;
    
    printf("=== SQLite Database Corruption Test ===\n");
    printf("SQLite version: %s\n\n", sqlite3_libversion());
    
    /* Initialize */
    if (init_database() != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    /* Insert records */
    printf("Inserting %d records...\n\n", NUM_RECORDS);
    if (insert_records() != 0) {
        fprintf(stderr, "Failed to insert records\n");
        cleanup();
        return 1;
    }
    
    /* Run verification tests */
    errors += verify_records();
    errors += verify_sum();
    errors += verify_lookup(5);  /* Look up record with id=5 */
    errors += verify_count();
    
    /* Summary */
    printf("\n=== Summary ===\n");
    if (errors == 0) {
        printf("All tests passed - no corruption detected\n");
    } else {
        printf("CORRUPTION DETECTED: %d errors found\n", errors);
    }
    
    cleanup();
    return errors > 0 ? 1 : 0;
}
