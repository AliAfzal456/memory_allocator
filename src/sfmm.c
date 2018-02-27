/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

free_list seg_free_list[4] = {
    {NULL, LIST_1_MIN, LIST_1_MAX},
    {NULL, LIST_2_MIN, LIST_2_MAX},
    {NULL, LIST_3_MIN, LIST_3_MAX},
    {NULL, LIST_4_MIN, LIST_4_MAX}
};

int sf_errno = 0;

void *sf_malloc(size_t size) {
    /* the way this will work is first check if all lists are null. If null, then we need to request our first page
        after getting first page, store that pointer in a temporary pointer. We will come back to this later*/

    if (seg_free_list[0].head == NULL && seg_free_list[1].head == NULL && seg_free_list[2].head == NULL && seg_free_list[3].head == NULL){
        // if all 4 lists don't have anything, it's our first call. Request some memory
        void* break1 = sf_sbrk();

        // right now break1 points to the start of the heap, aka the start of the first block.
        // so the size is valid. set the header and footer and try blockprinting it
        sf_free_header *headerL = (sf_free_header*)break1;

        sf_header *hdr =(sf_header*)break1;

        headerL -> next = NULL;
        headerL -> prev = NULL;

        hdr -> allocated = 0;                 // not allocated
        hdr -> padded = 0;                    // no padding on first page
        hdr -> two_zeroes = 0;                // two zeros should be 0...
        hdr -> block_size = PAGE_SZ >> 4;     // block size is payload + header +  footer = 4096. Shift left to store properly
        hdr -> unused = 0;                    // unused bits, zero them out for fun ;)

        // and now to set the footer
        // get the footer address by adding block size to header address and subtracting 8
        // first get a pointer to the pointer
        void *ftr_ptr = (void*) hdr;
        // if i want hdr back,then opt = (tcp_option_t *) ptr;
        // https://stackoverflow.com/questions/16578054/how-to-make-a-pointer-increment-by-1-byte-not-1-unit

        void *footerAddress = ftr_ptr + (hdr -> block_size << 4) - 8;

        // printf("Heap Start: %p\n", get_heap_start());
        // printf("Header:     %p\n", hdr);
        // printf("Footer:     %p\n", footerAddress);
        // printf("Heap end:   %p\n", get_heap_end());


        sf_footer *footer = (sf_footer*)footerAddress;

        footer -> allocated = 0;
        footer -> padded = 0;
        footer -> two_zeroes = 0;
        footer -> block_size = PAGE_SZ >> 4;
        footer -> requested_size = 0;

        // the block has been prepared. Add it to the correct list (4th list)
        if (seg_free_list[0].min <= PAGE_SZ && PAGE_SZ <= seg_free_list[0].max){
            seg_free_list[0].head = break1;
        }
        if (seg_free_list[1].min <= PAGE_SZ && PAGE_SZ <= seg_free_list[1].max){
            seg_free_list[1].head = break1;
        }
        if (seg_free_list[2].min <= PAGE_SZ && PAGE_SZ <= seg_free_list[2].max){
            seg_free_list[2].head = break1;
        }
        if (seg_free_list[3].min <= PAGE_SZ && PAGE_SZ <= seg_free_list[3].max){
            seg_free_list[3].head = break1;
        }


        // printf("Size of stuff: %li\n", (sizeof(sf_free_header) + sizeof(sf_footer)));

    }

    // after the first block is prepared, all requests are handled in the same manner
    // now lets do some size validations
    // if size is less than 0 (smallest block possible, give nothing) or if greater than max size
    if (size <= 0 || size > (PAGE_SZ * 4)){
        sf_errno = EINVAL;
        return NULL;
    }

    // make the size into a multiple of 16
    int padding = 0;
    int alignedSize = size;
    while (alignedSize % 16 != 0){
        padding+=1;
        alignedSize +=1;
    }

    // so it seems that the size is valid. get the list that has blocks of that size.
    int indexOf = 0;
    if (LIST_1_MIN <= alignedSize && alignedSize <= LIST_1_MAX){
        indexOf = 0;
    }
    else if (LIST_2_MIN <= alignedSize && alignedSize <= LIST_2_MAX){
        indexOf = 1;
    }
    else if(LIST_3_MIN <= alignedSize && alignedSize <= LIST_3_MAX){
        indexOf = 2;
    }
    else{
        indexOf = 3;
    }

    // set padded if:
    // block_size != requested_size + 16
    void* returnVal = NULL;

    for (indexOf = 0; indexOf < 4; indexOf++){
        // search the current list to see if it has a block that can accomodate this size
        // only look at lists that only have blocks in them
        if(seg_free_list[indexOf].head != NULL){

            // the first block
            // this variable contains the header, the next and the previous
            sf_free_header *currentListHeader = (sf_free_header*) seg_free_list[indexOf].head;

            // check if the head can hold it
            // if it can't, then we  will traverse through the rest of the list
            sf_header *currentHeader = (sf_header*) seg_free_list[indexOf].head;

            if (alignedSize + 16 <= seg_free_list[indexOf].max){
                if ((currentHeader -> block_size << 4) >= alignedSize + 16){
                    if ((currentHeader -> block_size << 4) - (alignedSize+ 16) < 32){
                        // there will be a splinter, so return the entire block
                        // update header and footer and we'll be gucci
                        currentHeader -> allocated = 1;

                        // if there was padding, or if the current block size is not the same as the aligned size + header/footer, then padding
                        if (padding > 0 || (currentHeader -> block_size << 4) != alignedSize + 16){
                            currentHeader -> padded = 1;
                        }
                        else{
                            currentHeader -> padded = 0;
                        }
                        currentHeader -> two_zeroes = 0;
                        // block size remains the same
                        currentHeader -> unused = 0;

                        // and now the footer
                        void *ftr_ptr = (void*) currentHeader;
                        void *footerAddress = ftr_ptr + (currentHeader -> block_size << 4) - 8;

                        sf_footer *currentFooter = (sf_footer*) footerAddress;

                        currentFooter ->allocated = 1;
                        currentFooter -> padded = currentHeader -> padded;
                        currentFooter -> two_zeroes = 0;
                        // block size is unchanged
                        currentFooter -> requested_size = size; // requested size is what was originally asked for

                        // and now remove it from the head of the list
                        // printf("%p\n", seg_free_list[indexOf].head -> next);
                        seg_free_list[indexOf].head = currentListHeader -> next;

                        if (seg_free_list[indexOf].head != NULL){
                            seg_free_list[indexOf].head -> prev = NULL;
                        }
                        void *returnAddress = (void*) currentHeader;
                        returnVal = returnAddress + 8;

                    }

                    // else it won't cause a splinter, so split into 2
                    else{
                        currentHeader -> allocated = 1;
                        if (padding > 0){
                            currentHeader -> padded = 1;
                        }
                        else{
                            currentHeader -> padded = 0;
                        }
                        currentHeader -> two_zeroes = 0;
                        int oldSize = currentHeader -> block_size << 4;
                        currentHeader -> block_size = (alignedSize + 16) >> 4; // +16 is for the header + footer
                        currentHeader -> unused = 0;

                        // and now the footer
                        void *ftr_ptr = (void*) currentHeader;
                        void *footerAddress = ftr_ptr + (currentHeader -> block_size << 4) - 8;

                        sf_footer *currentFooter = (sf_footer*) footerAddress;

                        currentFooter ->allocated = 1;
                        if (padding > 0){
                            currentFooter -> padded = 1;
                        }
                        else{
                            currentFooter -> padded = 0;
                        }
                        currentFooter -> two_zeroes = 0;
                        currentFooter -> block_size = (alignedSize + 16) >> 4;
                        currentFooter -> requested_size = size; // requested size is what was originally asked for


                        // sf_blockprint(currentHeader);

                        returnVal = ftr_ptr + 8;
                        // printf("Start of block to return %p\n", returnVal);

                        // and now remove it from the head of the list
                        // move the current header to the end

                        // printf("Old size of the block was: %i\n", oldSize);
                        currentHeader = ftr_ptr + (currentHeader -> block_size << 4);

                        // printf("Start of new block %p\n", currentHeader);

                        currentHeader -> allocated = 0;
                        currentHeader -> padded = 0;
                        currentHeader -> two_zeroes = 0;
                        currentHeader -> block_size = (oldSize - (alignedSize + 16)) >> 4;
                        currentHeader -> unused = 0;

                        void *myNewPointer = (void*) currentHeader;
                        void *newFooterAdd = myNewPointer + (currentHeader -> block_size << 4) - 8;


                        currentFooter = newFooterAdd;

                        currentFooter -> allocated = 0;
                        currentFooter -> padded = 0;
                        currentFooter -> two_zeroes = 0;
                        currentFooter -> block_size = (oldSize - (alignedSize + 16)) >> 4;
                        currentFooter -> requested_size = 0;

                        // sf_blockprint(currentHeader);

                        // since we split from the head, this new block can only go at the head.
                        // put it at the head of the correct list
                        size_t newSize = (currentFooter -> block_size) << 4;
                        void* next = currentListHeader -> next;

                        // start of the list is now the second element
                        if (seg_free_list[indexOf].head != NULL){
                            seg_free_list[indexOf].head = seg_free_list[indexOf].head -> next;
                        }
                        // update its pointers
                        if (seg_free_list[indexOf].head != NULL){
                            seg_free_list[indexOf].head -> prev = NULL;
                        }

                        if((newSize <= (size_t)LIST_1_MAX) == 1 && newSize >= LIST_1_MIN){
                            // and now add this list to its correct location
                            next = seg_free_list[0].head;


                            seg_free_list[0].head = (void*)currentHeader;
                            seg_free_list[0].head -> next = next;
                            sf_free_header *opHeader = seg_free_list[0].head -> next;

                            if (opHeader != NULL){
                                opHeader ->prev = seg_free_list[0].head;
                            }
                            seg_free_list[0].head -> prev = NULL;
                        }
                        else if((newSize <= (size_t)LIST_2_MAX) == 1 && newSize >= LIST_2_MIN){
                            next = seg_free_list[1].head;


                            seg_free_list[1].head = (void*)currentHeader;
                            seg_free_list[1].head -> next = next;
                            sf_free_header *opHeader = seg_free_list[1].head -> next;
                            if (opHeader != NULL){
                                opHeader ->prev = seg_free_list[1].head;
                            }

                            seg_free_list[1].head -> prev = NULL;
                        }
                        else if((newSize <= (size_t) LIST_3_MAX) == 1 && newSize >= LIST_3_MIN){
                            next = seg_free_list[2].head;


                            seg_free_list[2].head = (void*)currentHeader;
                            seg_free_list[2].head -> next = next;
                            sf_free_header *opHeader = seg_free_list[2].head -> next;
                            if (opHeader != NULL){
                                opHeader ->prev = seg_free_list[2].head;
                            }

                            seg_free_list[2].head -> prev = NULL;
                        }
                        else{
                            next = seg_free_list[3].head;


                            seg_free_list[3].head = (void*)currentHeader;
                            seg_free_list[3].head -> next = next;
                            sf_free_header *opHeader = seg_free_list[3].head -> next;
                            if (opHeader != NULL){
                                opHeader ->prev = seg_free_list[3].head;
                            }

                            seg_free_list[3].head -> prev = NULL;;
                        }

                    }

                    // found memory, so return it and get out of here
                    //sf_blockprint(returnVal - 8);
                    return returnVal;
                }

                // otherwise, it seems that the head cannot fit the entire block. search the remainder of this list
                while (currentListHeader -> next != NULL){
                    // this variable includes the header, then next pointer and the previous pointer
                    currentListHeader = (sf_free_header*) currentListHeader -> next;    // get a reference to the next header

                    returnVal = NULL;
                    if ((currentListHeader -> header.block_size << 4) >= alignedSize + 16){
                        // if the block will cause a splinter
                        if ((currentListHeader -> header.block_size << 4) - (alignedSize +16) < 32){
                            // if we cant split the current block
                            // store pointer to its next and previous
                            void* next = currentListHeader -> next;
                            void* prev = currentListHeader -> prev;
                            // set the header of this block to whatever it should be
                            sf_header *myHead = (sf_header*) currentListHeader;

                            myHead -> allocated = 1;
                            if (padding > 0 || (currentListHeader -> header.block_size << 4) != alignedSize + 16 ){
                                myHead -> padded = 1;
                            }
                            else{
                                myHead -> padded = 0;
                            }
                            myHead -> two_zeroes = 0;
                            myHead -> block_size = myHead -> block_size;
                            myHead -> unused = 0;

                            // and now to set the footer
                            void *ftr_ptr = (void*) myHead;
                            void *footerAddress = ftr_ptr + (myHead -> block_size << 4) - 8;

                            returnVal = ftr_ptr + 8;

                            sf_footer *myFoot = (sf_footer*)footerAddress;
                            myFoot -> allocated = 1;
                            myFoot -> padded = myHead -> padded;
                            myFoot -> two_zeroes = 0;
                            myFoot -> block_size = myHead -> block_size;
                            myFoot -> requested_size = size;


                            // and now we need to move pointers to next and previous
                            // this block definitely has a previous pointer, but maybe not a next pointer
                            // set the 'next' of the previous to the next block
                            // sf_snapshot();
                            sf_free_header *prevHeader = (sf_free_header*)prev;
                            // printf("previous header: %p\n", prevHeader);
                            // printf("previous header next: %p\n", prevHeader ->next);
                            // printf("previous header next becomes: %p\n", next);

                            if (prevHeader != NULL){
                                prevHeader -> next = next;
                            }
                            // sf_snapshot();

                            sf_free_header *nextHeader = (sf_free_header*)next;

                            if (nextHeader != NULL){
                                nextHeader -> prev = prev;
                            }
                            // printf("%p%p\n", next,prev);

                            return returnVal;
                        }

                        else{
                            void* next = currentListHeader -> next;
                            void* prev = currentListHeader -> prev;

                            sf_header *myHead = (sf_header*) currentListHeader;
                            myHead -> allocated = 1;
                            if (padding > 0){
                                myHead -> padded = 1;
                            }
                            else{
                                myHead -> padded = 0;
                            }
                            myHead -> two_zeroes = 0;
                            int oldSize = myHead -> block_size << 4;
                            myHead -> block_size = (alignedSize + 16) >> 4; // +16 is for the header + footer
                            myHead -> unused = 0;

                            // and now the footer
                            void *ftr_ptr = (void*) myHead;
                            void *footerAddress = ftr_ptr + (myHead -> block_size << 4) - 8;

                            sf_footer *currentFooter = (sf_footer*) footerAddress;

                            currentFooter ->allocated = 1;
                            if (padding > 0){
                                currentFooter -> padded = 1;
                            }
                            else{
                                currentFooter -> padded = 0;
                            }
                            currentFooter -> two_zeroes = 0;
                            currentFooter -> block_size = (alignedSize + 16) >> 4;
                            currentFooter -> requested_size = size; // requested size is what was originally asked for


                            // sf_blockprint(myHead);
                            returnVal = ftr_ptr + 8;
                            // printf("Start of block to return %p\n", returnVal);

                            // and now remove it from the head of the list
                            // move the current header to the end

                            // printf("Old size of the block was: %i\n", oldSize);
                            myHead = ftr_ptr + (myHead -> block_size << 4);

                            // printf("Start of new block %p\n", myHead);

                            myHead -> allocated = 0;
                            myHead -> padded = 0;
                            myHead -> two_zeroes = 0;
                            myHead -> block_size = (oldSize - (alignedSize + 16)) >> 4;
                            myHead -> unused = 0;

                            void *myNewPointer = (void*) myHead;
                            void *newFooterAdd = myNewPointer + (myHead -> block_size << 4) - 8;


                            currentFooter = newFooterAdd;

                            currentFooter -> allocated = 0;
                            currentFooter -> padded = 0;
                            currentFooter -> two_zeroes = 0;
                            currentFooter -> block_size = (oldSize - (alignedSize + 16)) >> 4;
                            currentFooter -> requested_size = 0;

                            // sf_blockprint(currentHeader);
                            // the old block had a previous pointer, and might have had a next pointer too. Update those pointers
                            // void* next = currentListHeader -> next;
                            // void* prev = currentListHeader -> p
                            sf_free_header *prevHeader = (sf_free_header*)prev;
                            prevHeader -> next = next;

                            sf_free_header *nextHeader = (sf_free_header*)next;

                            if (next != NULL){
                                nextHeader -> prev = prev;
                            }
                            // since we split from the head, this new block can only go at the head.
                            // put it at the head of the correct list
                            size_t newSize = (currentFooter -> block_size) << 4;
                            // start of the list is now the second element
                        if (seg_free_list[indexOf].head != NULL){
                            seg_free_list[indexOf].head = seg_free_list[indexOf].head -> next;
                        }
                        // update its pointers
                        if (seg_free_list[indexOf].head != NULL){
                            seg_free_list[indexOf].head -> prev = NULL;
                        }

                        if((newSize <= (size_t)LIST_1_MAX) == 1 && newSize >= LIST_1_MIN){
                            // and now add this list to its correct location
                            next = seg_free_list[0].head;


                            seg_free_list[0].head = (void*)currentHeader;
                            seg_free_list[0].head -> next = next;
                            sf_free_header *opHeader = seg_free_list[0].head -> next;

                            if (opHeader != NULL){
                                opHeader ->prev = seg_free_list[0].head;
                            }
                            seg_free_list[0].head -> prev = NULL;
                        }
                        else if((newSize <= (size_t)LIST_2_MAX) == 1 && newSize >= LIST_2_MIN){
                            next = seg_free_list[1].head;


                            seg_free_list[1].head = (void*)currentHeader;
                            seg_free_list[1].head -> next = next;
                            sf_free_header *opHeader = seg_free_list[1].head -> next;
                            if (opHeader != NULL){
                                opHeader ->prev = seg_free_list[1].head;
                            }

                            seg_free_list[1].head -> prev = NULL;
                        }
                        else if((newSize <= (size_t) LIST_3_MAX) == 1 && newSize >= LIST_3_MIN){
                            next = seg_free_list[2].head;


                            seg_free_list[2].head = (void*)currentHeader;
                            seg_free_list[2].head -> next = next;
                            sf_free_header *opHeader = seg_free_list[2].head -> next;
                            if (opHeader != NULL){
                                opHeader ->prev = seg_free_list[2].head;
                            }

                            seg_free_list[2].head -> prev = NULL;
                        }
                        else{
                            next = seg_free_list[3].head;


                            seg_free_list[3].head = (void*)currentHeader;
                            seg_free_list[3].head -> next = next;
                            sf_free_header *opHeader = seg_free_list[3].head -> next;
                            if (opHeader != NULL){
                                opHeader ->prev = seg_free_list[3].head;
                            }

                            seg_free_list[3].head -> prev = NULL;;
                        }
                            return returnVal;
                        }
                    }
                }
            // sf_snapshot();
            }
        }
    }

    // so none of the lists can fit it. Request more space from sbrk
    // printf("no space!!!!!!\n");

    void* currentEnd = get_heap_end();
    sf_footer *tempFoot = (sf_footer*)currentEnd;

    void *ftr_ptr = (void*)tempFoot;
    void *footerAddress = ftr_ptr -8;

    sf_footer *footer = (sf_footer*)footerAddress;

    int blockSize = 0;
    if ((footer -> allocated) == 1){
        blockSize = 0;
    }
    else{
        blockSize = footer -> block_size <<4;
    }

    // request a page
    void* newPage = sf_sbrk();

    // check if we can actually request
    if (newPage == (void*) - 1){
        sf_errno = ENOMEM;
        return NULL;
    }

    // so we can allocate this page. Coalese backwards
    // if there was nothing left in the old page, just assign this page
    if (blockSize == 0){
        // right now break1 points to the start of the heap, aka the start of the first block.
        // so the size is valid. set the header and footer and try blockprinting it
        sf_free_header *headerL = (sf_free_header*)newPage;

        sf_header *hdr =(sf_header*)newPage;

        headerL -> next = NULL;
        headerL -> prev = NULL;

        hdr -> allocated = 0;                 // not allocated
        hdr -> padded = 0;                    // no padding on first page
        hdr -> two_zeroes = 0;                // two zeros should be 0...
        hdr -> block_size = PAGE_SZ >> 4;     // block size is payload + header +  footer = 4096. Shift left to store properly
        hdr -> unused = 0;                    // unused bits, zero them out for fun ;)

        // and now to set the footer
        // get the footer address by adding block size to header address and subtracting 8
        // first get a pointer to the pointer
        void *ftr_ptr = (void*) hdr;
        // if i want hdr back,then opt = (tcp_option_t *) ptr;
        // https://stackoverflow.com/questions/16578054/how-to-make-a-pointer-increment-by-1-byte-not-1-unit

        void *footerAddress = ftr_ptr + (hdr -> block_size << 4) - 8;

        // printf("Heap Start: %p\n", get_heap_start());
        // printf("Header:     %p\n", hdr);
        // printf("Footer:     %p\n", footerAddress);
        // printf("Heap end:   %p\n", get_heap_end());


        sf_footer *footer = (sf_footer*)footerAddress;

        footer -> allocated = 0;
        footer -> padded = 0;
        footer -> two_zeroes = 0;
        footer -> block_size = PAGE_SZ >> 4;
        footer -> requested_size = 0;

        // the block has been prepared. Add it to the correct list (4th list)
        if (((size_t)PAGE_SZ <= (size_t)seg_free_list[0].max) == 1 && PAGE_SZ >= seg_free_list[0].min){
            sf_free_header *oldHead = (sf_free_header*)seg_free_list[0].head;
            oldHead -> prev = newPage;
            seg_free_list[0].head = newPage;
            seg_free_list[0].head -> next = oldHead;
        }
        else if (((size_t)PAGE_SZ <= (size_t)seg_free_list[1].max) == 1 && PAGE_SZ >= seg_free_list[1].min){
            sf_free_header *oldHead = (sf_free_header*)seg_free_list[1].head;
            oldHead -> prev = newPage;
            seg_free_list[1].head = newPage;
            seg_free_list[1].head -> next = oldHead;
        }
        else if (((size_t)PAGE_SZ <= (size_t)seg_free_list[2].max) == 1 && PAGE_SZ >= seg_free_list[2].min){
            sf_free_header *oldHead = (sf_free_header*)seg_free_list[2].head;
            oldHead -> prev = newPage;
            seg_free_list[2].head = newPage;
            seg_free_list[2].head -> next = oldHead;
        }
        else{
            sf_free_header *oldHead = (sf_free_header*)seg_free_list[3].head;
            oldHead -> prev = newPage;
            seg_free_list[3].head = newPage;
            seg_free_list[3].head -> next = oldHead;
        }

    }

    // otherwise there is a piece of the page left from before. coalese with it
    else{
        void *oldHeaderAddress = ftr_ptr - blockSize;
        sf_header *oldHeader = (sf_header*)oldHeaderAddress;

        oldHeader -> allocated = 0;
        oldHeader -> two_zeroes = 0;
        oldHeader -> padded = 0;
        oldHeader -> block_size = (blockSize + PAGE_SZ) >> 4;
        oldHeader -> unused = 0;

        // and now to set the footer of the new page
        void *newFooterAddress = ftr_ptr + PAGE_SZ - 8;
        sf_footer *newFooter = (sf_footer*) newFooterAddress;

        // printf("newfoooteraddress: %p\n", newFooter);
        // printf("heap end         : %p\n", get_heap_end());
        // printf("heap start       : %p\n", get_heap_start());
        // printf("oldheadeaddress  : %p\n", oldHeaderAddress);
        // printf("blocksize:       : %i\n", oldHeader -> block_size <<4);

        newFooter -> allocated = 0;
        newFooter -> padded = 0;
        newFooter -> two_zeroes = 0;
        newFooter -> block_size = (blockSize + PAGE_SZ) >> 4;
        int mySize = newFooter -> block_size << 4;
        newFooter -> requested_size = 0;

        // and now remove the old block from the list
        // loop through list, look for address that matches oldHeader
        sf_free_header *freeHeader = (sf_free_header*) oldHeaderAddress;
        void* previ = freeHeader -> prev;
        void* nexti = freeHeader -> next;
        // printf("prev: %p\n", prev);
        // printf("next: %p\n", next);

        // if there was a previous, set its reference to next
        if (previ != NULL){
            sf_free_header *prevHeader = previ;
            prevHeader -> next = nexti;
        }

        if (nexti != NULL){
            sf_free_header *nextHeader = nexti;
            nextHeader -> prev = previ;
        }

        for (int i = 0; i < 4; i++){
            if (seg_free_list[i].head == freeHeader){
                seg_free_list[i].head = seg_free_list[i].head -> next;

                if (seg_free_list[i].head != NULL){
                seg_free_list[i].head -> prev = NULL;
                }
            }
        }

        // and now add this current header to an appropriate list
        // the header before it and after it have already been linked
        // we can set this next and previous to null
        sf_free_header *thisHeader = (sf_free_header *) oldHeaderAddress;
        // printf("thisheader %p\n", thisHeader);
        // printf("previous:  %p\n", thisHeader -> prev);
        // printf("next    :  %p\n", thisHeader -> next);
        sf_free_header *previous = thisHeader -> prev;
        sf_free_header *nextous = thisHeader -> next;


        if (thisHeader -> next != NULL){
            thisHeader -> next -> prev = previous;
        }

        if (thisHeader -> prev != NULL){
            thisHeader -> prev -> next = nextous;
        }

        thisHeader -> prev = NULL;
        thisHeader -> next = NULL;

        // lastly, add this header to the correct list
        if (mySize >= LIST_1_MIN && ((size_t)mySize <= (size_t)LIST_1_MAX) == 1){
            seg_free_list[0].head = thisHeader;
        }

        else if (mySize >= LIST_2_MIN && ((size_t)mySize <= (size_t)LIST_2_MAX) == 1){
            seg_free_list[1].head = thisHeader;
        }

        else if (mySize >= LIST_3_MIN && ((size_t)mySize <= (size_t)LIST_3_MAX) == 1){
            seg_free_list[2].head = thisHeader;
        }

        else{
            seg_free_list[3].head = thisHeader;
        }
    }
    return sf_malloc(size);
    // printf("block sie: %i\n", blockSize);
    // printf("heap end : %p\n", get_heap_end());
    // printf("Footer at: %p\n", footerAddress);
    // printf("Allocated: %i\n", footer -> allocated);
    // printf("padded   : %i\n", footer -> padded);
    // printf("two zeros: %i\n", footer -> two_zeroes);
    // printf("blocksize: %i\n", footer -> block_size << 4);
    // printf("request s: %i\n", footer -> requested_size);
    //return(sf_malloc(size));
    return NULL;
}

