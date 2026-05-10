#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

// the declarations for these functions can be found in "BlockBuffer.h"

BlockBuffer::BlockBuffer(int blockNum)
{
	// initialise this.blockNum with the argument
	if (blockNum < 0 || blockNum >= DISK_BLOCKS)
		this->blockNum = E_DISKFULL;
	else
		this->blockNum = blockNum;
}

BlockBuffer::BlockBuffer(char blocktypeC){
    // allocate a block on the disk and a buffer in memory to hold the new block of
    // given type using getFreeBlock function and get the return error codes if any.
	int blockType = blocktypeC == 'R' ? REC : 
					blocktypeC == 'I' ? IND_INTERNAL :
					blocktypeC == 'L' ? IND_LEAF : UNUSED_BLK; 

	int blockNum = getFreeBlock(blockType);
	if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
		std::cout << "Error: Block is not available\n";
		this->blockNum = blockNum;
		return;
	}

	this->blockNum = blockNum;
}

//* calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

//* calls parent non-default constructor with 'R' denoting record block.
RecBuffer::RecBuffer() : BlockBuffer('R'){}

//* calls the corresponding parent constructor
IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType){}

//* calls the corresponding parent constructor
IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum){}

//* calls the corresponding parent constructor
//* 'I' used to denote IndInternal.
IndInternal::IndInternal() : IndBuffer('I'){}

//* calls the corresponding parent constructor
IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum){}

//* this is the way to call parent non-default constructor.
//* 'L' used to denote IndLeaf.
IndLeaf::IndLeaf() : IndBuffer('L'){} 

//* this is the way to call parent non-default constructor.
IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum){}

int BlockBuffer::getBlockNum(){
	return this->blockNum;
}

//* loads the block header into the argument pointer
int BlockBuffer::getHeader(HeadInfo *head)
{
	unsigned char *buffer;
	int ret = loadBlockAndGetBufferPtr(&buffer);
	if (ret != SUCCESS)
		return ret;

	memcpy(&head->pblock, buffer + 4, 4);
	memcpy(&head->lblock, buffer + 8, 4);
	memcpy(&head->rblock, buffer + 12, 4);
	memcpy(&head->numEntries, buffer + 16, 4);
	memcpy(&head->numAttrs, buffer + 20, 4);
	memcpy(&head->numSlots, buffer + 24, 4);

	return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head){

    unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);

	if (ret != SUCCESS) return ret;

    struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

	bufferHeader->blockType = head->blockType;
	bufferHeader->lblock = head->lblock;
	bufferHeader->rblock = head->rblock;
	bufferHeader->pblock = head->pblock;
	bufferHeader->numAttrs = head->numAttrs;
	bufferHeader->numEntries = head->numEntries;
	bufferHeader->numSlots = head->numSlots;

	ret = StaticBuffer::setDirtyBit(this->blockNum);

	if (ret != SUCCESS) return ret;

    return SUCCESS;
}

//* loads the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute *record, int slotNum)
{
	HeadInfo head;
	BlockBuffer::getHeader(&head);

	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;

	unsigned char *buffer;
	int ret = loadBlockAndGetBufferPtr(&buffer);
	if (ret != SUCCESS)
		return ret;

	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = buffer + (32 + slotCount + (recordSize * slotNum));

	memcpy(record, slotPointer, recordSize);

	return SUCCESS;
}

//* load the record at slotNum into the argument pointer
int RecBuffer::setRecord(union Attribute *record, int slotNum)
{
	unsigned char *buffer;
	int ret = loadBlockAndGetBufferPtr(&buffer);
	if (ret != SUCCESS)
		return ret;
	
	HeadInfo head;
	BlockBuffer::getHeader(&head);

	int attrCount = head.numAttrs;
	int slotCount = head.numSlots;

	if (slotNum >= slotCount) return E_OUTOFBOUND;

	int recordSize = attrCount * ATTR_SIZE;
	unsigned char *slotPointer = buffer + (HEADER_SIZE + slotCount + (recordSize * slotNum));

	memcpy(slotPointer, record, recordSize);

	ret = StaticBuffer::setDirtyBit(this->blockNum);

	if (ret != SUCCESS) {
		std::cout << "There is some error in the code!\n";
		exit(1);
	}

	return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr)
{
	int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
	if (bufferNum == E_OUTOFBOUND)
		return E_OUTOFBOUND;

	if (bufferNum != E_BLOCKNOTINBUFFER) {
		for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
			StaticBuffer::metainfo[bufferIndex].timeStamp++;
		}
		StaticBuffer::metainfo[bufferNum].timeStamp = 0;
	}
	else if (bufferNum == E_BLOCKNOTINBUFFER)
	{ 
		bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

		if (bufferNum == E_OUTOFBOUND || bufferNum == FAILURE)
			return bufferNum;

		Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
	}

	*buffPtr = StaticBuffer::blocks[bufferNum];

	return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap)
{
	unsigned char *bufferPtr;

	int ret = loadBlockAndGetBufferPtr(&bufferPtr);
	if (ret != SUCCESS)
		return ret;

	RecBuffer recordBlock (this->blockNum);
	struct HeadInfo head;
	recordBlock.getHeader(&head);

	int slotCount = head.numSlots;

	unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

	for (int slot = 0; slot < slotCount; slot++) 
		*(slotMap+slot)= *(slotMapInBuffer+slot);

	return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);

	if (ret != SUCCESS) return ret;

	HeadInfo blockHeader;
	getHeader(&blockHeader);

    int numSlots = blockHeader.numSlots;

	unsigned char *slotPointer = bufferPtr + HEADER_SIZE;
	memcpy(slotPointer, slotMap, numSlots);

	ret = StaticBuffer::setDirtyBit(this->blockNum);

	if (ret != SUCCESS) return ret;

    return SUCCESS;
}

