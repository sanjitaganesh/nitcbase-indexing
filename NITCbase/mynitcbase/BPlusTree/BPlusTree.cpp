#include "BPlusTree.h"
#include <cstdio>
#include <cstring>

inline bool operator == (RecId lhs, RecId rhs) {
	return (lhs.block == rhs.block && lhs.slot == rhs.slot);
}

inline bool operator != (RecId lhs, RecId rhs) {
	return (lhs.block != rhs.block || lhs.slot != rhs.slot);
}

inline bool operator == (IndexId lhs, IndexId rhs) {
	return (lhs.block == rhs.block && lhs.index == rhs.index);
}

inline bool operator != (IndexId lhs, IndexId rhs) {
	return (lhs.block != rhs.block || lhs.index != rhs.index);
}

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], 
                                Attribute attrVal, int op) 
{
    IndexId searchIndex;
    int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    int block = -1, index = -1;

    if (searchIndex == IndexId{-1, -1})
    {
        block = attrCatEntry.rootBlock;
        index = 0;

        if (block == -1)
            return RecId{-1, -1};
    } 
    else 
    {
        block = searchIndex.block, index = searchIndex.index + 1;

        IndLeaf leaf(block);
        HeadInfo leafHead;
        leaf.getHeader(&leafHead);

        if (index >= leafHead.numEntries) {
            block = leafHead.rblock, index = 0;

            if (block == -1) {
                return RecId{-1, -1};
            }
        }
    }

    while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL)
    {  
        IndInternal internalBlk(block);
        HeadInfo intHead;
        internalBlk.getHeader(&intHead);

        InternalEntry intEntry;

        if (op == NE || op == LT || op == LE) 
        {
            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;
        } 
        else 
        {
            int entryindex = 0;
            while (entryindex < intHead.numEntries)
            {
                ret = internalBlk.getEntry(&intEntry, entryindex);
                
                int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
                if (
                    (op == EQ && cmpVal >= 0) ||
                    (op == GE && cmpVal >= 0) ||
                    (op == GT && cmpVal > 0)
                )
                    break;

                entryindex++;
            }

            if (entryindex < intHead.numEntries)
            {
                block = intEntry.lChild;
            }
            else 
            {
                block = intEntry.rChild;
            }
        }
    }

    while (block != -1) {
        IndLeaf leafBlk(block);
        HeadInfo leafHead;
        leafBlk.getHeader(&leafHead);

        Index leafEntry;

        while (index < leafHead.numEntries)
        {
            leafBlk.getEntry(&leafEntry, index);

            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType); 

            if (
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == NE && cmpVal != 0)
            ) {
                searchIndex = IndexId{block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);

                return RecId{leafEntry.block, leafEntry.slot};
            } 
            else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) 
            {
                return RecId {-1, -1};
            }

            ++index;
        }

        if (op != NE) {
            break;
        }

        block = leafHead.rblock, index = 0;
    }

    return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) 
{
    // system relations cannot have indexes created on them
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
        return E_NOTPERMITTED;

    // fetch the attribute's cache entry to check if an index already exists
    AttrCatEntry attrCatEntryBuffer;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);
    if (ret != SUCCESS) return ret;

    // if rootBlock != -1, a B+ tree already exists on this attribute — nothing to do
    if (attrCatEntryBuffer.rootBlock != -1)
        return SUCCESS;

    // allocate a new leaf block to serve as the initial root of the B+ tree
    IndLeaf rootBlockBuf;
    int rootBlock = rootBlockBuf.getBlockNum();
    printf("Intial block when created: %d\n",rootBlock);

    // if no free disk block is available, abort
    if (rootBlock == E_DISKFULL) return E_DISKFULL;

    // update rootBlock in the attribute cache — marks the tree as now existing
    attrCatEntryBuffer.rootBlock = rootBlock;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    // fetch relation cache entry to get firstBlk and record layout info
    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(relId, &relCatEntryBuffer);

    // traverse all record blocks of the relation from firstBlk to last
    int block = relCatEntryBuffer.firstBlk;
    while (block != -1) {
        RecBuffer blockBuffer(block);

        // read the slot map to know which slots have valid records
        unsigned char slotmap[relCatEntryBuffer.numSlotsPerBlk];
        blockBuffer.getSlotMap(slotmap);

        for (int slot = 0; slot < relCatEntryBuffer.numSlotsPerBlk; slot++)
        {
            if (slotmap[slot] == SLOT_OCCUPIED)
            {
                // read the record and insert only the indexed attribute's value into the tree
                Attribute record[relCatEntryBuffer.numAttrs];
                blockBuffer.getRecord(record, slot);

                RecId recId = RecId{block, slot};

                // offset gives the position of this attribute in the record array
                ret = bPlusInsert(relId, attrName, 
                                    record[attrCatEntryBuffer.offset], recId);

                if (ret == E_DISKFULL) return E_DISKFULL;
            }
        }

        // move to the next record block via the rblock pointer in the header
        HeadInfo blockHeader;
        blockBuffer.getHeader(&blockHeader);
        block = blockHeader.rblock;
    }

    return SUCCESS;
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], 
                            Attribute attrVal, RecId recId) {
    // fetch attribute cache entry to get rootBlock and attrType
    AttrCatEntry attrCatEntryBuffer;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);
    if (ret != SUCCESS) return ret;

    int rootBlock = attrCatEntryBuffer.rootBlock;

    // no index exists on this attribute
    if (rootBlock == -1) return E_NOINDEX;

    // traverse from root down to the appropriate leaf block for this value
    int leafBlkNum = findLeafToInsert(rootBlock, attrVal, attrCatEntryBuffer.attrType);
    printf("Initial block num while inserting:%d\n",leafBlkNum);
    // build the Index entry: key + pointer to the actual record (block, slot)
    Index indexEntry; 
    indexEntry.attrVal = attrVal;
    indexEntry.block = recId.block;
    indexEntry.slot = recId.slot;
    
    if (insertIntoLeaf(relId, attrName, leafBlkNum, indexEntry) == E_DISKFULL)
    {
        // if disk is full mid-insert, destroy the entire tree and reset rootBlock
        // leaving a partial tree would corrupt future index operations
        BPlusTree::bPlusDestroy(rootBlock);
        attrCatEntryBuffer.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);
        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
    int blockNum = rootBlock;

    // keep descending until we reach a leaf block
    while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) 
    {  
        IndInternal internalBlock(blockNum);
        HeadInfo blockHeader;
        internalBlock.getHeader(&blockHeader);

        // find the first key in this internal node that is >= attrVal
        int index = 0;
        while (index < blockHeader.numEntries)
        {
            InternalEntry entry;
            internalBlock.getEntry(&entry, index);

            // if attrVal <= entry.attrVal, go left (lChild of this entry)
            if (compareAttrs(attrVal, entry.attrVal, attrType) <= 0)
                break;

            index++;
        }

        if (index == blockHeader.numEntries) 
        {
            // attrVal is greater than all keys — follow the rightmost rChild
            InternalEntry entry;
            internalBlock.getEntry(&entry, blockHeader.numEntries - 1);
            blockNum = entry.rChild;
        } 
        else 
        {
            // follow lChild of the first entry whose key >= attrVal
            InternalEntry entry;
            internalBlock.getEntry(&entry, index);
            blockNum = entry.lChild;
        }
    }
    printf("Block Num after finding leaf to insert:%d\n",blockNum);
    return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], 
                                int leafBlockNum, Index indexEntry) 
{
    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    IndLeaf leafBlock(leafBlockNum);
    HeadInfo blockHeader;
    leafBlock.getHeader(&blockHeader);

    // temporary array holding all existing entries + the new one (sorted)
    Index indices[blockHeader.numEntries + 1];

    // insert indexEntry into the sorted position within indices[]
    bool inserted = false;
    for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++)
    {
        Index entry;
        leafBlock.getEntry(&entry, entryindex);

        if (compareAttrs(entry.attrVal, indexEntry.attrVal, attrCatEntryBuffer.attrType) <= 0)
        {
            indices[entryindex] = entry; // existing entry comes before new one
        }
        else
        {
            indices[entryindex] = indexEntry; // insert new entry here
            inserted = true;

            // shift remaining existing entries one position to the right
            for (entryindex++; entryindex <= blockHeader.numEntries; entryindex++)
            {
                leafBlock.getEntry(&entry, entryindex - 1);
                indices[entryindex] = entry;
            }
            break;
        }
    }

    // if new entry is largest, append at the end
    if (!inserted) indices[blockHeader.numEntries] = indexEntry;

    if (blockHeader.numEntries < MAX_KEYS_LEAF) {
        // leaf has space — just write back the updated sorted entries
        blockHeader.numEntries++;
        leafBlock.setHeader(&blockHeader);

        for (int indicesIt = 0; indicesIt < blockHeader.numEntries; indicesIt++)
            leafBlock.setEntry(&indices[indicesIt], indicesIt);

        return SUCCESS;
    }

    // leaf is full — split it into two leaf blocks
    int newRightBlk = splitLeaf(leafBlockNum, indices);
    if (newRightBlk == E_DISKFULL) return E_DISKFULL;

    if (blockHeader.pblock != -1)
    {
        // leaf has a parent — push the middle key up into the parent internal node
        InternalEntry middleEntry;
        middleEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
        middleEntry.lChild = leafBlockNum;
        middleEntry.rChild = newRightBlk;

        return insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
    } 
    else 
    {
        // leaf was the root — create a new root above the two split halves
        if (createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, 
                            leafBlockNum, newRightBlk) == E_DISKFULL)
            return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
    // allocate a new leaf block for the right half
    IndLeaf rightBlock;
    IndLeaf leftBlock(leafBlockNum);

    int rightBlockNum = rightBlock.getBlockNum();
    int leftBlockNum = leftBlock.getBlockNum();

    if (rightBlockNum == E_DISKFULL) return E_DISKFULL;

    HeadInfo leftBlockHeader, rightBlockHeader;
    leftBlock.getHeader(&leftBlockHeader);
    rightBlock.getHeader(&rightBlockHeader);

    // each half gets ceil((MAX_KEYS_LEAF + 1) / 2) entries
    rightBlockHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rightBlockHeader.pblock = leftBlockHeader.pblock; // same parent as left
    rightBlockHeader.lblock = leftBlockNum;           // right's left neighbor = left block
    rightBlockHeader.rblock = leftBlockHeader.rblock; // right inherits left's old right neighbor
    rightBlock.setHeader(&rightBlockHeader);

    leftBlockHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    leftBlockHeader.rblock = rightBlockNum;           // left's right neighbor = new right block
    leftBlock.setHeader(&leftBlockHeader);

    // distribute entries: [0..MIDDLE] to left, [MIDDLE+1..end] to right
    for (int entryindex = 0; entryindex <= MIDDLE_INDEX_LEAF; entryindex++)
    {
        leftBlock.setEntry(&indices[entryindex], entryindex);
        rightBlock.setEntry(&indices[entryindex + MIDDLE_INDEX_LEAF + 1], entryindex);
    }

    return rightBlockNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], 
                                    int intBlockNum, InternalEntry intEntry) {
    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    IndInternal internalBlock(intBlockNum);
    HeadInfo blockHeader;
    internalBlock.getHeader(&blockHeader);

    // temporary array holding all existing entries + new one (sorted)
    InternalEntry internalEntries[blockHeader.numEntries + 1];

    // insert intEntry into the sorted position within internalEntries[]
    int insertedIndex = -1;
    for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++)
    {
        InternalEntry internalBlockEntry;
        internalBlock.getEntry(&internalBlockEntry, entryindex);

        if (compareAttrs(internalBlockEntry.attrVal, intEntry.attrVal, attrCatEntryBuffer.attrType) <= 0)
        {
            internalEntries[entryindex] = internalBlockEntry;
        }
        else 
        {
            internalEntries[entryindex] = intEntry;
            insertedIndex = entryindex;

            // shift remaining entries right
            for (entryindex++; entryindex <= blockHeader.numEntries; entryindex++)
            {
                internalBlock.getEntry(&internalBlockEntry, entryindex - 1);
                internalEntries[entryindex] = internalBlockEntry;
            }
            break;
        }
    }

    // new entry is largest — append at end
    if (insertedIndex == -1) {
        internalEntries[blockHeader.numEntries] = intEntry;
        insertedIndex = blockHeader.numEntries;
    }

    // fix up the rChild of the entry just before the inserted one
    // to point to intEntry.lChild (the left half of the split that triggered this insert)
    if (insertedIndex > 0)
        internalEntries[insertedIndex - 1].rChild = intEntry.lChild;

    if (blockHeader.numEntries < MAX_KEYS_INTERNAL) {
        // internal block has space — write back updated entries
        blockHeader.numEntries++;
        internalBlock.setHeader(&blockHeader);

        for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++)
            internalBlock.setEntry(&internalEntries[entryindex], entryindex);

        return SUCCESS;
    }

    // internal block is full — split it
    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    if (newRightBlk == E_DISKFULL)
    {
        // if split fails, destroy the rChild block that was just created below
        BPlusTree::bPlusDestroy(intEntry.rChild);
        return E_DISKFULL;
    }

    if (blockHeader.pblock != -1)
    {
        // push middle key up to the parent internal node
        InternalEntry middleEntry;
        middleEntry.lChild = intBlockNum;
        middleEntry.rChild = newRightBlk;
        middleEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;

        return insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
    } 
    else 
    {
        // this internal node was the root — create a new root
        return createNewRoot(relId, attrName, 
                                internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, 
                                intBlockNum, newRightBlk);
    }

    return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) 
{
    // allocate a new internal block for the right half
    IndInternal rightBlock;
    IndInternal leftBlock(intBlockNum);

    int leftBlockNum = leftBlock.getBlockNum();
    int rightBlockNum = rightBlock.getBlockNum();

    if (rightBlockNum == E_DISKFULL) return E_DISKFULL;

    HeadInfo leftBlockHeader, rightBlockHeader;
    leftBlock.getHeader(&leftBlockHeader);
    rightBlock.getHeader(&rightBlockHeader);

    // each half gets MAX_KEYS_INTERNAL / 2 entries
    // the middle entry is pushed up to the parent — it does NOT stay in either half
    rightBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    rightBlockHeader.pblock = leftBlockHeader.pblock;
    rightBlock.setHeader(&rightBlockHeader);

    leftBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    leftBlockHeader.rblock = rightBlockNum;
    leftBlock.setHeader(&leftBlockHeader);

    // distribute: [0..MIDDLE-1] to left, [MIDDLE+1..end] to right
    // the entry at MIDDLE_INDEX_INTERNAL is pushed up, not stored in either block
    for (int entryindex = 0; entryindex < MIDDLE_INDEX_INTERNAL; entryindex++)
    {
        leftBlock.setEntry(&internalEntries[entryindex], entryindex);
        rightBlock.setEntry(&internalEntries[entryindex + MIDDLE_INDEX_INTERNAL + 1], entryindex);
    }

    // update pblock of the leftmost child of the right block
    // (it was previously pointing to the left block's parent)
    BlockBuffer firstRightChild(internalEntries[MIDDLE_INDEX_INTERNAL + 1].lChild);
    HeadInfo childHeader;
    firstRightChild.getHeader(&childHeader);
    childHeader.pblock = rightBlockNum;
    firstRightChild.setHeader(&childHeader);

    // update pblock of all rChild nodes that move to the right block
    for (int entryindex = 0; entryindex < MIDDLE_INDEX_INTERNAL; entryindex++)
    {
        BlockBuffer childBlock(internalEntries[entryindex + MIDDLE_INDEX_INTERNAL + 1].rChild);
        childBlock.getHeader(&childHeader);
        childHeader.pblock = rightBlockNum;
        childBlock.setHeader(&childHeader);
    }

    return rightBlockNum; 
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], 
                                Attribute attrVal, int lChild, int rChild) {
    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    // allocate a new internal block to serve as the new root
    IndInternal newRootBlock;
    int newRootBlkNum = newRootBlock.getBlockNum();

    if (newRootBlkNum == E_DISKFULL) 
    {
        // if we can't allocate a root, destroy the newly split rChild to avoid leaking blocks
        BPlusTree::bPlusDestroy(rChild);
        return E_DISKFULL;
    }

    // new root has exactly one entry with lChild and rChild as its two subtrees
    HeadInfo blockHeader;
    newRootBlock.getHeader(&blockHeader);
    blockHeader.numEntries = 1;
    newRootBlock.setHeader(&blockHeader);

    InternalEntry internalentry;
    internalentry.lChild = lChild;
    internalentry.rChild = rChild;
    internalentry.attrVal = attrVal;
    newRootBlock.setEntry(&internalentry, 0);

    // update pblock of both children to point to the new root
    BlockBuffer leftChildBlock(lChild);
    BlockBuffer rightChildBlock(rChild);

    HeadInfo leftChildHeader, rightChildHeader;
    leftChildBlock.getHeader(&leftChildHeader);
    rightChildBlock.getHeader(&rightChildHeader);

    leftChildHeader.pblock = newRootBlkNum;
    rightChildHeader.pblock = newRootBlkNum;

    leftChildBlock.setHeader(&leftChildHeader);
    rightChildBlock.setHeader(&rightChildHeader);

    // update rootBlock in the attribute cache to point to the new root
    attrCatEntryBuffer.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS)
        return E_OUTOFBOUND;

    int type = StaticBuffer::getStaticBlockType(rootBlockNum);

    if (type == IND_LEAF) 
    {
        // base case: leaf block — just release it
        IndLeaf leafBlock(rootBlockNum);
        leafBlock.releaseBlock();
        return SUCCESS;
    } 
    else if (type == IND_INTERNAL) 
    {
        IndInternal internalBlock(rootBlockNum);
        HeadInfo blockHeader;
        internalBlock.getHeader(&blockHeader);

        // recursively destroy the leftmost child (lChild of first entry)
        InternalEntry blockEntry;
        internalBlock.getEntry(&blockEntry, 0);
        BPlusTree::bPlusDestroy(blockEntry.lChild);

        // recursively destroy all rChild subtrees
        for (int entry = 0; entry < blockHeader.numEntries; entry++) {
            internalBlock.getEntry(&blockEntry, entry);
            BPlusTree::bPlusDestroy(blockEntry.rChild);
        }

        // release this internal block after all children are freed
        internalBlock.releaseBlock();
        return SUCCESS;
    } 
    else 
    {
        return E_INVALIDBLOCK;
    }
}