#include "OpenRelTable.h"

#include <cstring>
#include <stdlib.h>
#include <stdio.h>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

inline bool operator == (const RecId& lhs, const RecId& rhs)
{
    return (lhs.block == rhs.block && lhs.slot == rhs.slot);
}

AttrCacheEntry *createAttrCacheEntryList(int size)
{
	AttrCacheEntry *head = nullptr, *curr = nullptr;
	head = curr = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
	size--;
	while (size--)
	{
		curr->next = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
		curr = curr->next;
	}
	curr->next = nullptr;

	return head;
}

OpenRelTable::OpenRelTable()
{
	// initialise all values in relCache and attrCache to be 
	// nullptr and all entries in tableMetaInfo to be free
	for (int i = 0; i < MAX_OPEN; ++i)
	{
		RelCacheTable::relCache[i] = nullptr;
		AttrCacheTable::attrCache[i] = nullptr;
		tableMetaInfo[i].free = true;
	}

	// setting up the variables
	RecBuffer relCatBlock(RELCAT_BLOCK);
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	RelCacheEntry *relCacheEntry = nullptr;

	for (int relId = RELCAT_RELID; relId <= ATTRCAT_RELID; relId++)
	{
		relCatBlock.getRecord(relCatRecord, relId);

		relCacheEntry = (RelCacheEntry *)malloc(sizeof(RelCacheEntry));
		RelCacheTable::recordToRelCatEntry(relCatRecord, &(relCacheEntry->relCatEntry));
		relCacheEntry->recId.block = RELCAT_BLOCK;
		relCacheEntry->recId.slot = relId;
		relCacheEntry->dirty = false;           // FIX: explicit init
		relCacheEntry->searchIndex = {-1, -1};  // FIX: explicit init

		RelCacheTable::relCache[relId] = relCacheEntry;
	}

	// setting up the variables
	RecBuffer attrCatBlock(ATTRCAT_BLOCK);
	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
	AttrCacheEntry *attrCacheEntry = nullptr, *head = nullptr;

	for (int relId = RELCAT_RELID, recordId = 0; relId <= ATTRCAT_RELID; relId++)
	{
		int numberOfAttributes = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
		head = createAttrCacheEntryList(numberOfAttributes);
		attrCacheEntry = head;

		while (numberOfAttributes--)
		{
			attrCatBlock.getRecord(attrCatRecord, recordId);

			AttrCacheTable::recordToAttrCatEntry(
				attrCatRecord,
				&(attrCacheEntry->attrCatEntry));
			attrCacheEntry->recId.slot = recordId++;
			attrCacheEntry->recId.block = ATTRCAT_BLOCK;
			attrCacheEntry->dirty = false;           // FIX: explicit init
			attrCacheEntry->searchIndex = {-1, -1};  // FIX: explicit init

			attrCacheEntry = attrCacheEntry->next;
		}

		AttrCacheTable::attrCache[relId] = head;
	}

	/************ Setting up tableMetaInfo entries ************/

	tableMetaInfo[RELCAT_RELID].free = false; 
	strcpy(tableMetaInfo[RELCAT_RELID].relName, RELCAT_RELNAME);

	tableMetaInfo[ATTRCAT_RELID].free = false; 
	strcpy(tableMetaInfo[ATTRCAT_RELID].relName, ATTRCAT_RELNAME);
}

OpenRelTable::~OpenRelTable() {

	for (int relId = 2; relId < MAX_OPEN; relId++)
    {
		if (OpenRelTable::tableMetaInfo[relId].free == false)
			OpenRelTable::closeRel(relId);
    }

	// release the relation cache entry of the attribute catalog
	if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty)
	{
		RelCatEntry relCatBuffer;
		RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatBuffer);

		Attribute relCatRecord[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatBuffer, relCatRecord);

		RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;

		RecBuffer relCatBlock(recId.block);
		relCatBlock.setRecord(relCatRecord, recId.slot);
    }
	free(RelCacheTable::relCache[ATTRCAT_RELID]);

	// release the relation cache entry of the relation catalog
	if (RelCacheTable::relCache[RELCAT_RELID]->dirty)
	{
		RelCatEntry relCatBuffer;
		RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatBuffer);

		Attribute relCatRecord[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatBuffer, relCatRecord);

		RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;

		RecBuffer relCatBlock(recId.block);
		relCatBlock.setRecord(relCatRecord, recId.slot);
    }
	free(RelCacheTable::relCache[RELCAT_RELID]);

	// free the memory allocated for the attribute cache entries of the
	// relation catalog and the attribute catalog
	for (int relId = ATTRCAT_RELID; relId >= RELCAT_RELID; relId--)
	{
		AttrCacheEntry *curr = AttrCacheTable::attrCache[relId], *next = nullptr;
		for (int attrIndex = 0; attrIndex < 6; attrIndex++)
		{
			next = curr->next;

			if (curr->dirty)
			{
				AttrCatEntry attrCatBuffer;
				AttrCacheTable::getAttrCatEntry(relId, attrIndex, &attrCatBuffer);

				Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
				AttrCacheTable::attrCatEntryToRecord(&attrCatBuffer, attrCatRecord);

				RecId recId = curr->recId;

				RecBuffer attrCatBlock(recId.block);
				attrCatBlock.setRecord(attrCatRecord, recId.slot);
			}

			free(curr);
			curr = next;
		}
	}
}