int BlockBuffer::setBlockType(int blockType){

    unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);

	if (ret != SUCCESS) return ret;

	(*(int32_t*) bufferPtr) = blockType;

	StaticBuffer::blockAllocMap[this->blockNum] = blockType;

	ret = StaticBuffer::setDirtyBit(this->blockNum);
	if (ret != SUCCESS) return ret;

    return SUCCESS;
}

int BlockBuffer::getFreeBlock(int blockType){
	int blockNum = 0;
	for (; blockNum < DISK_BLOCKS; blockNum++) {
		if (StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
			break;
	}

	if (blockNum == DISK_BLOCKS) return E_DISKFULL;

	this->blockNum = blockNum;

	int bufferIndex = StaticBuffer::getFreeBuffer(blockNum);

	if (bufferIndex < 0 && bufferIndex >= BUFFER_CAPACITY) {
		printf ("Error: Buffer is full\n");
		return bufferIndex;
	}
	
	HeadInfo blockHeader;
	blockHeader.pblock = blockHeader.rblock = blockHeader.lblock = -1;
	blockHeader.numEntries = blockHeader.numAttrs = blockHeader.numSlots = 0;

	setHeader(&blockHeader);
	setBlockType(blockType);

    return blockNum;
}

void BlockBuffer::releaseBlock()
{
	if (blockNum == INVALID_BLOCKNUM || 
		StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
		return;

	int bufferIndex = StaticBuffer::getBufferNum(blockNum);

	if (bufferIndex >= 0 && bufferIndex < BUFFER_CAPACITY)
		StaticBuffer::metainfo[bufferIndex].free = true;

	StaticBuffer::blockAllocMap[blockNum] = UNUSED_BLK;

	this->blockNum = INVALID_BLOCKNUM;
}

int compareAttrs(Attribute attr1, Attribute attr2, int attrType) {
	return attrType == NUMBER ? 
		(attr1.nVal < attr2.nVal ? -1 : (attr1.nVal > attr2.nVal ? 1 : 0)) : 
		strcmp(attr1.sVal, attr2.sVal) ;
}

int IndInternal::getEntry(void *ptr, int indexNum) {
	if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL) return E_OUTOFBOUND;

    unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);

	if (ret != SUCCESS) return ret;

    struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

    memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
    memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
    memcpy(&(internalEntry->rChild), entryPtr + 20, 4);

    return SUCCESS;
}

int IndLeaf::getEntry(void *ptr, int indexNum) 
{
	if (indexNum < 0 || indexNum >= MAX_KEYS_LEAF) return E_OUTOFBOUND;

    unsigned char *bufferPtr;
	int ret = loadBlockAndGetBufferPtr(&bufferPtr);

	if (ret != SUCCESS) return ret;

	unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
    memcpy((struct Index *)ptr, entryPtr, LEAF_ENTRY_SIZE);

    return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum) {
    // validate indexNum is within bounds of an internal index block
    // internal blocks hold at most MAX_KEYS_INTERNAL entries
    if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL) 
        return E_OUTOFBOUND;

    // load the block into the buffer pool and get a pointer to its raw bytes
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) return ret;

    // cast the void* input to InternalEntry to access lChild, attrVal, rChild
    struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

    // calculate the byte offset of this entry within the block
    // layout: [HEADER | entry_0 | entry_1 | ... ] each internal entry is 20 bytes
    // (4 bytes lChild + 16 bytes attrVal + 4 bytes rChild, but rChild is shared
    //  with the next entry's lChild — so only lChild + attrVal = 20 bytes per entry,
    //  and the final rChild is written at offset +20 from the last entry's start)
    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

    // write lChild (4 bytes) — block number of the left child
    memcpy(entryPtr, &(internalEntry->lChild), sizeof(int32_t));

    // write attrVal (16 bytes) — the key value that separates left and right subtrees
    memcpy(entryPtr + 4, &(internalEntry->attrVal), sizeof(Attribute));

    // write rChild (4 bytes) at offset +20 — block number of the right child
    memcpy(entryPtr + 20, &(internalEntry->rChild), 4);

    // mark the buffer block as dirty so it gets written back to disk on eviction
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if (ret != SUCCESS) return ret;

    return SUCCESS;
}

int IndLeaf::setEntry(void *ptr, int indexNum) {
    // validate indexNum is within bounds of a leaf index block
    // leaf blocks hold at most MAX_KEYS_LEAF entries
    if (indexNum < 0 || indexNum >= MAX_KEYS_LEAF) 
        return E_OUTOFBOUND;

    // load the block into the buffer pool and get a pointer to its raw bytes
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) return ret;

    // calculate the byte offset of this entry within the block
    // each leaf entry is LEAF_ENTRY_SIZE bytes (attrVal + block + slot = 16 + 4 + 4 = 24)
    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);

    // copy the entire Index struct directly — leaf entries have a flat uniform layout
    // so a single memcpy of LEAF_ENTRY_SIZE bytes is sufficient
    memcpy(entryPtr, (struct Index *)ptr, LEAF_ENTRY_SIZE);

    // mark the buffer block as dirty so it gets written back to disk on eviction
    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if (ret != SUCCESS) return ret;

    return SUCCESS;
}