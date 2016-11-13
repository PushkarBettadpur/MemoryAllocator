/*
 * The implementation provided is a segregated list. 
 * We have a array of linked lists (each element is a free block) and each index of the array contains a linked list of blocks of different sizes
 * The range of each element is a power of 2. ie, Elements in index 0 are 0-32 bytes, index 1 is 32-64, index 2 is 64-128 and so on.
 * We find the a block of an appropriate size and split it to find the best fit for our desired block (we place the remaining block in its appropriate free list index)
 * If we run out of memory, we request for more memory and immediately coalesce
 * When we free, we simply place the block on it's appropriate index and immediately coalesce again.
 * For realloc, we check if the size requestyed is smaller than the given size, if so, we simply return the old pointer. We also add an optimization where we check the next 
 * block to see if it's free and if the sizes are compatible, and if so, we merge the blocks and return that.
 * We also change the size we increase the heap by for realloc: Empirically found to give better results.

Size of global vars: sizeof(heap_listp) + MAX_LEVELS*(sizeof(free_lists)) + sizeof(EXPAND_SIZE) = 8 + 17*8 + 4 = 148 Bytes

Results:

Results for mm malloc:
trace  valid  util     ops      secs   Kops
 0       yes   97%    5694  0.000171  33318
 1       yes   98%    5848  0.000166  35229
 2       yes   97%    6648  0.000286  23269
 3       yes   98%    5380  0.000170  31628
 4       yes   99%   14400  0.000204  70727
 5       yes   93%    4800  0.000203  23669
 6       yes   91%    4800  0.000207  23144
 7       yes   53%   12000  0.000327  36753
 8       yes   47%   24000  0.000615  38999
 9       yes   65%   14401  0.000230  62559
10       yes   86%   14401  0.000185  77801
Total          84%  112372  0.002764  40660

Perf index = 50/60 (util) + 40/40 (thru) = 90/100

 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "PB",
    /* First member's full name */
    "Pushkar Bettadpur",
    /* First member's email address */
    "pushkar.bettadpur@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Set the depth to your segregated free list */
#define MAX_LEVELS 17
/* Used to calculate the index for a block of a given size */
/* Source http://stackoverflow.com/questions/3272424/compute-fast-log-base-2-ceiling */
#define FAST_LOG2(x) (sizeof(unsigned long)*8 - 1 - __builtin_clzl((unsigned long)(x)))
#define FAST_LOG2_UP(x) (((x) - (1 << FAST_LOG2(x))) ? FAST_LOG2(x) + 1 : FAST_LOG2(x))

/* The smallest permissible block */
#define MIN_BLOCK_SIZE 32

/* The pointer to the starting of the heap */
void* heap_listp = NULL;
/* The list of all free blocks arranged by size */
void* free_lists[MAX_LEVELS];
/* The amount we expand the heap by */
int EXPAND_SIZE = CHUNKSIZE;


/* Function Declarations */
void* mm_coalesce(void *bp);
int mm_check();
void* mm_extend_heap(size_t bytes);
void *malloc_with_no_split(size_t size);

/* 
 * Debug Functions
 * Designed because they are more useful to me in gdb
 */

/* Return The previous block */
void* returnPrevious(void* bp) {
    return PREV_BLKP(bp);
}

/* Return the next block */
void* returnNext(void* bp) {
    return NEXT_BLKP(bp);
}

/* Get the header of a block */
void* getHeader(void* bp) {
    return HDRP(bp);
}

/* Get the footer of a block */
void* getFooter(void* bp) {
    return FTRP(bp);
}

/* 
 * Unit tests which are useful for the
 * initial testing of the heap allocator
 */
void unit_tests()
{
/*
      mm_extend_heap(18, 5096);
      mm_extend_heap(18, 6000);
      mm_extend_heap(18, 4096);
      printf("ind are %d %d %d", index_of(64), index_of(1000), index_of(1024));
      char* bp1 = mm_malloc(10);
      char* bp2 = mm_malloc(10);
      char* bp3 = mm_malloc(10);
      mm_free(bp2);
      mm_free(bp1);
      mm_free(bp3);
      mm_extend_heap(1000);
      mm_extend_heap(16);
      mm_extend_heap(16);
      mm_extend_heap(24);
      char* bp4 = mm_malloc(10);
      char* bp5 = mm_malloc(500);
      char* bp6 = mm_malloc(40);
*/
}

