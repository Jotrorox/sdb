#ifndef SDB_H
#define SDB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDB_VERSION "0.1.0"

typedef struct SDBEntry {
    char *key;
    char *value;
    struct SDBEntry *next;
} SDBEntry;

typedef struct {
    SDBEntry *head;
    SDBEntry *tail;
} SDBEntryList;

typedef struct {
    char *name;
    SDBEntryList *entries;
} SDBTable;

typedef struct {
    char *path;
    SDBTable *tables;
    int table_count;
} SDB;

SDB* sdb_open(const char* path) {
    SDB* sdb = (SDB*)malloc(sizeof(SDB));
    if (sdb == NULL) {
        return NULL;
    }
    
    sdb->path = strdup(path);
    sdb->tables = NULL;
    sdb->table_count = 0;

    // Try to open existing database file
    FILE* file = fopen(path, "rb");
    if (file != NULL) {
        // Read table count
        fread(&sdb->table_count, sizeof(int), 1, file);
        
        if (sdb->table_count > 0) {
            sdb->tables = (SDBTable*)malloc(sizeof(SDBTable) * sdb->table_count);
            
            // Read each table
            for (int i = 0; i < sdb->table_count; i++) {
                int name_len;
                fread(&name_len, sizeof(int), 1, file);
                
                sdb->tables[i].name = (char*)malloc(name_len + 1);
                fread(sdb->tables[i].name, 1, name_len, file);
                sdb->tables[i].name[name_len] = '\0';
                
                sdb->tables[i].entries = (SDBEntryList*)malloc(sizeof(SDBEntryList));
                sdb->tables[i].entries->head = NULL;
                sdb->tables[i].entries->tail = NULL;
                
                // Read entries
                int entry_count;
                fread(&entry_count, sizeof(int), 1, file);
                
                SDBEntry* current = NULL;
                for (int j = 0; j < entry_count; j++) {
                    SDBEntry* entry = (SDBEntry*)malloc(sizeof(SDBEntry));
                    
                    int key_len, value_len;
                    fread(&key_len, sizeof(int), 1, file);
                    fread(&value_len, sizeof(int), 1, file);
                    
                    entry->key = (char*)malloc(key_len + 1);
                    entry->value = (char*)malloc(value_len + 1);
                    
                    fread(entry->key, 1, key_len, file);
                    fread(entry->value, 1, value_len, file);
                    
                    entry->key[key_len] = '\0';
                    entry->value[value_len] = '\0';
                    entry->next = NULL;
                    
                    if (current == NULL) {
                        sdb->tables[i].entries->head = entry;
                    } else {
                        current->next = entry;
                    }
                    current = entry;
                }
                sdb->tables[i].entries->tail = current;
            }
        }
        fclose(file);
    }
    
    return sdb;
}

void sdb_close(SDB* sdb) {
    free(sdb);
}

void sdb_save(SDB* sdb) {
    FILE* file = fopen(sdb->path, "wb");
    if (file == NULL) {
        return;
    }

    // Write table count
    fwrite(&sdb->table_count, sizeof(int), 1, file);
    
    // Write each table
    for (int i = 0; i < sdb->table_count; i++) {
        // Write table name
        int name_len = strlen(sdb->tables[i].name);
        fwrite(&name_len, sizeof(int), 1, file);
        fwrite(sdb->tables[i].name, 1, name_len, file);
        
        // Count and write entries
        int entry_count = 0;
        SDBEntry* current = sdb->tables[i].entries->head;
        while (current != NULL) {
            entry_count++;
            current = current->next;
        }
        
        fwrite(&entry_count, sizeof(int), 1, file);
        
        // Write each entry
        current = sdb->tables[i].entries->head;
        while (current != NULL) {
            int key_len = strlen(current->key);
            int value_len = strlen(current->value);
            
            fwrite(&key_len, sizeof(int), 1, file);
            fwrite(&value_len, sizeof(int), 1, file);
            fwrite(current->key, 1, key_len, file);
            fwrite(current->value, 1, value_len, file);
            
            current = current->next;
        }
    }
    
    fclose(file);
}

void sdb_table_create(SDB* sdb, const char* name) {
    sdb->table_count++;
    sdb->tables = (SDBTable*)realloc(sdb->tables, sizeof(SDBTable) * sdb->table_count);
    sdb->tables[sdb->table_count - 1].name = strdup(name);
    sdb->tables[sdb->table_count - 1].entries = (SDBEntryList*)malloc(sizeof(SDBEntryList));
    sdb->tables[sdb->table_count - 1].entries->head = NULL;
    sdb->tables[sdb->table_count - 1].entries->tail = NULL;
}

void sdb_table_destroy(SDB* sdb, const char* name) {
    for (int i = 0; i < sdb->table_count; i++) {
        if (strcmp(sdb->tables[i].name, name) == 0) {
            free(sdb->tables[i].name);
            SDBEntry* current = sdb->tables[i].entries->head;
            while (current != NULL) {
                SDBEntry* next = current->next;
                free(current->key);
                free(current->value);
                free(current);
                current = next;
            }
        }
    }
}

SDBTable* sdb_table_find(SDB* sdb, const char* name) {
    for (int i = 0; i < sdb->table_count; i++) {
        if (strcmp(sdb->tables[i].name, name) == 0) {
            return &sdb->tables[i];
        }
    }
    return NULL;
}

void sdb_table_set(SDB* sdb, const char* table, const char* key, const char* value) {
    SDBTable* t = sdb_table_find(sdb, table);
    if (t == NULL) {
        return;
    }

    SDBEntry* e = (SDBEntry*)malloc(sizeof(SDBEntry));
    e->key = strdup(key);
    e->value = strdup(value);
    e->next = NULL;

    if (t->entries->head == NULL) {
        t->entries->head = e;
    } else {
        t->entries->tail->next = e;
    }
    t->entries->tail = e;

    sdb_save(sdb);
}

#endif