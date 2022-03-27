/*
 * CS 208 Lab 5: Malloc Lab
 *
 * <Danel Chung and chungd>
 * <Victor Huang and huangv2>
 *
 * Simple allocator based on implicit free lists, first fit search,
 * and boundary tag coalescing.
 *
 * Each block has header and footer of the form:
 *
 *      63                  4  3  2  1  0
 *      ------------------------------------
 *     | s  s  s  s  ... s  s  0  0  0  a/f |
 *      ------------------------------------
 *
 * where s are the meaningful size bits and a/f is 1
 * if and only if the block is allocated. The list has the following form:
 *
 * begin                                                             end
 * heap                                                             heap
 *  -----------------------------------------------------------------
 * |  pad   | hdr(16:a) | ftr(16:a) | zero or more usr blks | hdr(0:a) |
 *  -----------------------------------------------------------------
 *          |       prologue        |                       | epilogue |
 *          |         block         |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/* Basic constants and macros */
#define WSIZE 8             /* word size (bytes) */
#define DSIZE 16            /* doubleword size (bytes) */
#define CHUNKSIZE (1 << 12) /* initial heap size (bytes) */
#define OVERHEAD 16         /* overhead of header and footer (bytes) */

/* NOTE: feel free to replace these macros with helper functions and/or
 * add new ones that will be useful for you. Just make sure you think
 * carefully about why these work the way they do
 */

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))

/* Perform unscaled pointer arithmetic */
#define PADD(p, val) ((char *)(p) + (val))
#define PSUB(p, val) ((char *)(p) - (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0xf)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) (PSUB(bp, WSIZE))
#define FTRP(bp) (PADD(bp, GET_SIZE(HDRP(bp)) - DSIZE))

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) (PADD(bp, GET_SIZE(HDRP(bp))))
#define PREV_BLKP(bp) (PSUB(bp, GET_SIZE((PSUB(bp, DSIZE)))))

/* Global variables */
team_t team = {.teamname = "chungdhuangv2",
               .name1 = "Daniel Chung",
               .id1 = "chungd",
               .name2 = "Victor Huang",
               .id2 = "huangv2"};

// Pointer to first block
static void *heap_start = NULL;

/* Function prototypes for internal helper routines */

static bool check_heap(int lineno);
static void print_heap();
static void print_block(void *bp);
static bool check_block(int lineno, void *bp);
static void *extend_heap(size_t size);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static size_t max(size_t x, size_t y);
static void *heap_end = NULL;
int counter = 0;

