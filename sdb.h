/**
 * @file sdb.h
 * @brief Simple Database Library
 * @version 0.2.0
 * 
 * This is a simple database library that is used to store data in a file.
 * It is not meant to be a full-featured database, but rather a simple way to store data.
 * 
 * @author Johannes (Jotrorox) Müller
 * @copyright Copyright (c) 2024
 */

#ifndef SDB_H
#define SDB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDB_VERSION "0.2.0"

/**
 * @brief Compresses data using run-length encoding
 * 
 * @param data Input data to compress
 * @param data_len Length of input data
 * @param out_len Pointer to store compressed length
 * @return Compressed data buffer
 */
static unsigned char* rle_compress(const unsigned char* data, size_t data_len, size_t* out_len) {
    if (!data || data_len == 0) return NULL;
    
    // Allocate worst case size (input size * 2)
    unsigned char* compressed = (unsigned char*)malloc(data_len * 2);
    if (!compressed) return NULL;
    
    size_t comp_pos = 0;
    size_t pos = 0;
    
    while (pos < data_len) {
        unsigned char count = 1;
        unsigned char current = data[pos];
        
        while (pos + count < data_len && 
               data[pos + count] == current && 
               count < 255) {
            count++;
        }
        
        compressed[comp_pos++] = count;
        compressed[comp_pos++] = current;
        pos += count;
    }
    
    *out_len = comp_pos;
    return realloc(compressed, comp_pos); // Shrink to actual size
}

/**
 * @brief Decompresses RLE compressed data
 * 
 * @param compressed Compressed input data
 * @param comp_len Length of compressed data
 * @param out_len Pointer to store decompressed length
 * @return Decompressed data buffer
 */
static unsigned char* rle_decompress(const unsigned char* compressed, size_t comp_len, size_t* out_len) {
    if (!compressed || comp_len == 0) return NULL;
    
    // First pass: calculate decompressed size
    size_t decom_size = 0;
    for (size_t i = 0; i < comp_len; i += 2) {
        decom_size += compressed[i];
    }
    
    unsigned char* decompressed = (unsigned char*)malloc(decom_size);
    if (!decompressed) return NULL;
    
    size_t pos = 0;
    for (size_t i = 0; i < comp_len; i += 2) {
        unsigned char count = compressed[i];
        unsigned char value = compressed[i + 1];
        
        for (unsigned char j = 0; j < count; j++) {
            decompressed[pos++] = value;
        }
    }
    
    *out_len = decom_size;
    return decompressed;
}

/**
 * @struct SDBEntry
 * @brief A single entry in the database
 * 
 * @var key
 * @brief The key of the entry
 * @var value
 * @brief The value of the entry
 * @var next
 * @brief The next entry in the list
 */
typedef struct SDBEntry {
    char *key;
    char *value;
    struct SDBEntry *next;
} SDBEntry;

/**
 * @struct SDBEntryList
 * @brief A list of entries
 * 
 * @var head
 * @brief The head of the list
 * @var tail
 * @brief The tail of the list
 */
typedef struct {
    SDBEntry *head;
    SDBEntry *tail;
} SDBEntryList;

/**
 * @struct SDBTable
 * @brief A table in the database
 * 
 * @var name
 * @brief The name of the table
 * @var entries
 * @brief The list of entries in the table
 */
typedef struct {
    char *name;
    SDBEntryList *entries;
} SDBTable;

/**
 * @struct SDB
 * @brief The database
 * 
 * @var path
 * @brief The path to the database file
 * @var tables
 * @brief The list of tables in the database
 * @var table_count
 * @brief The number of tables in the database
 */
typedef struct {
    char *path;
    SDBTable *tables;
    int table_count;
} SDB;

/**
 * @brief Helper function to write data to a buffer with automatic resizing
 * 
 * @param buffer Pointer to buffer pointer
 * @param buffer_size Pointer to current buffer size
 * @param current_size Pointer to current data size
 * @param data Data to write
 * @param size Size of data to write
 */
