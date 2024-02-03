/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>
double totalsize = 0;
double payload = 0;
double payload2 = 0;
int counter = 0;
size_t calculate_padding(size_t size, size_t alignment);
int findFibonacci(size_t t);
void *find_empty_block(size_t index);
int blockSize(sf_block *value);
void add_block_in_list(sf_block *ptr, int index);
sf_block *coalesce_blocks(sf_block *first);
void remove_block_in_list(sf_block *ptr);

void *sf_malloc(size_t size) {
    //if size is 0, return null but no error
    if(size == 0) return NULL;
    size_t alignment = 16;
    size_t total_size = 0;
    if(size < 16){
        total_size = 0 + sizeof(sf_block);
    } else{
        total_size = size - 16 + sizeof(sf_block) + calculate_padding(size, alignment);
    }
    //if size is less than 32 throw a error
    if(total_size < 32){
        sf_errno = ENOMEM;
        return NULL;
    }
    
    //extends heap if it's the first iteration
    if(sf_mem_start() == sf_mem_end()){
        void *growth = sf_mem_grow();
        if(growth == NULL){
            sf_errno = ENOMEM;
            return NULL;
        }
        size_t sizesub = size;
        while(sizesub > PAGE_SZ-64){
            void *p = sf_mem_grow();

            if(p == NULL){
                //don't feel like editing my code, so it just makes a free block
                //instead of rewriting my code completely.
                for(int i = 0 ; i<NUM_FREE_LISTS ; i++){
                    sf_free_list_heads[i].body.links.next=&sf_free_list_heads[i];
                    sf_free_list_heads[i].body.links.prev=&sf_free_list_heads[i];
                }

                //Make prologue
                sf_block *prologue = (sf_block*)sf_mem_start();
                prologue->header = 32;
                prologue->header |= 8;
                
                //sets the rest as a free block.
                sf_block *freeblock = (sf_block *)sf_mem_start() + 32;
                uint64_t head1 = sf_mem_end()-sf_mem_start()-48;
                head1 |= 4;
                freeblock->prev_footer = prologue->header;
                freeblock->header = head1;

                if(blockSize(freeblock) != 0){
                    add_block_in_list(freeblock, NUM_FREE_LISTS-1);
                }

                //needs a epilogue block of size 32 bits
                sf_block *epi = (sf_block*)(sf_mem_end()-16);
                epi->header = 8;
                epi->prev_footer = head1;
                sf_errno = ENOMEM;
                return NULL;
            }
            sizesub -= PAGE_SZ;
        }
        if(growth==NULL){
            sf_errno = ENOMEM;
            return NULL;
        }
        for(int i = 0 ; i<NUM_FREE_LISTS ; i++){
            sf_free_list_heads[i].body.links.next=&sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev=&sf_free_list_heads[i];
        }

        //Make prologue
        sf_block *prologue = (sf_block*)growth;
        prologue->header = 44;

        //creates a new block on the heap
        sf_block *new_block = growth + 32;

        uint64_t head = 0;

        // Search for a free block in the free list that is large enough if not large enough, just returns NUM_FREE_LISTS-2 (counts for wilderness block)
        int index = total_size + calculate_padding(total_size, alignment);
        head = index;
        head |= ((size & 0x0FFFFFFF) << 32);
        if(size > payload) payload = size;
        payload2 += size;
        head |= 8;
        head |= 4;
        new_block->header = head;
        new_block->prev_footer = prologue->header;
        int leftFree = 0;
        if((sf_mem_end() - 16 - (sf_mem_start() + 32 + blockSize(new_block))) == 16){
            new_block->header += 16;
            leftFree = 1;
        }
        totalsize += blockSize(new_block);
        counter++;

        uint64_t head1 = 0;
        if(leftFree == 0){
            //sets the rest as a free block.
            sf_block *freeblock = growth + 32 + blockSize(new_block);
            head1 |= sf_mem_end() - 16 - (sf_mem_start() + 32 + blockSize(new_block));
            head1 |= 4;
            freeblock->prev_footer = new_block->prev_footer;
            freeblock->header = head1;
            if(blockSize(freeblock) != 0){
                add_block_in_list(freeblock, NUM_FREE_LISTS-1);
            }
        }

        //needs a epilogue block of size 32 bits
        sf_block *epi = (sf_block*)(sf_mem_end()-16);
        epi->header = 8;
        epi->prev_footer = head1;
        if (leftFree == 0){
            epi->header |= 4;
        }

        return new_block->body.payload;
    } else {
        int index = findFibonacci(total_size);
        //goes through the whole freelist starting from index, if it doesn't find it, it'll return wilderness block.
        sf_block *block = find_empty_block(index);
        //looks for a free area on the free list, if not there it iterates through the 'heap' and sets it
        if(block == NULL){
            void *p = sf_mem_grow();
            if(p == NULL){
                sf_errno = ENOMEM;
                return NULL;
            }
            block = p - 16;
            block->header = (size << 32);
            block->header |= total_size;
            block->header |= 8;
            block->header |= 4;
            if(PAGE_SZ - total_size < 32 && PAGE_SZ >= total_size){
                block->header = (size << 32);
                block->header |= PAGE_SZ;
                block->header |= 8;
                block->header |= 4;

                sf_block *epi = sf_mem_end() - 16;
                epi->header = 12;
                epi->prev_footer = block->header;
                return block->body.payload;
            }

            sf_block *rest = p + blockSize(block) - 16;
            rest->prev_footer = block->header;
            rest->header = PAGE_SZ - total_size;
            rest->header |= 4;
            add_block_in_list(rest, NUM_FREE_LISTS - 1);

            sf_block *epi = sf_mem_end() - 16;
            epi->header = 8;
            epi->prev_footer = rest->header;
            return block->body.payload;
        } else{
            //if the internal fragmentation is less than 32
            if(blockSize(block) - total_size < 32){
                remove_block_in_list(block);
                size_t sizes = blockSize(block);
                sf_block *nextBlock = (void *) block + sizes;

                size_t head = sizes;
                head |= ((size & 0x0FFFFFFF) << 32);
                head |= 8;
                if((block->prev_footer & 0x4) == 4) head |= 4;
                block->header = head;
                totalsize += sizes;
                if(size > payload) payload = size;
                payload2 += size;
                counter++;
                nextBlock->prev_footer = head;

                sf_block *nnblock = nextBlock + blockSize(nextBlock);
                nnblock->prev_footer |= 4;
                nextBlock->header |= 4;
                return block->body.payload;
            } else {
                remove_block_in_list(block);

                sf_block *nextBlock = (void *) block + blockSize(block);

                size_t head1 = blockSize(block);
                int hold = findFibonacci(total_size);

                // if the space is not enough, largen heap
                size_t temp = total_size;
                size_t comp = blockSize(block);
                while(temp > comp){

                    //grab the epilogue and remove it.
                    if((void *)block + blockSize(block) == sf_mem_end()-16){
                        sf_block *epi = (void *)block + blockSize(block);
                        epi->prev_footer = 0;
                        epi->header = 8;
                    }

                    sf_mem_grow();
                    comp += PAGE_SZ;
                    
                    //make a new free block and epilogue
                    block->header = block->header+PAGE_SZ;
                    sf_block *epi = (sf_block*)(sf_mem_end()-16);
                    epi->header = 8;
                    epi->prev_footer = block->header;
                }

                int index = total_size + calculate_padding(total_size, alignment);
                size_t head = index;

                head = index;
                head |= ((size & 0x0FFFFFFF) << 32);
                head |= 8;
                if((block->prev_footer & 0x4) == 4) head |= 4;
                block->header = head;
                totalsize += blockSize(block);
                if(size > payload) payload = size;
                payload2 += size;
                counter++;

                //makes the rest of the free block.
                sf_block *rest = (void *)block + blockSize(block);
                rest->prev_footer = head;
                rest->body.links.next = block->body.links.next;
                rest->body.links.prev = block->body.links.prev;

                if(hold != NUM_FREE_LISTS-2) {
                    head1 = head1 - blockSize(block);
                } else {
                    head1 = (sf_mem_end()-16 - ((void *)block + blockSize(block)));
                }

                head1 |= 4;
                rest->header = head1;

                //check if next block is epilogue, if it is, set's the prevheader.
                if((void *)rest + blockSize(rest) == sf_mem_end()-16 && blockSize(rest) != 0){
                    sf_block *epi = (void *)rest + blockSize(rest);
                    epi->prev_footer = head1;
                    add_block_in_list(rest, NUM_FREE_LISTS-1);
                } else if(blockSize(rest) != 0){
                    nextBlock->prev_footer = head1;
                    int num = findFibonacci(blockSize(rest));
                    add_block_in_list(rest, num);
                }

                return block->body.payload;
            }
        }
    }
    return NULL;
}