/**********************************************************
 * indexOf
 * Given a size, return it's expected index
 **********************************************************/
int indexOf(size_t size)
{
    /* Minimum Size */
    if (size <= 32) {
        return 0;
    }

    /* Maximum Level Depth */
    if (size >= 1048576)
        return 16;

    /* To get the depth, get the ceiling of the log 
       and adjust to the free list */
    return FAST_LOG2_UP(size) - 5;
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {

    // Initialize the free list to empty
    int i = 0;
    for (; i < MAX_LEVELS; i++)
        free_lists[i] = NULL;

	if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
		return -1;

	PUT(heap_listp, 0);                         // alignment padding
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
	heap_listp += DSIZE;

    return 0;

 }

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void* mm_extend_heap(size_t words)
{
	char *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignments */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ( (bp = mem_sbrk(size)) == (void *)-1 )
		return NULL;

	/* Initialize free block header/footer and the epilogue header */
	PUT(HDRP(bp), PACK(size, 0));                // free block header
	PUT(FTRP(bp), PACK(size, 0));                // free block footer
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

	/* Coalesce if the previous block was free */
	return mm_coalesce(bp);
}

/**********************************************************
 * getBlockSize
 * Provide an easier interface to get the size of a block
 * Also can be called from GDB, which is useful for debugging
 **********************************************************/
int getBlockSize(void* bp)
{
    if (bp == NULL)
        return -1;

    return GET_SIZE(HDRP(bp));
}

/**********************************************************
 * addToFreeList
 * Adds a block (bp) to the free list (at level index)
 * The addition is simple, just put it at the head of the list
 **********************************************************/
void addToFreeList(void* bp, int index)
{
    char* oldHead = free_lists[index];

    if (oldHead)
        PUT((char*)oldHead+8, bp);

    // Place the block at the head of the list
    PUT((char*)bp, oldHead);
    PUT((char*)bp+8, 0);
    free_lists[index] = bp;
    return;
    
    /* I also tried to maintian lists in 
       Descending order of block size, but the hit to throughput
       Was too high. */
}

/**********************************************************
 * removeToFreeList
 * Removes a block (bp) from the free list (at level index)
 **********************************************************/
void removeFromFreeList(void* bp, int index)
{
    /* Get the blocks (if any) on either side */
    char* next = (char*)GET(bp);
    char* prev = (char*)GET((char*)bp+8);
    
    /* Perform the removal operation */
    if (!prev)
        free_lists[index] = next;
    else
        PUT((char*)prev, next);

    if (next)
        PUT((char*)next+8, prev);    
}

/**********************************************************
 * place
 * Provided a block, we check 'how much' of the block we 
 * actually need to allocate. If the provided block can actually
 * be split into two blocks, we split it and allocate the best
 * fit block, putting the other on the appropriate free list
 **********************************************************/
void place(void* bp, size_t size)
{
    /* Calculate the remaining fragment */
    int remainSize = getBlockSize(bp) - size;

    /* Remove the block from the free List */
    int index_to_remove = indexOf(getBlockSize(bp));
    removeFromFreeList(bp, index_to_remove);

    /* Check if the remaining fragment is atleast 
       The size of a 32 byte block (minimum supported) */
    if (remainSize >= MIN_BLOCK_SIZE)
    {
        /* Add Headers and Footers for block to be returned */
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));

        /* Convert Remaining into remaining size block */
        void* remaining = NEXT_BLKP(bp);
        int index = indexOf(remainSize);
        PUT(HDRP(remaining), PACK(remainSize, 0));
        PUT(FTRP(remaining), PACK(remainSize, 0));
        addToFreeList(remaining, index);     
    }
    /* Provide the full block itself */
    else
    {
        PUT(HDRP(bp), PACK(getBlockSize(bp), 1));
        PUT(FTRP(bp), PACK(getBlockSize(bp), 1));
    }
}