static void write_to_buffer(unsigned char** buffer, size_t* buffer_size, 
                          size_t* current_size, const void* data, size_t size) {
    // Check if we need to resize
    while (*current_size + size > *buffer_size) {
        *buffer_size *= 2;
        *buffer = (unsigned char*)realloc(*buffer, *buffer_size);
    }
    
    // Copy data to buffer
    memcpy(*buffer + *current_size, data, size);
    *current_size += size;
}

/**
 * @brief Opens a database file
 * 
 * @param path The path to the database file
 * @return The database
 */
SDB* sdb_open(const char* path) {
    SDB* sdb = (SDB*)malloc(sizeof(SDB));
    if (sdb == NULL) {
        return NULL;
    }
    
    sdb->path = strdup(path);
    sdb->tables = NULL;
    sdb->table_count = 0;

    FILE* file = fopen(path, "rb");
    if (file != NULL) {
        // Read compressed data
        size_t compressed_size, original_size;
        fread(&compressed_size, sizeof(size_t), 1, file);
        fread(&original_size, sizeof(size_t), 1, file);
        
        unsigned char* compressed = (unsigned char*)malloc(compressed_size);
        fread(compressed, 1, compressed_size, file);
        
        // Decompress data
        size_t decompressed_size;
        unsigned char* buffer = rle_decompress(compressed, compressed_size, &decompressed_size);
        free(compressed);
        
        if (buffer && decompressed_size == original_size) {
            size_t pos = 0;
            
            // Read table count
            memcpy(&sdb->table_count, buffer + pos, sizeof(int));
            pos += sizeof(int);
            
            if (sdb->table_count > 0) {
                sdb->tables = (SDBTable*)malloc(sizeof(SDBTable) * sdb->table_count);
                
                // Read each table
                for (int i = 0; i < sdb->table_count; i++) {
                    int name_len;
                    memcpy(&name_len, buffer + pos, sizeof(int));
                    pos += sizeof(int);
                    
                    sdb->tables[i].name = (char*)malloc(name_len + 1);
                    memcpy(sdb->tables[i].name, buffer + pos, name_len);
                    pos += name_len;
                    sdb->tables[i].name[name_len] = '\0';
                    
                    sdb->tables[i].entries = (SDBEntryList*)malloc(sizeof(SDBEntryList));
                    sdb->tables[i].entries->head = NULL;
                    sdb->tables[i].entries->tail = NULL;
                    
                    // Read entries
                    int entry_count;
                    memcpy(&entry_count, buffer + pos, sizeof(int));
                    pos += sizeof(int);
                    
                    SDBEntry* current = NULL;
                    for (int j = 0; j < entry_count; j++) {
                        SDBEntry* entry = (SDBEntry*)malloc(sizeof(SDBEntry));
                        
                        int key_len, value_len;
                        memcpy(&key_len, buffer + pos, sizeof(int));
                        pos += sizeof(int);
                        memcpy(&value_len, buffer + pos, sizeof(int));
                        pos += sizeof(int);
                        
                        entry->key = (char*)malloc(key_len + 1);
                        entry->value = (char*)malloc(value_len + 1);
                        
                        memcpy(entry->key, buffer + pos, key_len);
                        pos += key_len;
                        memcpy(entry->value, buffer + pos, value_len);
                        pos += value_len;
                        
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
            free(buffer);
        }
        fclose(file);
    }
    
    return sdb;
}

/**
 * @brief Closes the database
 * 
 * @param sdb The database
 */
void sdb_close(SDB* sdb) {
    if (!sdb) return;

    // Free all tables and their entries
    for (int i = 0; i < sdb->table_count; i++) {
        // Free all entries in the table
        SDBEntry* current = sdb->tables[i].entries->head;
        while (current != NULL) {
            SDBEntry* next = current->next;
            free(current->key);
            free(current->value);
            free(current);
            current = next;
        }
        
        // Free table structure
        free(sdb->tables[i].name);
        free(sdb->tables[i].entries);
    }

    // Free tables array
    free(sdb->tables);
    
    // Free path
    free(sdb->path);
    
    // Finally free the SDB structure
    free(sdb);
}

/**
 * @brief Saves the database
 * 
 * @param sdb The database
 */
void sdb_save(SDB* sdb) {
    FILE* file = fopen(sdb->path, "wb");
    if (file == NULL) {
        return;
    }

    size_t buffer_size = 1024;  // Initial size
    size_t current_size = 0;
    unsigned char* buffer = (unsigned char*)malloc(buffer_size);
    
    // Write table count
    write_to_buffer(&buffer, &buffer_size, &current_size, 
                   &sdb->table_count, sizeof(int));
    
    // Write each table
    for (int i = 0; i < sdb->table_count; i++) {
        // Write table name
        int name_len = strlen(sdb->tables[i].name);
        write_to_buffer(&buffer, &buffer_size, &current_size, 
                       &name_len, sizeof(int));
        write_to_buffer(&buffer, &buffer_size, &current_size, 
                       sdb->tables[i].name, name_len);
        
        // Count and write entries
        int entry_count = 0;
        SDBEntry* current = sdb->tables[i].entries->head;
        while (current != NULL) {
            entry_count++;
            current = current->next;
        }
        
        write_to_buffer(&buffer, &buffer_size, &current_size, 
                       &entry_count, sizeof(int));
        
        // Write each entry
        current = sdb->tables[i].entries->head;
        while (current != NULL) {
            int key_len = strlen(current->key);
            int value_len = strlen(current->value);
            
            write_to_buffer(&buffer, &buffer_size, &current_size, 
                           &key_len, sizeof(int));
            write_to_buffer(&buffer, &buffer_size, &current_size, 
                           &value_len, sizeof(int));
            write_to_buffer(&buffer, &buffer_size, &current_size, 
                           current->key, key_len);
            write_to_buffer(&buffer, &buffer_size, &current_size, 
                           current->value, value_len);
            
            current = current->next;
        }
    }

    // Compress the buffer
    size_t compressed_size;
    unsigned char* compressed = rle_compress(buffer, current_size, &compressed_size);
    
    // Write compressed size followed by compressed data
    fwrite(&compressed_size, sizeof(size_t), 1, file);
    fwrite(&current_size, sizeof(size_t), 1, file);  // Original size
    fwrite(compressed, 1, compressed_size, file);
    
    free(buffer);
    free(compressed);
    fclose(file);
}

/**
 * @brief Creates a table in the database
 * 
 * @param sdb The database
 * @param name The name of the table
 */
void sdb_table_create(SDB* sdb, const char* name) {
    sdb->table_count++;
    sdb->tables = (SDBTable*)realloc(sdb->tables, sizeof(SDBTable) * sdb->table_count);
    sdb->tables[sdb->table_count - 1].name = strdup(name);
    sdb->tables[sdb->table_count - 1].entries = (SDBEntryList*)malloc(sizeof(SDBEntryList));
    sdb->tables[sdb->table_count - 1].entries->head = NULL;
    sdb->tables[sdb->table_count - 1].entries->tail = NULL;
}

/**
 * @brief Destroys a table in the database
 * 
 * @param sdb The database
 * @param name The name of the table
 */
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

/**
 * @brief Finds a table in the database
 * 
 * @param sdb The database
 * @param name The name of the table
 * @return The table
 */
SDBTable* sdb_table_find(SDB* sdb, const char* name) {
    for (int i = 0; i < sdb->table_count; i++) {
        if (strcmp(sdb->tables[i].name, name) == 0) {
            return &sdb->tables[i];
        }
    }
    return NULL;
}

/**
 * @brief Sets a value in the database
 * 
 * @param sdb The database
 * @param table The name of the table
 * @param key The key
 * @param value The value
 */
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

/**
 * @brief Gets a value from the database
 * 
 * @param sdb The database
 * @param table The name of the table
 * @param key The key
 * @return The value
 */
char* sdb_table_get(SDB* sdb, const char* table, const char* key) {
    SDBTable* t = sdb_table_find(sdb, table);
    if (t == NULL) {
        return NULL;
    }

    SDBEntry* e = t->entries->head;
    while (e != NULL) {
        if (strcmp(e->key, key) == 0) {
            return e->value;
        }
        e = e->next;
    }
    return NULL;
}

#endif