void sf_free(void *pp) {
    if(pp==NULL){
        abort();
    }
    pp -= 16;
    sf_block *block = (sf_block*)pp;

    //trying to free a free block
    if ((block->header & 0x8) == 0) {
        abort();
    }
    //trying to free a block not in heap || footer is after epilogue
    if ((sf_mem_start() + 40) > (void *)(&(block->header)) || (((void*)block+blockSize(block))+8)>(sf_mem_end()-8)) {
        abort();
    }
    //trying to free a block smaller than permitted
    if (blockSize(block) < 32) {
        abort();
    }
    payload2 -= (block->header >> 32);
    block->header &= ~(1u << 3);
    sf_block *next = (void *)block + blockSize(block);
    next->prev_footer &= ~(1u << 3);
    coalesce_blocks(block);
}

void *sf_realloc(void *pp, size_t rsize) {
    if (rsize == 0){
        sf_free(pp-16);
        return NULL;
    }
    // To be implemented.
    sf_block *block = pp-16;
    size_t mallocSize = rsize - 16 + sizeof(sf_block) + calculate_padding(rsize, 16);
    size_t holder = blockSize(block);
    int pay = block->header>>32;
    sf_block *nextBlock = (void *)block + blockSize(block);
    size_t heading = 0;
    char temp[pay];
    memcpy(temp, block->body.payload, pay);

    if((block->header & 8) == 0){
        sf_errno = EINVAL;
        return NULL;
    }

    size_t origSize = (block->header >> 32);

    if(pp == NULL){
        sf_errno = EINVAL;
        return NULL;
    }
    //starts before heap
    if (sf_mem_start() + 40 > (void *)&(block->header)) {
        sf_errno = EINVAL;
        return NULL;
    }

    //ends after heap
    void *footer_addr = (void *)&block->header + blockSize(block);
    if (footer_addr > (sf_mem_end() - 8)) {
        sf_errno = EINVAL;
        return NULL;
    }

    if(blockSize(block) < 32){
        sf_errno = EINVAL;
        return NULL;
    }

    //if the internal fragmentation is less than 32
    if((origSize - rsize < 32) && (origSize - rsize > 0)){
        heading |= ((rsize & 0x0FFFFFFF) << 32);
        heading |= (block->header << 32) >> 32;
        block->header = heading;
        nextBlock->prev_footer = heading;
        
        totalsize += blockSize(block);
        if(rsize > payload) payload = rsize;
        payload2 -= origSize;
        payload2 += rsize;
        counter++;
        return block->body.payload;
    }
    if(origSize - rsize == 0){
        return block->body.payload;
    }
    
    int t = 0;
    if(mallocSize > holder){
        void *p = sf_malloc(rsize);
        sf_free(pp);
        t = 1;
        return p;
    }
    if(t == 0) sf_free(pp);
    
    if(origSize > rsize) origSize = rsize;
    void *newBlock = sf_malloc(rsize);

    if(newBlock == NULL){
        sf_errno = ENOMEM;
        return NULL;
    }

    memcpy(newBlock, temp, origSize);
    return newBlock;
}

