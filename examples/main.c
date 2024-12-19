#include "../sdb.h"

int main() {
    SDB* sdb = sdb_open("test.sdb", SDB_COMPRESS_RLE);
    if (sdb == NULL) {
        printf("Failed to open database\n");
        return 1;
    }

    sdb_table_create(sdb, "test");
    sdb_table_set(sdb, "test", "key", "value");
    sdb_table_set(sdb, "test", "key2", "value2");

    printf("%s\n", sdb_table_get(sdb, "test", "key"));
    printf("%s\n", sdb_table_get(sdb, "test", "key2"));

    SDBInfo info = sdb_info(sdb);
    
    sdb_close(sdb);

    sdb = sdb_open("test.sdb", SDB_COMPRESS_RLE);
    printf("%s\n", sdb_table_get(sdb, "test", "key"));
    printf("%s\n", sdb_table_get(sdb, "test", "key2"));

    printf("Path: %s\n", info.path);
    printf("Version: %s\n", info.version);
    printf("Compress Type: %d\n", info.compress_type);

    sdb_close(sdb);
    sdb_free_info(info);
    
    return 0;
}