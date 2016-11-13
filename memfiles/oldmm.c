/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
    "AP",
    /* First member's full name */
    "Pushkar Bettadpur",
    /* First member's email address */
    "pushkar.bettadpur@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Asna Shafiq",
    /* Second member's email address (leave blank if none) */
    "asna.shafiq@mail.utoronto.ca"
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
//#define PUTP(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

void* heap_listp = NULL;

/*
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
                MY IMPLEMENTATION
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
*/
/*
#define MAX_LEVELS 12
#define HIGHEST_POWER 16 //diff must be 4?
#define STARTING_POWER 14
#define STARTING_INDEX 2
#define EXPAND_POWER 13
#define EXPAND_INDEX 3
#define META_POWER 15
*/
#define HIGHEST_POWER 30//22 //diff must be 4?
#define STARTING_POWER 18//22
#define STARTING_INDEX (HIGHEST_POWER - STARTING_POWER)//0
#define EXPAND_POWER 18
#define EXPAND_INDEX (HIGHEST_POWER - EXPAND_POWER)
#define MAX_LEVELS (HIGHEST_POWER - 4)//18
#define META_POWER 15
#define MAX_SIZE (1 << HIGHEST_POWER)

#define OFFSET 16
#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))
#define MY_LEVEL(bp) (GET(bp) >> 1)
void* free_lists[MAX_LEVELS];
void* metadata = NULL;
void mm_coalesce(void *bp, int level);

void unitTests()
{
    // Say we allocate 4096 blocks
    // Malloc for a 64 byte block
    //              4096
    //          2048    2048
    //      1024   1024
    //   512    512
    //256   256
 //128   128
//64  64

    char* bp = mm_malloc(40);
    mm_free(bp);
    

}

/**********************************************************
 * my_init
 **********************************************************/
int mm_init(void)
 {

    // Request, around 4GB (29 levels: 0th level = 4G and 28th level = 16 Bytes or 2 Words)
    size_t metadata_size = 0;//1 << META_POWER;
    size_t initial_size = (1 << STARTING_POWER) + metadata_size;

    if ((metadata = mem_sbrk(initial_size)) == (void *)-1)
         return -1;
    
    heap_listp = (char*)metadata + metadata_size;
    // Initialize the free lists
    int i = 0;
    for (i = 0; i < MAX_LEVELS; i++)
    {
        free_lists[i] = NULL;
    }

    PUT(heap_listp, PACK(STARTING_INDEX << 1, 0)); // Set allocated and level to NULL
    PUT((char*)heap_listp+8, 0); // Set next to NULL
    PUT((char*)heap_listp+16, 0); // Set prev to NULL
    free_lists[STARTING_INDEX] = heap_listp; // Add it to free_list at level 0
   // unitTests();
    return 0;

 }

/**********************************************************
 * my_extend_heap
 **********************************************************/
void mm_extend_heap(int level, size_t bytes)
{
  //  count++;
 //   printf("reallocing count: %d\n", count);
    char* bp;
    int t_level = EXPAND_INDEX;
    size_t c_size = 1<<EXPAND_POWER;
    if (bytes > c_size)
    {
        t_level = level;
        c_size = bytes;
    }
    

    if ((bp = mem_sbrk(c_size)) == (void *)-1)
    {
        /*
         printf("\n******Damn, extend failed, time for %zu\n",bytes);
         if ((bp = mem_sbrk(bytes)) == (void *)-1)
            return;
         else
            t_level = level;
        */
        return;
    }

    char* oldHead = free_lists[t_level];
    free_lists[t_level] = bp;
    PUT(bp, PACK(t_level<<1,0));
    PUT((char*)bp+8, oldHead);
    PUT((char*)bp+16, 0); // Because placing at head of queue
    if (oldHead)
        PUT((char*)oldHead+16, bp);
    //mm_coalesce(bp, t_level);
}

/**********************************************************
 * my_findBlock
 **********************************************************/
