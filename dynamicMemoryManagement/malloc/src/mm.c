/*
 Segregated list implementation
 
 Allocated block strucutre:
 header (1 word): stores block size and 'allocated' bit
 foot (1 word): stores block size and 'allocated' bit
 
 Free block structure:
 header (1 word): stores block size and 'allocated' bit
 prev free block pointer (1 word): store location of prev 
	free block in the free list
 next free block pointer (1 word): store location of next 
	free block in the free list
 foot (1 word): stores block size and 'allocated' bit
 
 Free list structure (segregated):
	-each list size range is a power of 2
 list 0: <= 32B
 list 1: >32B && <=64B
 list 2: >64B && <=128B
 ...
 list NUM_FREE_LISTS-1: >2^(NUM_FREE_LISTS-2+MIN_FREE_SIZE_POW)
 
 For example, NUM_FREE_LISTS=5, MIN_FREE_SIZE_POW=5
 list 0: <=32B
 list 1: >32B && <=64B
 list 2: >64B && <=128B
 list 3: >128 && <256B
 list 4: >256B
 
 Allocator policy: First fit
 The allocator starts searching from the list that is index by
 list_index(size). If no suitable block is found in that list,
 move on to the next (bigger list). It returns the FIRST 
 suitable block free block.
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
    "Void",
    /* First member's full name */
    "Yi Ping Sun",
    /* First member's email address */
    "peteryiping.sun@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Ling Zhong",
    /* Second member's email address (leave blank if none) */
    "jack.zhong@mail.utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))
#define MIN(x,y) ((x) > (y)?(y) :(x))

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

void* heap_listp = NULL;

typedef struct free_block
{
	struct free_block* prev;
	struct free_block* next;    
} free_block;

/* Segregated lists structure */
#define NUM_FREE_LISTS	1
free_block* free_lists[NUM_FREE_LISTS];

/* Minimum size of free block */
#define MIN_FREE_SIZE_POW   5
#define MIN_FREE_SIZE       (1<<MIN_FREE_SIZE_POW)




/* Find the segregated list to starting searching from
list 0: <= 32B
list 1: >32B && <=64B
list 2: >64B && <=128B
...
list NUM_FREE_LISTS-1: >2^(NUM_FREE_LISTS-1+MIN_FREE_SIZE_POW)
*/
int list_index(size_t words)
{
    int index = 0;
    words--;
    while (words != 0) {
        words >>= 1;
        index++;
    }
    index = MAX(0, index-MIN_FREE_SIZE_POW);
    index = MIN(NUM_FREE_LISTS-1, index);
    return index;
}

/* Remove a free block from the free list */
void list_remove(free_block* bp)
{
    if (bp==NULL)
        return;

    int index = list_index(GET_SIZE(HDRP(bp)));
    
    if (bp->next==bp) /* only one block in list */
        free_lists[index] = NULL;
    else
    {
    	/* block is the head, update to new head */
	    if (free_lists[index] == bp) 
            free_lists[index] = bp->next;
		/* update pointers of prev and next blocks */
		bp->next->prev = bp->prev;
        bp->prev->next = bp->next;
    }
}

/* Add a free block to the FRONT of the free list 
-> First Fit
Free list is implemented as a circularly linked list
*/
void list_add(free_block* bp)
{
    if (bp==NULL)
        return;

    int index = list_index(GET_SIZE(HDRP(bp)));
    
    /* list is empty, make the free block link to itself */
    if (free_lists[index] == NULL) {
    	bp->next = bp;
        bp->prev = bp;
        free_lists[index] = bp; 
    }
    else { /* update pointers of prev and next blocks */
        bp->next = free_lists[index];
        bp->prev = free_lists[index]->prev;
        bp->prev->next = bp;
        bp->next->prev = bp;
    }
        
    
}


