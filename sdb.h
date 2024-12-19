/**
 * @file sdb.h
 * @brief Simple Database Library
 * @version 0.3.0
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

#define SDB_VERSION "0.3.0"
#define WINDOW_SIZE 1024
#define MIN_MATCH 3
#define POOL_BLOCK_SIZE 4096

/**
 * @brief Compression type for database storage
 */
typedef enum {
    SDB_COMPRESS_NONE,
    SDB_COMPRESS_RLE,
    SDB_COMPRESS_LZ77
} SDBCompressType;

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
 * @brief Compresses data using LZ77-style compression
 * 
 * @param data Input data to compress
 * @param data_len Length of input data
 * @param out_len Pointer to store compressed length
 * @return Compressed data buffer
 */
static unsigned char* lz77_compress(const unsigned char* data, size_t data_len, size_t* out_len) {
    unsigned char* compressed = (unsigned char*)malloc(data_len * 2);
    size_t comp_pos = 0;
    
    for (size_t i = 0; i < data_len;) {
        size_t best_len = 0;
        size_t best_offset = 0;
        
        // Search in sliding window
        size_t window_start = (i > WINDOW_SIZE) ? i - WINDOW_SIZE : 0;
        
        for (size_t j = window_start; j < i; j++) {
            size_t len = 0;
            while (i + len < data_len && 
                   j + len < i && 
                   data[i + len] == data[j + len] &&
                   len < 255) {
                len++;
            }
            
            if (len > best_len) {
                best_len = len;
                best_offset = i - j;
            }
        }
        
        if (best_len >= MIN_MATCH) {
            compressed[comp_pos++] = 1;  // Flag for match
            compressed[comp_pos++] = best_offset & 0xFF;
            compressed[comp_pos++] = (best_offset >> 8) & 0xFF;
            compressed[comp_pos++] = best_len;
            i += best_len;
        } else {
            compressed[comp_pos++] = 0;  // Flag for literal
            compressed[comp_pos++] = data[i++];
        }
    }
    
    *out_len = comp_pos;
    return realloc(compressed, comp_pos);
}

/**
 * @brief Decompresses LZ77 compressed data
 * 
 * @param compressed Compressed input data
 * @param comp_len Length of compressed data
 * @param out_len Pointer to store decompressed length
 * @return Decompressed data buffer
 */
static unsigned char* lz77_decompress(const unsigned char* compressed, size_t comp_len, size_t* out_len) {
    if (!compressed || comp_len == 0) return NULL;
    
    unsigned char* decompressed = (unsigned char*)malloc(comp_len * 2); // Initial size estimate
    size_t decom_pos = 0;
    size_t pos = 0;
    
    while (pos < comp_len) {
        if (compressed[pos] == 0) { // Literal
            decompressed[decom_pos++] = compressed[pos + 1];
            pos += 2;
        } else { // Match
            size_t offset = compressed[pos + 1] | (compressed[pos + 2] << 8);
            size_t length = compressed[pos + 3];
            
            for (size_t i = 0; i < length; i++) {
                decompressed[decom_pos] = decompressed[decom_pos - offset];
                decom_pos++;
            }
            pos += 4;
        }
    }
    
    *out_len = decom_pos;
    return realloc(decompressed, decom_pos);
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
 * @var capacity
 * @brief The capacity of the hash table
 * @var entries
 * @brief The entries array for hash table
 */
typedef struct {
    SDBEntry *head;
    SDBEntry *tail;
    size_t capacity;
    SDBEntry** entries;
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
    SDBCompressType compress_type;
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
SDB* sdb_open(const char* path, SDBCompressType compress_type) {
    SDB* sdb = (SDB*)malloc(sizeof(SDB));
    if (sdb == NULL) {
        return NULL;
    }
    
    sdb->path = strdup(path);
    sdb->tables = NULL;
    sdb->table_count = 0;
    sdb->compress_type = compress_type;

    FILE* file = fopen(path, "rb");
    if (file != NULL) {
        // Read compressed data
        size_t compressed_size, original_size;
        fread(&compressed_size, sizeof(size_t), 1, file);
        fread(&original_size, sizeof(size_t), 1, file);
        
        unsigned char* compressed = (unsigned char*)malloc(compressed_size);
        fread(compressed, 1, compressed_size, file);
        
        // Decompress data using the specified compression type
        size_t decompressed_size;
        unsigned char* buffer;
        if (compress_type == SDB_COMPRESS_RLE) {
            buffer = rle_decompress(compressed, compressed_size, &decompressed_size);
        } else {
            buffer = lz77_decompress(compressed, compressed_size, &decompressed_size);
        }
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
    unsigned char* compressed;
    
    if (sdb->compress_type == SDB_COMPRESS_RLE) {
        compressed = rle_compress(buffer, current_size, &compressed_size);
    } else {
        compressed = lz77_compress(buffer, current_size, &compressed_size);
    }
    
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
    
    // Initialize the new table
    SDBTable* table = &sdb->tables[sdb->table_count - 1];
    table->name = strdup(name);
    table->entries = (SDBEntryList*)malloc(sizeof(SDBEntryList));
    
    // Initialize hash table components
    table->entries->head = NULL;
    table->entries->tail = NULL;
    table->entries->capacity = 16;  // Initial capacity, can be adjusted
    table->entries->entries = (SDBEntry**)calloc(table->entries->capacity, sizeof(SDBEntry*));
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
            
            // Free entries
            SDBEntry* current = sdb->tables[i].entries->head;
            while (current != NULL) {
                SDBEntry* next = current->next;
                free(current->key);
                free(current->value);
                free(current);
                current = next;
            }
            
            // Free hash table array
            free(sdb->tables[i].entries->entries);
            free(sdb->tables[i].entries);
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

typedef struct {
    size_t capacity;
    size_t size;
    SDBEntry** entries;
} SDBHashTable;

static size_t hash_string(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
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
    if (!t) return;
    
    size_t hash = hash_string(key) % t->entries->capacity;
    SDBEntry* entry = t->entries->entries[hash];
    
    // Handle collision with linear probing
    while (entry && strcmp(entry->key, key) != 0) {
        hash = (hash + 1) % t->entries->capacity;
        entry = t->entries->entries[hash];
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

typedef struct MemoryBlock {
    unsigned char* data;
    size_t used;
    struct MemoryBlock* next;
} MemoryBlock;

typedef struct {
    MemoryBlock* current;
    size_t block_size;
} MemoryPool;

static void* pool_alloc(MemoryPool* pool, size_t size) {
    if (pool->current->used + size > pool->block_size) {
        MemoryBlock* new_block = malloc(sizeof(MemoryBlock));
        new_block->data = malloc(pool->block_size);
        new_block->used = 0;
        new_block->next = pool->current;
        pool->current = new_block;
    }
    
    void* ptr = pool->current->data + pool->current->used;
    pool->current->used += size;
    return ptr;
}

typedef struct {
    char* table;
    char* key;
    char* value;
} SDBOperation;

void sdb_batch_execute(SDB* sdb, SDBOperation* ops, size_t count) {
    for (size_t i = 0; i < count; i++) {
        sdb_table_set(sdb, ops[i].table, ops[i].key, ops[i].value);
    }
    // Single save at the end
    sdb_save(sdb);
}

#endif