void *mm_findBlock(int level, size_t size)
{
    //Check if level has a free block
    if (free_lists[level] != NULL)
    {
        char* newPtr = free_lists[level];
        char* next = (uintptr_t*)GET(newPtr+8);
        if (next)
            PUT(next+16, 0);
        free_lists[level] = next;
        // Set level and allocated=1
        PUT(newPtr, ((level<<1) | 1));
        // Return ptr to after header
        return newPtr+OFFSET;
    }

    int index = level;
    size_t levelSize = size;
    char* newPtr = NULL;

    // Basic Premise: Keep searching through levels until
    // you hit a free block of a desired size
    while (free_lists[level] == NULL)
    {
        // Can't go a lower level. ie. we've run out of block sizes
        if (index < 0)
            return NULL;

        // If current level is NULL, we need to do this dance again
        // Reduce the level index by one and update the level size
        if (free_lists[index] == NULL)
        {
            index -= 1;
            levelSize = levelSize << 1;
            continue;
        }

        // So we've found out an appropriate free block, and now, we must do the following
        // a. Get the free block off the list at the current level
        // b. Split it into two and put it on the lower level
        // c. Repeat until you get one of the desired size.
        else
        {
            // Size of split will be half of current level size
            levelSize = levelSize >> 1;
            // Get the head off the curr level and update head
            newPtr = free_lists[index];
            char* next = (char*)GET(newPtr+8);
            if (next)
                PUT(next+16, 0);
            free_lists[index] = next;
            // Get the next pointer for below level
            char* half = newPtr + levelSize;
            // Next of the 2nd half is NULL
            PUT(half+16, newPtr);
            PUT(half+8, 0);
            PUT(half, PACK(levelSize, 0));
            // Next of this block is 2nd half
            PUT(newPtr+16, 0);
            PUT(newPtr+8, (uintptr_t*)half);
            PUT(newPtr, PACK(levelSize, 0));
            // Update head of prev level and update index
            free_lists[index+1] = newPtr;
            index += 1;
        }
    }

    // Now that we've found out block, put the level in it and return
    // Why are we inserting level and not size?
    // Makes it easier in free as we can just dump it in the approp free list
    newPtr = free_lists[level];
    char* next = (char*)GET(newPtr+8);
    if (next)
        PUT(next+16, 0);
    free_lists[level] = next;
    // Set level and allocated=1
    PUT(newPtr, ((level<<1) | 1));
    // Return ptr to after header
    return newPtr+OFFSET;
}

/**********************************************************
 * my_malloc
 **********************************************************/
void *mm_malloc(size_t size)
{

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    // Adjust block size for overhead (1Byte for Size+Alloc) and Alignment
    // leading_zeros = __builtin_clzl(size+8);
    size_t adjsize = size+OFFSET;

    // Find the ceiling log2
    // Using int __builtin_clzl (unsigned long)
    // http://stackoverflow.com/questions/3272424/compute-fast-log-base-2-ceiling
    // http://stackoverflow.com/questions/11376288/fast-computing-of-log2-for-64-bit-integers
    int level = LOG2(adjsize);
    if (adjsize > (1<<level)) 
        level++;
    
    adjsize = 1<<level;
    if (adjsize > MAX_SIZE)
        printf ("HMM. Size error %zu from max %zu\n", adjsize, MAX_SIZE);
    // Calculate the level index based on level
    // RIGHT NOW ASSUME NO CALLS > 4GB
    level = HIGHEST_POWER - level;     

    // Find the block at the specified level size    
    char* bp;
    if ((bp = mm_findBlock(level, adjsize)) != NULL)
        return bp;

 //   else {
 //       printf("Returning Null for %zu\n", adjsize);
 //       return NULL;
 //   }
    // Extend the heap?
    mm_extend_heap(level, adjsize);
    bp = mm_findBlock(level, adjsize);
    if (bp == NULL)
        printf("Returning Null for %zu real and %zu adj\n",size, adjsize);
    return bp;
}

/**********************************************************
 * coalesce
 **********************************************************/