/*
 * mm_init -- <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
int mm_init(void)
{
    /* create the initial empty heap */
    if ((heap_start = mem_sbrk(4 * WSIZE)) == NULL)
        return -1;

    PUT(heap_start, 0);                               /* alignment padding */
    PUT(PADD(heap_start, WSIZE), PACK(OVERHEAD, 1));  /* prologue header */
    PUT(PADD(heap_start, DSIZE), PACK(OVERHEAD, 1));  /* prologue footer */
    PUT(PADD(heap_start, WSIZE + DSIZE), PACK(0, 1)); /* epilogue header */

    heap_start = PADD(heap_start, DSIZE); /* start the heap at the (size 0) payload of the prologue block */

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc -- <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
    {
        asize = DSIZE + OVERHEAD;
    }
    else
    {
        /* Add overhead and then round up to nearest multiple of double-word alignment */
        asize = DSIZE * ((size + (OVERHEAD) + (DSIZE - 1)) / DSIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = max(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

/*
 * mm_free -- <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
void mm_free(void *bp)
{  
    if (GET_ALLOC(HDRP(bp)))
    {
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 0 ));
        PUT(FTRP(bp), PACK(GET_SIZE(FTRP(bp)), 0 ));
        coalesce(bp);
    }
}

/* The remaining routines are internal helper routines */

/*
 * place -- Place block of asize bytes at start of free block bp
 *          and <How are you handling splitting?>
 * Takes a pointer to a free block and the size of block to place inside it
 * Returns nothing
 * <Are there any preconditions or postconditions?>
 */
static void place(void *bp, size_t asize)
{
    // TODO: improve this function

    // REPLACE THIS
    // currently does no splitting, just allocates the entire free block
     size_t sizecheck = GET_SIZE(HDRP(bp));
   if((sizecheck - asize) >= (2 * DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(sizecheck-asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(sizecheck-asize, 0));
   }
   else
   {
       PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
   }
   
}

/*
 * coalesce -- Boundary tag coalescing.
 * Takes a pointer to a free block
 * Return ptr to coalesced block
 * <Are there any preconditions or postconditions?>
 */
static void *coalesce(void *bp)
{
    size_t size_bp = GET_SIZE(HDRP(bp));
    size_t size_prev_bp = GET_SIZE(HDRP(PREV_BLKP(bp)));
    size_t size_next_bp = GET_SIZE(HDRP(NEXT_BLKP(bp)));
    if (!GET_ALLOC(HDRP(NEXT_BLKP(bp))) && !GET_ALLOC(HDRP(PREV_BLKP(bp))) && PREV_BLKP(bp) != bp)
    {
        size_bp += size_prev_bp + size_next_bp;
        bp = PREV_BLKP(bp);
        PUT( HDRP(bp), PACK(size_bp, 0));
        PUT( FTRP(bp), PACK(size_bp, 0));
    } else if (!GET_ALLOC(HDRP(PREV_BLKP(bp))) && PREV_BLKP(bp) != bp)
    {
        size_bp += size_prev_bp ;
        bp = PREV_BLKP(bp);
        PUT( HDRP(bp), PACK(size_bp , 0 ));
        PUT( FTRP(bp), PACK(size_bp, 0 ));
    } else if (!GET_ALLOC(HDRP(NEXT_BLKP(bp))))
    {
        size_bp += size_next_bp;
        PUT( HDRP(bp), PACK(size_bp, 0 ));
        PUT( FTRP(bp), PACK(size_bp, 0 ));
    }
    return bp;
}


/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize)
{
    /* search from the start of the free list to the end */
    for (char *cur_block = heap_start; GET_SIZE(HDRP(cur_block)) > 0; cur_block = NEXT_BLKP(cur_block))
    {
        if (!GET_ALLOC(HDRP(cur_block)) && (asize <= GET_SIZE(HDRP(cur_block))))
            return cur_block;
    }

    return NULL; /* no fit found */
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = words * WSIZE;
    if (words % 2 == 1)
        size += WSIZE;
    // printf("extending heap to %zu bytes\n", mem_heapsize());
    if ((long)(bp = mem_sbrk(size)) < 0)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */
    heap_end = NEXT_BLKP(bp);
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * check_heap -- Performs basic heap consistency checks for an implicit free list allocator
 * and prints out all blocks in the heap in memory order.
 * Checks include proper prologue and epilogue, alignment, and matching header and footer
 * Takes a line number (to give the output an identifying tag).
 */
static bool check_heap(int line)
{
    char *bp;

    if ((GET_SIZE(HDRP(heap_start)) != DSIZE) || !GET_ALLOC(HDRP(heap_start)))
    {
        printf("(check_heap at line %d) Error: bad prologue header\n", line);
        return false;
    }

    for (bp = heap_start; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!check_block(line, bp))
        {
            return false;
        }
    }

    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
    {
        printf("(check_heap at line %d) Error: bad epilogue header\n", line);
        return false;
    }

    return true;
}

/*
 * check_block -- Checks a block for alignment and matching header and footer
 */
static bool check_block(int line, void *bp)
{
    if ((size_t)bp % DSIZE)
    {
        printf("(check_heap at line %d) Error: %p is not double-word aligned\n", line, bp);
        return false;
    }
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
    {
        printf("(check_heap at line %d) Error: header does not match footer\n", line);
        return false;
    }
    return true;
}

/*
 * print_heap -- Prints out the current state of the implicit free list
 */
static void print_heap()
{
    char *bp;

    printf("Heap (%p):\n", heap_start);

    for (bp = heap_start; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        print_block(bp);
    }

    print_block(bp);
}

/*
 * print_block -- Prints out the current state of a block
 */
static void print_block(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0)
    {
        printf("%p: End of free list\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
           hsize, (halloc ? 'a' : 'f'),
           fsize, (falloc ? 'a' : 'f'));
}

/*
 * max: returns x if x > y, and y otherwise.
 */
static size_t max(size_t x, size_t y)
{
    return (x > y) ? x : y;
}


void* mm_realloc(void* ptr, size_t size)
{
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }
    size_t old_size = GET_SIZE(HDRP(ptr)); 
    size_t asize = size + DSIZE;
    if(size > 0)
    { 
        if(asize <= old_size)
        { 
            return ptr;
        }
        void *ptr2 = mm_malloc(asize);  
        place(ptr2, asize);
        for (int i =0; i <((int)old_size/8) - 2 ; i++)
        {
            PUT(PADD(ptr2,8*i), GET(PADD(ptr,8*i)));
        }
        mm_free(ptr); 
        return ptr2; 
    }
    return NULL;
}