/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                         // alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
    heap_listp += DSIZE;
    
    int i;
    /* Initialize all seg list heads */
    for (i=0; i < NUM_FREE_LISTS; i++)
        free_lists[i] = NULL;

    return 0;
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        list_remove((free_block*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        list_remove((free_block*)PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            					  /* Case 4 */
        list_remove((free_block*)PREV_BLKP(bp));
        list_remove((free_block*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        return (PREV_BLKP(bp));
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
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

    return bp;
}


/**********************************************************
 * find_fit
 * Traverse the seg lists searching for a block to fit adjusted_size
 * Return NULL if no free blocks can handle that size
 * Assumed that adjusted_size is aligned
 **********************************************************/
void * find_fit(size_t adjusted_size)
{
    int index = list_index(adjusted_size);
    int i;
    for (i = index; i < NUM_FREE_LISTS; i++) {
        if (free_lists[i] != NULL) {
			/* start search from the head of the seg list
			If no sutiable fit is found on current lists
			keep search in the next list
			*/			
            free_block* p = free_lists[i];
            do {
                int size = GET_SIZE(HDRP(p));
                int free_size = size - adjusted_size;
				/* Found a suitable block */
                if (size >= adjusted_size) {
					/* Leftover space is not enough to be broken into 
					another free block */
		            if (free_size < MIN_FREE_SIZE) {
		                list_remove(p);
		                return (void*)p;
		            }
		            else { /* Break leftover into new free list block */
		            	list_remove(p);
		                PUT(HDRP(p), PACK(adjusted_size, 0));
		                PUT(FTRP(p), PACK(adjusted_size, 0));
		                
		                void* free_ptr = (void*)p + adjusted_size;
		                PUT(HDRP(free_ptr), PACK(free_size, 0));
		                PUT(FTRP(free_ptr), PACK(free_size, 0));
		                list_add(free_ptr);
		                
		                return (void*)p;
		            }
                }
                p = p->next;
            } while(p!=free_lists[i]);

        }
        
        
    }
    
    return NULL;
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t adjusted_size)
{
  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    list_add((free_block*)coalesce(bp));
}

/* Adjust block size to include overhead and alignment reqs. */
size_t mm_size_adjust(size_t size)
{
    if (size <= DSIZE)
        return MIN_FREE_SIZE;
    else
        return  DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
}

/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    size_t adjusted_size; /* adjusted block size */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
	
	/* Optimize for binary trace pattern
	 round to power of 2 blocks */
	if (size < 512) { 
	   	int i = 1;
	   	while (i < size)
	   		i <<= 1;
	   	size = i;
	}
        
    /* Adjust block size to include overhead and alignment reqs. */
    adjusted_size = mm_size_adjust(size);

    /* Search the free list for a fit */
    if ((bp = find_fit(adjusted_size)) != NULL) {
        place(bp, adjusted_size);
        return bp;
    }

    /* No fit found. Get more memory and place the block */	
	if ((bp = extend_heap(adjusted_size/WSIZE)) == NULL)
        return NULL;
    place(bp, adjusted_size);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Shrink and extend an existing block to a new size
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
	void *coal_ptr;
    void *new_ptr;
    
    size_t adjusted_size; /* adjusted block size */
    size_t old_size;
    size_t coal_size;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));
    
    adjusted_size = mm_size_adjust(size);
    old_size = GET_SIZE(HDRP(ptr));
    
	/* Size does not change */
    if (adjusted_size == old_size)
    	return ptr;
    
    /* Size shrinks */
    else if (adjusted_size < old_size) { 
    	int free_size = old_size - adjusted_size;
		/* See if salvaging leftover space is possible */
    	if (free_size >= MIN_FREE_SIZE) {
            PUT(HDRP(ptr), PACK(adjusted_size, 1));
            PUT(FTRP(ptr), PACK(adjusted_size, 1));
            
            void* free_ptr = (void*)ptr + adjusted_size;
		    PUT(HDRP(free_ptr), PACK(free_size, 0));
		    PUT(FTRP(free_ptr), PACK(free_size, 0));
		    list_add(free_ptr);
    	}
    	return ptr;
    }
    
    /* Size expands */
    else {
		/* Free the old block because it is guaranteed that it 
		won't fit the new demand 
		Coalesce with neighbors -> possibly the coalesced block
		can fit the new demand
		*/
		PUT(HDRP(ptr), PACK(old_size, 0));
		PUT(FTRP(ptr), PACK(old_size, 0));
		coal_ptr = coalesce(ptr);
		coal_size = GET_SIZE(HDRP(coal_ptr));
		
		/* New size fits in coalesced block */
		if (coal_size >= adjusted_size) {
			int free_size = coal_size - adjusted_size;
			/* Salvaging is possible */
			if (free_size >= MIN_FREE_SIZE) {
				memmove(coal_ptr, ptr, old_size - DSIZE);
				PUT(HDRP(coal_ptr), PACK(adjusted_size, 1));
		    	PUT(FTRP(coal_ptr), PACK(adjusted_size, 1));
		    	
				void* free_ptr = (void*)coal_ptr + adjusted_size;    
		        PUT(HDRP(free_ptr), PACK(free_size, 0));
		        PUT(FTRP(free_ptr), PACK(free_size, 0));
		        list_add(free_ptr);   
			}
			/* Salvaging not possible -> just use the entire block */
			else {
				memmove(coal_ptr, ptr, old_size - DSIZE);
				PUT(HDRP(coal_ptr), PACK(coal_size, 1));
		    	PUT(FTRP(coal_ptr), PACK(coal_size, 1));
			}
			return coal_ptr;
		}

		/* New size does not fit in coalesced block 
		Must call malloc to switch to a new block
		*/
		else {
			new_ptr = mm_malloc(size);
			if (new_ptr == NULL)
			  return NULL;
			  
			/* Copy the old data. */
			memcpy(new_ptr, ptr, old_size - DSIZE);
			
			/* Add the freed coalesce block to seg list */
			list_add((free_block*)coal_ptr); 
			
			return new_ptr;
		}
    }
    
    return NULL;
}



/**********************************************************
 * check_marked_as_free
 * check whether each freeblock's header and footer
 * isAllocated bit is not set
 * returns 1 is check is successful
 *********************************************************/
int check_marked_as_free() {
	int i;
	free_block* current;
	free_block* srcRunner;
	for (i = 0; i < NUM_FREE_LISTS; i++) {
		current = free_lists[i];
		srcRunner = current;
		while (srcRunner->next != current) {
			if (GET_ALLOC(srcRunner)) {
				return 0;
			}
		}
	}
	return 1;
}

/**********************************************************
 * check_missing_coalescing
 * check whether each freeblock's end address collide with
 * another free block's start N^2 brute force approach
 * returns 1 is check is successful
 *********************************************************/
int check_missing_coalescing() {
	int i;
	free_block* current;
	free_block* srcRunner;
	free_block* dstRunner;
	for (i = 0; i < NUM_FREE_LISTS; i++) {
		current = free_lists[i];
		srcRunner = current;
		while (srcRunner->next != current) {
			dstRunner = srcRunner;
			while (dstRunner->next != srcRunner) {
				if ((FTRP(srcRunner)+WSIZE) == HDRP(dstRunner)) {
					return 0;
				}
				dstRunner = dstRunner->next;
			}
			srcRunner = srcRunner->next;
		}
	}
	return 1;
}

/**********************************************************
 * check_freeblock_in_freelist
 * check whether each free block exists in the freelist
 * by checking the isAllocated bit in every block then find
 * if in free list
 * M*N^2 bruteful search and compare, M = number of freelist
 * returns 1 is check is successful
 *********************************************************/
int check_freeblock_in_freelist() {
	int found = 0;
	int i;
	free_block* current = heap_listp;
	free_block* srcRunner;
	free_block* dstRunner;
	while (current->next) {
		if (! GET_ALLOC(current)) {
			for (i = 0; i < NUM_FREE_LISTS; i++) {
				srcRunner = free_lists[i];
				dstRunner = srcRunner;
				while (dstRunner->next != srcRunner) {
					if (dstRunner == current) {
						found = 1;
					}
					srcRunner = srcRunner->next;
				}
			}
			if (! found) {
				return 0;
			}
		}
		found = 0;
		current = current->next;
	}
	return 1;
}

/**********************************************************
 * check_valid_free_pointer
 * check whether each free block pointer in free list points
 * to valid free blocks
 * returns 1 is check is successful
 *********************************************************/
int check_valid_free_pointer() {
	int i;
	free_block* current;
	free_block* srcRunner;
	for (i = 0; i < NUM_FREE_LISTS; i++) {
		current = free_lists[i];
		srcRunner = current;
		while (srcRunner->next != current) {
			if (GET_ALLOC(srcRunner)) {
				return 0;
			}
			srcRunner = srcRunner->next;
		}
	}
	return 1;
}



/**********************************************************
 * check_block_overlap
 * check whether each block overlap by checking the address
 * of the footer base vs. the header of another block
 * N^2 bruteful search and compare
 * returns 1 is check is successful
 *********************************************************/
int check_block_overlap() {
	free_block* current = heap_listp;
	free_block* srcRunner;
	free_block* dstRunner;
	while (current->next) {
		srcRunner = current;
		dstRunner = srcRunner;
		while (dstRunner->next != srcRunner) {
			if ((FTRP(srcRunner)+WSIZE) > HDRP(dstRunner)) {
				return 0;
			}
			dstRunner = dstRunner->next;
		}
		srcRunner = srcRunner->next;
	}
	return 1;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
	int isSuccessful = 1;
	// Is every block in the free list marked as free?
	isSuccessful = isSuccessful && check_marked_as_free();
	// Are there any contiguous free blocks that somehow escaped coalescing?
	isSuccessful = isSuccessful && check_missing_coalescing();
	// Is every free block actually in the free list?
	isSuccessful = isSuccessful && check_freeblock_in_freelist();
	// Do the pointers in the free list point to valid free blocks?
	isSuccessful = isSuccessful && check_valid_free_pointer();
	// Do any allocated blocks overlap?
	isSuccessful = isSuccessful && check_block_overlap();
	/*if (!isSuccessful) {
	 *	printf("THE CHECK FAILED");
	 *}
	 */
	return isSuccessful ? 1 : 0;
}