//ratio of the total amount of payload to the total size of allocated blocks.
double sf_fragmentation() {
    if(counter == 0) return 0;
    return payload2/totalsize;
}

double sf_utilization() {
    if(sf_mem_start() == sf_mem_end()) return 0;
    return payload/(sf_mem_end() - sf_mem_start());
}

size_t calculate_padding(size_t size, size_t alignment) {
    size_t padding = alignment - (size % alignment);
    return (padding == alignment) ? 0 : padding;
}

//takes in a size, not a header
int findFibonacci(size_t t){
    unsigned long long fib[NUM_FREE_LISTS];
    fib[0] = 1;
    fib[1] = 2;
    for (int i = 2; i < NUM_FREE_LISTS-2; i++) {
        fib[i] = fib[i - 1] + fib[i - 2];
    }
    for (int i = 0; i < NUM_FREE_LISTS-2; i++) {
        if(i == NUM_FREE_LISTS-2){
            return i;
        }
        if(t <= (fib[i]*32)) {
            return i;
        }
    }
    return NUM_FREE_LISTS-2;
}

//goes through the free list and looks for free blocks
void *find_empty_block(size_t index){
    
    for(int i = index; i < NUM_FREE_LISTS; i++){
        void *a = &sf_free_list_heads[i];
        void *b = sf_free_list_heads[i].body.links.next;
        while (a != b){
            b = ((sf_block *)b)->body.links.next;
        }
        if(a != ((sf_block *) b)->body.links.prev){
            return (((sf_block *)b)->body.links.prev);
        }
    }
    return NULL;
}

int blockSize(sf_block *value){
    // Define the starting and ending bit positions
    int start_bit = 4;
    int end_bit = 32;

    // Calculate the number of bits to extract (inclusive of end_bit)
    int num_bits = end_bit - start_bit + 1;

    // Create a mask with the desired bits set to 1
    unsigned int mask = ((1U << num_bits) - 1) << start_bit;

    // Extract the bits from the value
    unsigned int extracted_bits = (value->header & mask) >> start_bit;

    return extracted_bits * 16;
}