int OpenRelTable::getFreeOpenRelTableEntry()
{
	for (int relId = 0; relId < MAX_OPEN; relId++)
		if (tableMetaInfo[relId].free)
			return relId;

	return E_CACHEFULL;
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) 
{
	for (int relId = 0; relId < MAX_OPEN; relId++) 
		if (strcmp(tableMetaInfo[relId].relName, relName) == 0
			&& tableMetaInfo[relId].free == false)
			return relId;

	return E_RELNOTOPEN;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) 
{
	int relId = getRelId(relName);
  	if (relId >= 0) {
		return relId;
  	}

	relId = OpenRelTable::getFreeOpenRelTableEntry();
  	if (relId == E_CACHEFULL) return E_CACHEFULL;

  	/****** Setting up Relation Cache entry for the relation ******/

	Attribute attrVal; strcpy(attrVal.sVal, relName);
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

  	RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, attrVal, EQ);

	if (relcatRecId == RecId{-1, -1}) {
		return E_RELNOTEXIST;
	}

	RecBuffer relationBuffer(relcatRecId.block);
	Attribute relationRecord[RELCAT_NO_ATTRS];
	relationBuffer.getRecord(relationRecord, relcatRecId.slot);

	RelCacheEntry *relCacheBuffer = (RelCacheEntry *)malloc(sizeof(RelCacheEntry));
	RelCacheTable::recordToRelCatEntry(relationRecord, &(relCacheBuffer->relCatEntry));

	relCacheBuffer->recId.block = relcatRecId.block;
	relCacheBuffer->recId.slot = relcatRecId.slot;
	relCacheBuffer->dirty = false;           // FIX: explicit init
	relCacheBuffer->searchIndex = {-1, -1};  // FIX: explicit init

	RelCacheTable::relCache[relId] = relCacheBuffer;

  	/****** Setting up Attribute Cache entry for the relation ******/

	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
	AttrCacheEntry *attrCacheEntry = nullptr, *head = nullptr;

	int numberOfAttributes = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
	head = createAttrCacheEntryList(numberOfAttributes);
	attrCacheEntry = head;

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	while (numberOfAttributes--)
	{
		RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, RELCAT_ATTR_RELNAME, attrVal, EQ);

		RecBuffer attrCatBlock(attrcatRecId.block);
		attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);

		AttrCacheTable::recordToAttrCatEntry(
			attrCatRecord,
			&(attrCacheEntry->attrCatEntry)
		);

		attrCacheEntry->recId.block = attrcatRecId.block;
		attrCacheEntry->recId.slot = attrcatRecId.slot;
		attrCacheEntry->dirty = false;           // FIX: explicit init
		attrCacheEntry->searchIndex = {-1, -1};  // FIX: explicit init

		attrCacheEntry = attrCacheEntry->next;
	}

	AttrCacheTable::attrCache[relId] = head;

  	/****** Setting up metadata in the Open Relation Table for the relation ******/

	tableMetaInfo[relId].free = false;
	strcpy(tableMetaInfo[relId].relName, relName);

  	return relId;
}

int OpenRelTable::closeRel(int relId) {
    // RELCAT and ATTRCAT are always kept open by NITCbase — closing them is not permitted
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) 
        return E_NOTPERMITTED;

    // validate relId is within bounds of the open relation table
    if (0 > relId || relId >= MAX_OPEN) 
        return E_OUTOFBOUND;

    // if the slot is already free, the relation is not open
    if (tableMetaInfo[relId].free) 
        return E_RELNOTOPEN;

    /* --- STEP 1: flush relation cache if dirty --- */

    if (RelCacheTable::relCache[relId]->dirty == true) {
        // convert the in-memory RelCatEntry struct back to a disk record (Attribute array)
        Attribute relCatBuffer[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(
            &(RelCacheTable::relCache[relId]->relCatEntry), 
            relCatBuffer
        );

        // get the disk location (block, slot) where this relation's catalog record lives
        RecId recId = RelCacheTable::relCache[relId]->recId;
        RecBuffer relCatBlock(recId.block);

        // overwrite that slot in the relation catalog block with the updated record
        relCatBlock.setRecord(relCatBuffer, RelCacheTable::relCache[relId]->recId.slot);
    }

    // free the heap memory allocated for this relation's cache entry
    free(RelCacheTable::relCache[relId]);

    /* --- STEP 2: flush attribute cache entries that are dirty --- */

    // walk the linked list of attribute cache entries for this relation
    AttrCacheEntry *curr = AttrCacheTable::attrCache[relId];
    while (curr != nullptr) {
        AttrCacheEntry *next = curr->next; // save next pointer before freeing curr

        if (curr->dirty) {
            // convert the in-memory AttrCatEntry struct back to a disk record
            Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
            AttrCacheTable::attrCatEntryToRecord(&(curr->attrCatEntry), attrCatRecord);

            // get the block buffer for the attribute catalog block this entry lives in
            RecBuffer attrCatBlockBuffer(curr->recId.block);

            // overwrite that slot with the updated attribute record
            attrCatBlockBuffer.setRecord(attrCatRecord, curr->recId.slot);
        }

        // free this node regardless of whether it was dirty or not
        free(curr);
        curr = next;
    }

    /* --- STEP 3: mark the slot as free in all three tables --- */

    // mark this slot as available in the open relation table
    tableMetaInfo[relId].free = true;

    // null out the cache pointers so stale data is not accidentally accessed
    RelCacheTable::relCache[relId] = nullptr;
    AttrCacheTable::attrCache[relId] = nullptr;

    return SUCCESS;
}