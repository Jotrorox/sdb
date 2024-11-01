#include "../sdb.h"

int main() {
    SDB* sdb = sdb_open("test.sdb");
    if (sdb == NULL) {
        printf("Failed to open database\n");
        return 1;
    }

    sdb_table_create(sdb, "test");
    sdb_table_set(sdb, "test", "key", "value");
    sdb_table_set(sdb, "test", "key2", "value2");

    
    sdb_close(sdb);
    return 0;
}