void add_block_in_list(sf_block *ptr, int index) {
    sf_block *sentinel = &sf_free_list_heads[index];

    ptr -> body.links.next = sentinel -> body.links.next;
    ptr -> body.links.prev = sentinel;

    sentinel -> body.links.next -> body.links.prev = ptr;
    sentinel -> body.links.next = ptr;
}

sf_block *coalesce_blocks(sf_block *block){
    sf_block *prevBlock = (void*)block - (block->prev_footer & 0xFFFFFFF0);
    size_t prevAllocated = block->header & 0x4;
    sf_block *nextBlock = (void*)block + blockSize(block);
    size_t nextAllocated = nextBlock->header & 0x8;
    counter--;
    payload2 -= (block->header >> 32);
    totalsize -= blockSize(block);

    if(prevAllocated == 0 && nextAllocated > 0){
        //done
        remove_block_in_list(prevBlock);
        remove_block_in_list(block);

        size_t total = blockSize(block) + blockSize(prevBlock);
        int x = findFibonacci(total);
        total |= (prevBlock->header & 0xf);
        total |= (prevBlock->header & 0xFFFFFFFF00000000);
        block->header=0;
        block->prev_footer=0;
        prevBlock->header = total;
        nextBlock->prev_footer = total;
        nextBlock->header = nextBlock->header & ~(0x4);
        sf_block *last = (void *)block + blockSize(block);
        last->prev_footer = nextBlock->header;
        add_block_in_list(prevBlock, x);
        return prevBlock;
        
    } else if(prevAllocated > 0 && nextAllocated == 0){
        //done
        size_t total = blockSize(block) + blockSize(nextBlock);
        remove_block_in_list(nextBlock);

        size_t newa = total;
        newa |= block->header & 0xFFFFFFFF00000000;
        newa |= 4;
        block->header = newa;
        
        nextBlock->prev_footer = 0;
        block->prev_footer = prevBlock->header;
        nextBlock->header = 0;
        //sets the block after the next one's footer.
        sf_block *temp = (void *)block+total;
        temp->prev_footer = newa;

        //if epilogue, set as wilderness block
        if((void *) block + total == sf_mem_end()-16){
            add_block_in_list(block, NUM_FREE_LISTS-1);
        } else{
            int x = findFibonacci(total);
            add_block_in_list(block, x);
        }
        return block;

    } else if(prevAllocated > 0 && nextAllocated > 0){
        //done
        int x = findFibonacci(blockSize(block));
        add_block_in_list(block, x);
        nextBlock->header &= ~(0x4);
        nextBlock->prev_footer = block->header;
        return block;
    } else if(prevAllocated == 0 && nextAllocated == 0){
        remove_block_in_list(prevBlock);
        remove_block_in_list(nextBlock);

        size_t total1 = blockSize(prevBlock)+blockSize(nextBlock)+blockSize(block);
        total1 |= 4;
        //payload size ?
        total1 |= ((prevBlock->header >> 32) & 0xFFFFFFFF) << 32;
        block->header = 0;
        block->prev_footer = 0;
        sf_block *lastBlock = (void *)nextBlock + blockSize(nextBlock);
        lastBlock->prev_footer = total1;
        nextBlock->header = 0;
        prevBlock->header = total1;

        if((void *)nextBlock + blockSize(nextBlock) == sf_mem_end() - 16){
            add_block_in_list(prevBlock, NUM_FREE_LISTS-1);
            sf_block *epi = (sf_block*)(sf_mem_end()-16);
            epi->header = 8;
            epi->prev_footer = total1;
        } else{
            int y = findFibonacci(blockSize(prevBlock));
            add_block_in_list(prevBlock,y);
        }
        return prevBlock;

    } else {
        return NULL;
    }
}

// Remove the specified block from its free-list.
void remove_block_in_list(sf_block *ptr) {
    for(int i = 0; i < NUM_FREE_LISTS; i++){
        sf_block *a = &sf_free_list_heads[i];
        sf_block *b = sf_free_list_heads[i].body.links.next;
        while (a != b){
            if(b == ptr){
                (ptr->body.links.prev)->body.links.next = ptr->body.links.next;
                (ptr->body.links.next)->body.links.prev = ptr->body.links.prev; 
                return;
            }
            if(b->body.links.next != a){
                b = b->body.links.next;
            } else{
                break;
            }
        }
    }
}