/**********************************************************
 * findFromFreeList
 * Finds a block of a certain size (arg size)
 * This function performs a first find, ie, returns the first
 * available block of the requisite size
 **********************************************************/
void* findFromFreeList(int level, size_t size)
{
    /* Check the current index for an
       appropriate block */
    char* curr = free_lists[level];
    while (curr)
    {
        if (getBlockSize(curr) >= size)
            return curr;
        
        curr = (char*)GET(curr);
    }

    /* Need to go through the higher levels */
    level++;
    while (level < MAX_LEVELS)
    {
        if (free_lists[level])
            return free_lists[level];

        level++;
    }

    /* Sadly, no we can't find an appropriate level */
    return NULL;
    
}

/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is first_find
 * However, the splitting performed in place(..) approximates
 * it more to a best (or better!) bit strategy
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    size_t adjsize;

    /* Adjust block size to include overhead and alignment reqs. */
    if ((size) <= DSIZE)
        adjsize = 2*DSIZE;
    else
        adjsize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    /* Find the minimum index we should find a block from */
    int index_to_add = indexOf(adjsize);
    /* Search the free list for a fit */
    char* bp = findFromFreeList(index_to_add, adjsize);

    if (bp == NULL) 
    {
        /* No fit found. Get more memory and place the block */
        if ((bp = mm_extend_heap(MAX(adjsize, EXPAND_SIZE) >> 3)) == NULL)
            return NULL;
        place(bp, adjsize);
    }
    else
        place(bp, adjsize);

    return bp;
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void* mm_coalesce(void *bp)
{
 	int prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
	int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = getBlockSize(bp);

    /* No Neighbors are free */
	if (prev_alloc && next_alloc)
    {
        /* Set the header and footer of block */
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));

        /* Get the index of free list we have
           to add ourselves in */
        int index = indexOf(size);
        addToFreeList(bp, index);        
        return bp;
    }

    /* Right side neighbor is free */
    else if (prev_alloc && !next_alloc) 
    { 
        /* Add the size of the right side ptr */
        size += getBlockSize(NEXT_BLKP(bp));

        /* Remove right side block from its list */
        removeFromFreeList(NEXT_BLKP(bp), indexOf(getBlockSize(NEXT_BLKP(bp))));

        /* Add new block to its list */
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
        int index = indexOf(size);   
        addToFreeList(bp, index);
        return bp;     
    }

    /* Left side neighbor is free */
    else if (!prev_alloc && next_alloc)
    {
        /* Add the left side ptr */
        size += getBlockSize(PREV_BLKP(bp));

        /* Remove left side block from list */
        removeFromFreeList(PREV_BLKP(bp), indexOf(getBlockSize(PREV_BLKP(bp))));

        /* Add new block to free list */
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        int index = indexOf(size);
        addToFreeList(PREV_BLKP(bp), index);
        return PREV_BLKP(bp);
    }
    
    /* Both sides are free */
    else
    {
        /* Add both sizes */
        size += getBlockSize(PREV_BLKP(bp)) + getBlockSize(NEXT_BLKP(bp));

        /* Remove both side blocks from free list */
        removeFromFreeList(PREV_BLKP(bp), indexOf(getBlockSize(PREV_BLKP(bp))));
        removeFromFreeList(NEXT_BLKP(bp), indexOf(getBlockSize(NEXT_BLKP(bp))));

        /* Add new block to free List */
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        int index = indexOf(size);
        addToFreeList(PREV_BLKP(bp), index);
        return PREV_BLKP(bp);
    }
}


/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    /* Ignore Spurious Requests */
    if (bp == NULL)
        return;

    int blkSize = getBlockSize(bp);
    
    /* Set headers and coalesce */
    PUT(HDRP(bp), PACK(blkSize, 0));
    PUT(FTRP(bp), PACK(blkSize, 0));
    PUT((char*)bp, 0);
    PUT((char*)bp+8, 0);

    mm_coalesce(bp);
}