void *sf_realloc(void *ptr, size_t size) {
    // for realloc, first free the block, then malloc it for the requested size and return
    if (ptr == NULL){
        return NULL;
    }

    void* pointercounter = (char*) ptr;
    void* headerStart = pointercounter - 8;

    // abort if header is before heap start
    if (headerStart < get_heap_start()){
        return NULL;
    }


    sf_header *header = (sf_header*)headerStart;

    // if it was already free, then abort
    if (header -> allocated == 0){
        return NULL;
    }

    // go to footer
    void* footerStart = pointercounter -8 + (header -> block_size <<4) - 8;
    sf_footer *footer = (sf_footer*)footerStart;

    // printf("footer size: %i\n", footer -> block_size << 4);
    // printf("footer alloc:%i\n", footer -> allocated);

    if (footerStart > get_heap_end()){
        return NULL;
    }

    if (footer -> allocated == 0){
        return NULL;
    }



    // if the block size is not requested size + 16, then there must be a padding
    if ((footer -> block_size << 4) != (footer -> requested_size) + 16){
        // if there was no padding, then abort
        if (footer -> padded == 0){
            return NULL;
        }
    }

    // and now check to see if the padded bits are same for both
    if (header -> padded != footer -> padded){
        return NULL;
    }
    if (header -> block_size != footer -> block_size){
        return NULL;
    }

    int alignedSize = size;
    int padding = 0;
    while (alignedSize %16 !=0){
        alignedSize +=1;
        padding +=1;
    }


    // check if we can fit the requested size inside of our current malloc'd area
    if ((header -> block_size <<4) - (alignedSize + 16) < 32 && (header -> block_size << 4) - (alignedSize + 16) >=0){
        footer -> requested_size = size;
        header -> allocated = 1;
        footer -> allocated = 1;
        return ptr;
    }

    // else if the block size - the size we want leaves us with 2 valid blocks and if the sizes are greater than 0, then split into 2 and return
    else if ((header -> block_size << 4) - (alignedSize + 16) >= 32 && (alignedSize + 16) < (header->block_size <<4)){

        header -> allocated = 1;
        header -> two_zeroes = 0;
        if (padding > 0){
            header -> padded = 1;
        }
        else{
            header -> padded = 0;
        }
        int originalSize = header -> block_size <<4;
        header -> block_size = (alignedSize + 16) >> 4;
        header -> unused = 0;

        // and now set the footer
        footer = pointercounter + (header -> block_size <<4) - 16;
        footer -> allocated = 1;
        footer -> two_zeroes = 0;
        if (padding > 0){
            footer ->padded = 1;
        }
        else{
            footer -> padded = 0;
        }
        footer -> block_size = (alignedSize + 16) >> 4;
        footer -> requested_size = size;
        // and now for the new block.
        // since its coming from an allocated block, we dont need to
        // move around pointers for before and after
        // just slap on a header and footer, and insert it at head
        void* nextBlockHeaderAddress = pointercounter + (footer -> block_size << 4) -8;
        sf_header *nextHeader = (sf_header*)nextBlockHeaderAddress;

        nextHeader -> allocated = 1;
        nextHeader -> two_zeroes = 0;
        nextHeader -> block_size = (originalSize - (header -> block_size << 4))>>4;
        nextHeader -> padded = 0;
        nextHeader -> unused = 0;

        // printf("\n##############\n###### %i ########\n###########\n", footer -> block_size <<4);
        void *nextBlockFooterAddress = pointercounter + (footer -> block_size <<4) + (nextHeader -> block_size <<4) - 16;
        sf_footer *nextFooter = (sf_footer*) nextBlockFooterAddress;

        nextFooter -> allocated = 1;
        nextFooter -> two_zeroes = 0;
        nextFooter -> block_size = nextHeader -> block_size;
        nextFooter -> padded = 0;
        nextFooter -> requested_size = (nextFooter -> block_size << 4) - 16;

        // sf_snapshot();
        // and now free that new split block
        sf_free(nextBlockHeaderAddress + 8);
        // sf_snapshot();
        // and then return the pointer
        return ptr;
    }

    else{
        // else we cant split, so malloc to a different block
        void* returnAddress = sf_malloc(size);

        if (returnAddress == NULL){
            return NULL;
        }

        // else malloc gave us space.
        // copy the old stuff to this new place
        returnAddress = memcpy(returnAddress, ptr, size);

        // free the old pointer
        sf_free(ptr);

        // return the address
        return(returnAddress);
    }
    return NULL;
}

