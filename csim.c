/*
 * CS 208 Lab 4: Cache Lab
 *
 * <Victor Huang, huangv2>
 *
 * csim.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions.  The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most one cache miss. 
 *  2. data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, an M operation can result in two cache hits, or a miss and a
 *  hit plus an possible eviction.
 *
 * The function printSummary() is given to print output.
 * Please use this function to print the number of hits, misses and evictions.
 * This is crucial for the driver to evaluate your work.
 */

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "cachelab.h"

#define MAX_LINE_LENGTH 1000

/* Type: Cache line
   lru_counter is a counter used to implement LRU replacement policy
   since we are just simluation cache hits/misses, we do not need to store the
   data blocks */
typedef struct {
    bool valid;
    unsigned tag;
    long lru_counter;
} cache_line_t;

/* a cache set is just an array of cache lines */
typedef struct {
    cache_line_t *lines;
} cache_set_t;

/* a cache is just an array of cache sets */
typedef struct {
    cache_set_t *sets;
} cache_t;

/* Allocate and initialize a cache of a given structure
 * s: number of bits in a set index
 * E: number of lines per set
 * b: number of block offset bits
 * returns a pointer to a cache
 */
cache_t *make_cache(int s, int E, int b) {
    cache_t* big_cache = malloc(sizeof(cache_t));
    big_cache->sets = malloc(sizeof(cache_set_t) * (1<<s));
    for(cache_set_t* i = big_cache->sets; i < big_cache->sets + (1<<s); ++i){
        i->lines = calloc(E, sizeof(cache_line_t));
    }
    return big_cache;
}

void free_cache(cache_t *cache, int s) {
    // this function should use free to deallocate all memory allocated by
    // make_cache
    for(cache_set_t* i = cache->sets; i < cache->sets + (1<<s); ++i){
        free(i->lines);
    }
    free(cache->sets);
    free(cache);
}

/* prints out each set of the cache and its contents; useful for debugging */
void print_cache(cache_t *cache, int s, int E) {
    int S = 1<<s;
    for (int i = 0; i < S; i++) {
        printf("set %d\n", i);
        for (int j = 0; j < E; j++) {
            printf("\t%d %d %ld\n", cache->sets[i].lines[j].valid, cache->sets[i].lines[j].tag, cache->sets[i].lines[j].lru_counter);
        }
    }
}

/* print a command-line usage message */
void print_usage(char *name) {
    fprintf(stderr, "Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", name);
}

int main(int argc, char *argv[])
{
    int s, E, b; // cache parameters
    bool vflag = false; // controls diagnostic printing
    char str[MAX_LINE_LENGTH]; // buffer to hold trace file input

    /* parse command line arguments, store them in the variables declared above */
    int opt;
    while ((opt = getopt(argc, argv, "hv::s:E:b:t:")) != -1) {
        switch (opt) {
        case 'v':
            vflag = true;
            break;
        case 'h':
            print_usage(argv[0]);
            exit(0);
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strncpy(str, optarg, MAX_LINE_LENGTH);
            break;
        default:
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    /* open the trace file */
    FILE * tfile = fopen(str, "r");
    if (tfile == NULL) {
        fprintf(stderr, "Could not open file %s\n", str);
        exit(EXIT_FAILURE);
    }

    /* allocate and initialize the cache */
    cache_t *cache = make_cache(s, E, b);

    /* variables to take cache behavior */
    int hit = 0;
    int miss = 0;
    int evict = 0;

    /* loop over each line in the trace file */
    while (fgets(str, MAX_LINE_LENGTH, tfile) != NULL) {

        /* parse the trace line into type of access, memory address, and size */
        char type; // either M(odify), L(oad), or S(tore)
        unsigned long addr; // 64-bit memory address
        if (sscanf(str, "%c %lx\n", &type, &addr) != 2) {
            fprintf(stderr, "error parsing trace line %s\n", str);
            exit(EXIT_FAILURE);
        }

        if (vflag)
            printf("%c %lx", type, addr);

        unsigned long index = ((1<<s)- 1) &(addr>>b);
        unsigned long c_tag = addr>>(s+b);

        bool has_hit = false;
        bool has_empty = false;
        
        catchfunc:
        for(cache_line_t *l = cache->sets[index].lines; l < cache->sets[index].lines+E; ++l){
        if(l->valid){
            if (c_tag == l->tag){
                hit++;
                if(vflag) puts(" hit");
                has_hit = true;
                break;
            }
        }
        }
        if(!has_hit){ 
            miss++;
            if(vflag) puts(" miss");
            for(cache_line_t *l = cache->sets[index].lines; l < cache->sets[index].lines+E; ++l){
                if (!l->valid){
                    has_empty = true;
                    l->tag = c_tag;
                    l->valid = true;
                    break;
                }
            }         
        }
        if (!has_empty && !has_hit){
            evict++;
            if(vflag) puts(" eviction");
            cache_line_t *oof = cache->sets[index].lines;
            oof->tag = c_tag;
            oof->valid = true;
        }
        if (type == 'M'){
            has_empty = false;
            has_hit = false;
            type = 'S';
            goto catchfunc;
        }
        
        // perform the cache lookup for memory address addr
        // increment hit, miss, and eviction as appropriate
        // L and S lookups are not different for the purposes of determining
        // hit/miss, and M is simply two consecutive accesses to the same address
    }

    fclose(tfile);
    free_cache(cache, s);
    printSummary(hit, miss, evict);
    return 0;
}
