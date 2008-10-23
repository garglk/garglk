/* This is a Cfunctions (version 0.24) generated header file.
   Cfunctions is a free program for extracting headers from C files.
   Get Cfunctions from `http://www.hayamasa.demon.co.uk/cfunctions'. */

/* This file was generated with:
`cfunctions -i hash.c' */
#ifndef CFH_HASH_H
#define CFH_HASH_H

/* From `hash.c': */
typedef struct bucket {
    char *key;
    void *data;
    struct bucket *next;
} 
bucket;
typedef struct hash_table {
    size_t size;
    bucket **table;
} 
hash_table;
hash_table * n_hash_construct_table (hash_table *table , size_t size );
void * n_hash_insert (const char *key , void *data , hash_table *table );
void * n_hash_lookup (const char *key , hash_table *table );
void * n_hash_del (const char *key , hash_table *table );
void n_hash_free_table (hash_table *table , void ( *func ) ( void * ) );
void n_hash_enumerate (hash_table *table , void ( *func ) ( const char * , void * ) );

#endif /* CFH_HASH_H */