void sf_free(void *ptr) {
    /* just writing a quick brute force free. free the block and add it to list assuming all is valid
    */
    // to free, first check if ptr is null
    if (ptr == NULL){
        abort();
    }

    void* pointercounter = (void*) ptr;
    void* headerStart = pointercounter - 8;

    // abort if header is before heap start or if header is after end of heap
    if (headerStart < get_heap_start() || headerStart > get_heap_end()){
        abort();
    }


    sf_header *header = (sf_header*)headerStart;

    // if it was already free, then abort
    if (header -> allocated == 0){
        abort();
    }
    else {
        header -> allocated = 0;
    }
    int size = header -> block_size << 4;
    // printf("@@@@@@@@@@@@@@@@@@@@@@@ block sent: %i\n", size);

    // go to footer
    void* footerStart = pointercounter -8 + size - 8;
    sf_footer *footer = (sf_footer*)footerStart;

    if (footerStart > get_heap_end() || footerStart < get_heap_start()){
        abort();
    }

    if (footer -> allocated == 0){
        abort();
    }
    else{
        footer -> allocated = 0;
    }

    // if the block size is not requested size + 16, then there must be a padding
    if ((footer -> block_size << 4) != footer -> requested_size + 16){
        // if there was no padding, then abort
        if (footer -> padded == 0){
            abort();
        }
    }

    // and now check to see if the padded bits are same for both
    if (header -> padded != footer -> padded){
        abort();
    }
    if (header -> block_size != footer -> block_size){
        abort();
    }

    // otherwise, header and footer have same padded, same size, and not allocated
    header -> allocated = 0;
    header -> two_zeroes = 0;
    header -> padded = 0;

    footer -> allocated = 0;
    footer -> padded = 0;

    // check if we can coalese with the block in front.
    void* nextBlockAddress = pointercounter - 8 + size;

    // if its not the end of the heap, then there is a potential for coalese
    if (nextBlockAddress != get_heap_end()){
        sf_free_header *nextFreeHeader = (sf_free_header*)nextBlockAddress;
        sf_header *nextBlockHeader = (sf_header*)nextBlockAddress;
        if (nextBlockHeader -> allocated == 0){
            // printf("DPMT EEWJ A;EK REA\n");
            nextBlockHeader -> allocated = 0;

            int nextblocksize = nextBlockHeader -> block_size << 4;
            void* next = nextFreeHeader -> next;
            void* prev = nextFreeHeader -> prev;

            if (next != NULL){
                sf_free_header *nHead = next;
                nHead -> prev = prev;
            }

            if (prev != NULL){
                sf_free_header *pHead = prev;
                pHead -> next = next;
            }

            for (int i = 0; i < 4; i++){
                if ((sf_free_header*)seg_free_list[i].head == (sf_free_header*)nextBlockHeader){
                    seg_free_list[i].head = seg_free_list[i].head -> next;

                    if (seg_free_list[i].head != NULL){
                        seg_free_list[i].head -> prev = NULL;
                    }
                }
            }
            // and now update header/footer sizes and add to list
            header -> block_size = (size + nextblocksize) >> 4;
            header -> two_zeroes = 0;
            // now go to the end of this block
            void *nextBlockFooterAddress = pointercounter -8 + size + nextblocksize -8;
            sf_footer *nextFooter = (sf_footer*)nextBlockFooterAddress;

            nextFooter -> allocated = 0;
            nextFooter -> two_zeroes = 0;
            nextFooter -> padded = 0;
            nextFooter -> block_size = (size + nextblocksize) >> 4;

            int mySize = size + nextblocksize;

            // printf("\n\n\n\n\n\nMY SIZE %i\n\n\n\n\n", mySize);
            // and now add the block to the correct list
            if (mySize >= LIST_1_MIN && ((size_t)mySize <= (size_t)LIST_1_MAX) == 1){
                sf_free_header *currHead = (sf_free_header*)seg_free_list[0].head;  //old head
                seg_free_list[0].head = (sf_free_header*)header;                    // new head

                if (currHead != NULL){
                    currHead -> prev = seg_free_list[0].head;                       // point old head to new head
                }
                seg_free_list[0].head ->next = currHead;                            // point new head to old head.
                seg_free_list[0].head = (sf_free_header*)header;
            }

            else if (mySize >= LIST_2_MIN && ((size_t)mySize <= (size_t)LIST_2_MAX) == 1){
                sf_free_header *currHead = (sf_free_header*)seg_free_list[1].head;  //old head
                seg_free_list[1].head = (sf_free_header*)header;                    // new head

                if (currHead != NULL){
                    currHead -> prev = seg_free_list[0].head;                       // point old head to new head
                }
                seg_free_list[1].head ->next = currHead;                            // point new head to old head.
                seg_free_list[1].head = (sf_free_header*)header;
            }

            else if (mySize >= LIST_3_MIN && ((size_t)mySize <= (size_t)LIST_3_MAX) == 1){
                sf_free_header *currHead = (sf_free_header*)seg_free_list[2].head;  //old head
                seg_free_list[2].head = (sf_free_header*)header;                    // new head

                if (currHead != NULL){
                    currHead -> prev = seg_free_list[0].head;                       // point old head to new head
                }
                seg_free_list[2].head ->next = currHead;                            // point new head to old head.
                seg_free_list[2].head = (sf_free_header*)header;
            }

            else{
                sf_free_header *currHead = (sf_free_header*)seg_free_list[3].head;  //old head
                seg_free_list[3].head = (sf_free_header*)header;                    // new head

                if (currHead != NULL){
                    currHead -> prev = seg_free_list[0].head;                       // point old head to new head
                }
                seg_free_list[3].head ->next = currHead;                            // point new head to old head.
                seg_free_list[3].head = (sf_free_header*)header;
            }

            return;
        }
    }

    // so the next address was at the end of the heap, or the next address was allocated
    // simply update the current header and footer, and put it at the front of the correct list
    // printf("and now this case\n\n\n\n\n\n");
    header -> allocated = 0;
    header -> padded = 0;
    header -> two_zeroes = 0;
    header -> block_size = header -> block_size;
    header -> unused = 0;

    footer -> allocated = 0;
    footer -> padded = 0;
    footer -> two_zeroes = 0;
    footer -> block_size = footer -> block_size;
    footer -> requested_size = 0;

    // and now add it to the correct list
    sf_free_header *currentFree = (sf_free_header*) header;
    void *next = currentFree -> next;
    void* prev = currentFree -> prev;

    if (next != NULL){
        sf_free_header *nFree = next;
        nFree -> prev = prev;
    }

    if (prev != NULL){
        sf_free_header *pFree = prev;
        pFree -> next = next;
    }

    int mySize = footer -> block_size <<4;
    if (mySize >= LIST_1_MIN && ((size_t)mySize <= (size_t)LIST_1_MAX) == 1){
    sf_free_header *currHead = (sf_free_header*)seg_free_list[0].head;  //old head
    seg_free_list[0].head = (sf_free_header*)header;                    // new head

    if (currHead != NULL){
        currHead -> prev = seg_free_list[0].head;                       // point old head to new head
    }
    seg_free_list[0].head ->next = currHead;                            // point new head to old head.
    seg_free_list[0].head = (sf_free_header*)header;
}

else if (mySize >= LIST_2_MIN && ((size_t)mySize <= (size_t)LIST_2_MAX) == 1){
    sf_free_header *currHead = (sf_free_header*)seg_free_list[1].head;  //old head
    seg_free_list[1].head = (sf_free_header*)header;                    // new head

    if (currHead != NULL){
        currHead -> prev = seg_free_list[0].head;                       // point old head to new head
    }
    seg_free_list[1].head ->next = currHead;                            // point new head to old head.
    seg_free_list[1].head = (sf_free_header*)header;
}

else if (mySize >= LIST_3_MIN && ((size_t)mySize <= (size_t)LIST_3_MAX) == 1){
    sf_free_header *currHead = (sf_free_header*)seg_free_list[2].head;  //old head
    seg_free_list[2].head = (sf_free_header*)header;                    // new head

    if (currHead != NULL){
        currHead -> prev = seg_free_list[0].head;                       // point old head to new head
    }
    seg_free_list[2].head ->next = currHead;                            // point new head to old head.
    seg_free_list[2].head = (sf_free_header*)header;
}

else{
    sf_free_header *currHead = (sf_free_header*)seg_free_list[3].head;  //old head
    seg_free_list[3].head = (sf_free_header*)header;                    // new head

    if (currHead != NULL){
        currHead -> prev = seg_free_list[0].head;                       // point old head to new head
    }
    seg_free_list[3].head ->next = currHead;                            // point new head to old head.
    seg_free_list[3].head = (sf_free_header*)header;
}

    return;
}