/**********************************************************
 * mm_realloc
 * Implemented in terms of mm_malloc and mm_free.
 * A few cases have optimizations such as checking the 
 * next block for availability and using a malloc implementation
 * with no splitting
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{

    /* We found empirically that if we increase the amount we expand the heap
       by for realloc ops, the space utilization improves */
    EXPAND_SIZE = 1 << 15;

    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }
    
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
        return (mm_malloc(size));

    void *newptr;
    size_t oldSize, adjsize;

    /* Get the size of the block provided */
    oldSize = getBlockSize(ptr);

	/* We compare the new size provided to the 
       size of the original block */
	if ((size) <= DSIZE)
		adjsize = 2 * DSIZE;
	else
		adjsize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    
    /* We can return the current block itself if the old size > new size */
    if ((adjsize <= oldSize))
    {
        /* Surprisingly, providing an exact block degraded the space utiliztion % */
        //place(ptr, adjsize);
        return ptr;         
    }

    /* Commonly found optimization:
       Find the next block, if it's free and if the combined sizes of
       current block + next block is appropriate, then combine the two
       and use it */
	size_t nextBlockSize = getBlockSize(NEXT_BLKP(ptr));
    char* next = NEXT_BLKP(ptr);
	if(!(GET_ALLOC(HDRP(next))) && (adjsize <= (oldSize + nextBlockSize)))
	{
		removeFromFreeList(next, indexOf(nextBlockSize));
		PUT(HDRP(ptr), PACK(oldSize + nextBlockSize, 1));
		PUT(FTRP(ptr), PACK(oldSize + nextBlockSize, 1));
		return ptr;
	}
    /* Surprisingly, this optimization did not extend to checking the previous block */
   
    /* Allocate the new block.
       INTERESTINGLY: The performance utilization was much
       improved when the malloc at this stage did not split blocks
       but placed it directly in the first available block
    */

    newptr = malloc_with_no_split(size);
    if (!newptr)
        return NULL;

    /* Copy the old data. */
    memcpy(newptr, ptr, size);
    mm_free(ptr);
    return newptr;    
}

/**********************************************************
 * malloc_with_no_split
 * Same as previous malloc except no 'splitting' of blocks
 * allocated in first available block. Gives better performance
 * for realloc
 **********************************************************/
void *malloc_with_no_split(size_t size)
{
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    size_t adjsize;

    if ((size) <= DSIZE)
        adjsize = 2*DSIZE;
    else
        adjsize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    int index_to_add = indexOf(adjsize);
    char* bp = findFromFreeList(index_to_add, adjsize);
    if (bp == NULL) 
    {
        if ((bp = mm_extend_heap(MAX(adjsize, EXPAND_SIZE) >> 3)) == NULL)
            return NULL;
        place(bp, getBlockSize(bp));
    }
    else
        place(bp, getBlockSize(bp));

    return bp;
}

/**********************************************************
 * mm_check
 * Runs a number of diagnostic tests to check for the correctness
 * of the heap allocator
 **********************************************************/
int mm_check()
{
    /* 1. Check if all blocks in the free list are indeed, free */
    int i = 0;
    for (i = 0; i < MAX_LEVELS; i++)
    {
        char* curr = free_lists[i];
        while (curr)
        {
            if (GET_ALLOC(HDRP(curr)))
            {
                printf("Element in the free list is not free!\n");
                printf("Heap Check Failed\n");
                return 0;
            }
            curr = (char*)GET(curr);
        }
    }    

    /* 2. Check if any 2 adjacent blocks are free */
	void *bp;
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!(GET_ALLOC(HDRP(bp))) && !(GET_ALLOC(HDRP(NEXT_BLKP(bp)))))
        {
                printf("Consecutive elements seem to be free!\n");
                printf("Heap Check Failed\n");
                return 0;
        }
    }

    /* 3. Check if every free block is in it's expected list
       Note: Run this test on only the initial subset of traces
       Will take forever on traces like binary-bal */
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!(GET_ALLOC(HDRP(bp))))
        {
            int expectedIndex = indexOf(getBlockSize(bp));
            char* curr = free_lists[expectedIndex];
            while (curr)
            {
                if (curr == (char*)bp)
                    break;
                curr = (char*)GET(curr);            
            }
            if (!curr)
            {
                printf("A freed element isn't on its expected list\n");
                printf("Heap Check Failed\n");
                return 0;
            }
        }
    }

    /* All tests have passed successfully */
    return 1;    
}