void mm_coalesce(void *bp, int level)
{
    // Find index in level of block
    // Find index of buddy and thereby the address
    // Check if it's allocated
    // If so, merge and recurse, else return
    
    // Max size is 4GB? That is
    while (1)
    {
        if (level == 0)
            return;

        int bits_to_shift = HIGHEST_POWER - level;
        int index_in_level = ((char*)bp - (char*)heap_listp) >> bits_to_shift;
        size_t levelSize = 1 << bits_to_shift;
        
        // if index in level is even
        if ((index_in_level & 1) == 0)
        {
            // Get buddy at index i+1; 
            char* buddy = (char*)bp + levelSize;
            // Search through free list at level given to see if buddy is also free
            if (GET_ALLOC(buddy))
                return;
            if (MY_LEVEL(buddy) != level)
                return;

            // For bp, find prev in linked l and next in ll and link em up
            char* next_nbp = (char*)GET(bp+8); // next block
            char* prev_nbp = (char*)GET(bp+16); // prev block
            if (!prev_nbp)
                free_lists[level] = next_nbp;
            else
                PUT(prev_nbp+8, next_nbp);
            if (next_nbp)
                PUT((char*)next_nbp+16, prev_nbp);

            // Do same for buddy
            char* next_buddy = (char*)GET(buddy+8); // next block
            char* prev_buddy = (char*)GET(buddy+16); // prev block
            if (!prev_buddy)
                free_lists[level] = next_buddy;
            else
                PUT(prev_buddy+8, next_buddy);
            if (next_buddy)
                PUT((char*)next_buddy+16, prev_buddy);

            // Placed coalesced block on next level
            // Clear buddy blocks 
            PUT((char*)buddy, 0);
            PUT((char*)buddy+8, 0);
            PUT((char*)buddy+16, 0);

            char* upper_block = free_lists[level - 1];
            if (upper_block)
                PUT((char*)upper_block+16, bp);
            free_lists[level - 1] = bp;
            level = level - 1;
            PUT((char*)bp, PACK(level << 1, 0));
            PUT((char*)bp+8, upper_block);
            PUT((char*)bp+16, 0);   
        }

        else
        {
            // Get buddy at index i-1; 
            char* buddy = (char*)bp - levelSize;
            // Search through free list at level given to see if buddy is also free
            if (GET_ALLOC(buddy))
                return;
            if (MY_LEVEL(buddy) != level)
                return;

            // For bp, find prev in linked l and next in ll and link em up
            char* next_nbp = (char*)GET(bp+8); // next block
            char* prev_nbp = (char*)GET(bp+16); // prev block
            if (!prev_nbp)
                free_lists[level] = next_nbp;
            else
                PUT(prev_nbp+8, next_nbp);
            if (next_nbp)
                PUT((char*)next_nbp+16, prev_nbp);

            // Do same for buddy
            char* next_buddy = (char*)GET(buddy+8); // next block
            char* prev_buddy = (char*)GET(buddy+16); // prev block
            if (!prev_buddy)
                free_lists[level] = next_buddy;
            else
                PUT(prev_buddy+8, next_buddy);
            if (next_buddy)
                PUT((char*)next_buddy+16, prev_buddy);

            // Placed coalesced block on next level
            // Clear buddy blocks 
            PUT((char*)bp, 0);
            PUT((char*)bp+8, 0);
            PUT((char*)bp+16, 0);

            char* upper_block = free_lists[level - 1];
            if (upper_block)
                PUT((char*)upper_block+16, buddy);
            free_lists[level - 1] = buddy;
            level = level - 1;
            PUT((char*)buddy, PACK(level << 1, 0));        
            PUT((char*)buddy+8, upper_block);
            PUT((char*)buddy+16, 0);   
        }
    }    
}

/**********************************************************
 * my_free
 **********************************************************/
void mm_free(void *bp)
{
    // Ignore spurious requests
    // Get Size, thereby, determining the level
    // Calculate the relevant index
    // Set to free and add it to head of that relevant level
    // Check for coalescing

    if (bp == NULL)
        return;

    // Get the header pointer
    bp = (char*)bp - OFFSET;
    
    // Need to figure out which level to put it at
    int level = GET(bp);
    level = level >> 1;
    // Clear allocated bit
    PUT(bp, PACK(level<<1, 0));
    // Place on particular table
    char* oldHead = free_lists[level];
    if (oldHead)
        PUT(oldHead+16, bp);
    free_lists[level] = bp;
    PUT((char*)bp+8, oldHead);
    PUT((char*)bp+16, 0);
    //mm_coalesce(bp, level);
}

/**********************************************************
 * my_realloc
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    // Ignore spurious requests
    // if oldptr is NULL, then just call malloc
    // Call malloc on new size
    // Get the size of oldptr and put MAX (oldsize, size) as the header
    // Call memcpy
    // Free oldptr
    
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    // if size is within the block allocated, then use same block

    void *oldptr = ptr;
    void *newptr;
    size_t oldSize;

    /* Get metrics */
    int levels = GET(((char*)oldptr)-OFFSET);
    levels = levels >> 1;
    oldSize = 1 << (HIGHEST_POWER - levels);
    if (((size+OFFSET) <= oldSize) && ((size+OFFSET) > (oldSize>>1)))
    {
        return memmove(oldptr, oldptr, size);         
    }
    newptr = mm_malloc(size);
    if (newptr == NULL)
    {
        printf("\nRealloc Null for %zu old and %zu new\n", oldSize, size);
        return NULL;
    }
    memcpy(newptr, oldptr, size);
    mm_free(oldptr);

    return newptr;
}

