#include "Algebra.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <iostream>

/* used to select all the records that satisfy a condition.
the arguments of the function are
* srcRel - the source relation we want to select from
* targetRel - the relation we want to select into. (ignore for now)
* attr - the attribute that the condition is checking
* op - the operator of the condition
* strVal - the value that we want to compare against (represented as a string)
*/
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], 
                    char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) 
{
    // get the srcRel's rel-id (let it be srcRelid), using OpenRelTable::getRelId()
    // if srcRel is not open in open relation table, return E_RELNOTOPEN
    
    int srcRelId = OpenRelTable::getRelId(srcRel); // we'll implement this later
    if (srcRelId == E_RELNOTOPEN) return E_RELNOTOPEN;

    // get the attr-cat entry for attr, using AttrCacheTable::getAttrCatEntry()
    // if getAttrcatEntry() call fails return E_ATTRNOTEXIST
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);

    if (ret == E_ATTRNOTEXIST) return E_ATTRNOTEXIST;

    /*** Convert strVal to an attribute of data type NUMBER or STRING ***/

    // TODO: Convert strVal (string) to an attribute of data type NUMBER or STRING 
    int type = attrCatEntry.attrType;
    Attribute attrVal;
    if (type == NUMBER)
    {
        if (isNumber(strVal)) // the isNumber() function is implemented below
            attrVal.nVal = atof(strVal);
        else
            return E_ATTRTYPEMISMATCH;
    }
    else if (type == STRING)
        strcpy(attrVal.sVal, strVal);

    /*** Creating and opening the target relation ***/
    // Prepare arguments for createRel() in the following way:
    // get RelcatEntry of srcRel using RelCacheTable::getRelCatEntry()
    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntryBuffer);

    int srcNoAttrs =  relCatEntryBuffer.numAttrs;

    /* let attr_names[src_nAttrs][ATTR_SIZE] be a 2D array of type char
        (will store the attribute names of rel). */
    char srcAttrNames [srcNoAttrs][ATTR_SIZE];

    // let attr_types[src_nAttrs] be an array of type int
    int srcAttrTypes [srcNoAttrs];

    /*iterate through 0 to src_nAttrs-1 :
        get the i'th attribute's AttrCatEntry using AttrCacheTable::getAttrCatEntry()
        fill the attr_names, attr_types arrays that we declared with the entries
        of corresponding attributes
    */
    for (int attrIndex = 0; attrIndex < srcNoAttrs; attrIndex++) {
        AttrCatEntry attrCatEntryBuffer;
        AttrCacheTable::getAttrCatEntry(srcRelId, attrIndex, &attrCatEntryBuffer);

        strcpy (srcAttrNames[attrIndex], attrCatEntryBuffer.attrName);
        srcAttrTypes[attrIndex] = attrCatEntryBuffer.attrType;
    }

    /* Create the relation for target relation by calling Schema::createRel()
       by providing appropriate arguments */
    // if the createRel returns an error code, then return that value.

    ret = Schema::createRel(targetRel, srcNoAttrs, srcAttrNames, srcAttrTypes);
    if (ret != SUCCESS) return ret;

    /* Open the newly created target relation by calling OpenRelTable::openRel()
       method and store the target relid */
    /* If opening fails, delete the target relation by calling Schema::deleteRel()
       and return the error value returned from openRel() */
    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0 || targetRelId >= MAX_OPEN) return targetRelId;

    /*** Selecting and inserting records into the target relation ***/
    /* Before calling the search function, reset the search to start from the
       first using RelCacheTable::resetSearchIndex() */
    // RelCacheTable::resetSearchIndex(srcRelId);

    Attribute record[srcNoAttrs];

    /*
        The BlockAccess::search() function can either do a linearSearch or
        a B+ tree search. Hence, reset the search index of the relation in the
        relation cache using RelCacheTable::resetSearchIndex().
        Also, reset the search index in the attribute cache for the select
        condition attribute with name given by the argument `attr`. Use
        AttrCacheTable::resetSearchIndex().
        Both these calls are necessary to ensure that search begins from the
        first record.
    */

    RelCacheTable::resetSearchIndex(srcRelId);
    AttrCacheTable::resetSearchIndex(srcRelId, attr);

    // read every record that satisfies the condition by repeatedly calling
    // BlockAccess::search() until there are no more records to be read

    while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS) 
    {
        ret = BlockAccess::insert(targetRelId, record);

        // if (insert fails) {
        //     close the targetrel(by calling Schema::closeRel(targetrel))
        //     delete targetrel (by calling Schema::deleteRel(targetrel))
        //     return ret;
        // }

        if (ret != SUCCESS) 
        {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    // Close the targetRel by calling closeRel() method of schema layer
    Schema::closeRel(targetRel);

    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) 
{
    int srcRelId = OpenRelTable::getRelId(srcRel); // srcRel's rel-id (use OpenRelTable::getRelId() function

    // if srcRel is not open in open relation table, return E_RELNOTOPEN
    if (srcRelId < 0 || srcRelId >= MAX_OPEN) return E_RELNOTOPEN;

    // get RelCatEntry of srcRel using RelCacheTable::getRelCatEntry()
    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntryBuffer);

    // get the no. of attributes present in relation from the fetched RelCatEntry.
    int srcNoAttrs = relCatEntryBuffer.numAttrs;

    // declare attr_offset[tar_nAttrs] an array of type int.
    // where i-th entry will store the offset in a record of srcRel for the
    // i-th attribute in the target relation.
    int attrOffset [tar_nAttrs];
    // for (int attrIndex = 0; attrIndex < tar_nAttrs; attrIndex++) {
    //     attrOffset[attrIndex] = AttrCacheTable::getAttributeOffset(srcRelId, tar_Attrs[attrIndex]);
    //     if (attrOffset[attrIndex] < 0) return attrOffset[attrIndex];
    // }

    // let attr_types[tar_nAttrs] be an array of type int.
    // where i-th entry will store the type of the i-th attribute in the target relation.
    int attrTypes [tar_nAttrs];
    
    /*** Checking if attributes of target are present in the source relation
         and storing its offsets and types ***/

    /*iterate through 0 to tar_nAttrs-1 :
        - get the attribute catalog entry of the attribute with name tar_attrs[i].
        - if the attribute is not found return E_ATTRNOTEXIST
        - fill the attr_offset, attr_types arrays of target relation from the
          corresponding attribute catalog entries of source relation
    */
    
    for (int attrIndex = 0; attrIndex < tar_nAttrs; attrIndex++) {
        attrOffset[attrIndex] = AttrCacheTable::getAttributeOffset(srcRelId, tar_Attrs[attrIndex]);
        if (attrOffset[attrIndex] < 0) return attrOffset[attrIndex];

        AttrCatEntry attrCatEntryBuffer;
        AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[attrIndex], &attrCatEntryBuffer);

        attrTypes[attrIndex] = attrCatEntryBuffer.attrType;
    }

    /*** Creating and opening the target relation ***/

    // Create a relation for target relation by calling Schema::createRel()
    int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attrTypes);

    // if the createRel returns an error code, then return that value.
    if (ret != SUCCESS) return ret;

    // Open the newly created target relation by calling OpenRelTable::openRel()
    // and get the target relid
    int targetRelId = OpenRelTable::openRel(targetRel);

    // If opening fails, delete the target relation by calling Schema::deleteRel()
    // and return the error value from openRel()

    if (targetRelId < 0)
    {
        Schema::deleteRel (targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/

    // Take care to reset the searchIndex before calling the project function
    // using RelCacheTable::resetSearchIndex()
    RelCacheTable::resetSearchIndex(srcRelId);

    Attribute record[srcNoAttrs];

    while (BlockAccess::project(srcRelId, record) == SUCCESS) {
        // the variable `record` will contain the next record
        Attribute proj_record[tar_nAttrs];

        //iterate through 0 to tar_attrs-1:
        //    proj_record[attr_iter] = record[attr_offset[attr_iter]]

        for (int attrIndex = 0; attrIndex < tar_nAttrs; attrIndex++)
            proj_record[attrIndex] = record[attrOffset[attrIndex]];


        ret = BlockAccess::insert(targetRelId, proj_record);

        if (ret != SUCCESS) {
            // close the targetrel by calling Schema::closeRel()
            // delete targetrel by calling Schema::deleteRel()
            // return ret;

            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);

            return ret;
        }
    }

    // Close the targetRel by calling Schema::closeRel()
    Schema::closeRel(targetRel);

    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) 
{
    int srcRelId = OpenRelTable::getRelId(srcRel); // srcRel's rel-id (use OpenRelTable::getRelId() function

    // if srcRel is not open in open relation table, return E_RELNOTOPEN
    if (srcRelId < 0 || srcRelId >= MAX_OPEN) return E_RELNOTOPEN;

    // get RelCatEntry of srcRel using RelCacheTable::getRelCatEntry()
    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntryBuffer);

    // get the no. of attributes present in relation from the fetched RelCatEntry.
    int srcNoAttrs = relCatEntryBuffer.numAttrs;

    // attrNames and attrTypes will be used to store the attribute names
    // and types of the source relation respectively
    char attrNames[srcNoAttrs][ATTR_SIZE];
    int attrTypes[srcNoAttrs];

    /*iterate through every attribute of the source relation :
        - get the AttributeCat entry of the attribute with offset.
          (using AttrCacheTable::getAttrCatEntry())
        - fill the arrays `attrNames` and `attrTypes` that we declared earlier
          with the data about each attribute
    */

    for (int attrIndex = 0; attrIndex < srcNoAttrs; attrIndex++) {
        AttrCatEntry attrCatEntryBuffer;
        AttrCacheTable::getAttrCatEntry(srcRelId, attrIndex, &attrCatEntryBuffer);

        strcpy (attrNames[attrIndex], attrCatEntryBuffer.attrName);
        attrTypes[attrIndex] = attrCatEntryBuffer.attrType;
    }

    /*** Creating and opening the target relation ***/

    // Create a relation for target relation by calling Schema::createRel()
    int ret = Schema::createRel(targetRel, srcNoAttrs, attrNames, attrTypes);
    
    // if the createRel returns an error code, then return that value.
    if (ret != SUCCESS) return ret;

    // Open the newly created target relation by calling OpenRelTable::openRel()
    // and get the target relid
    int targetRelId = OpenRelTable::openRel(targetRel);

    // If opening fails, delete the target relation by calling Schema::deleteRel() of
    // return the error value returned from openRel().
    if (targetRelId < 0 || targetRelId >= MAX_OPEN) return targetRelId;

    /*** Inserting projected records into the target relation ***/

    // Take care to reset the searchIndex before calling the project function
    // using RelCacheTable::resetSearchIndex()
    RelCacheTable::resetSearchIndex(srcRelId);

    Attribute record[srcNoAttrs];

    while (BlockAccess::project(srcRelId, record) == SUCCESS)
    {
        // record will contain the next record

        ret = BlockAccess::insert(targetRelId, record);

        if (ret != SUCCESS) {
            // close the targetrel by calling Schema::closeRel()
            // delete targetrel by calling Schema::deleteRel()
            // return ret;

            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    // Close the targetRel by calling Schema::closeRel()
    Schema::closeRel(targetRel);

    return SUCCESS;
}

// will return if a string can be parsed as a floating point number
inline bool isNumber(char *str)
{
    int len;
    float ignore;
    /*
      sscanf returns the number of elements read, so if there is no float matching
      the first %f, ret will be 0, else it'll be 1

      %n gets the number of characters read. this scanf sequence will read the
      first float ignoring all the whitespace before and after. and the number of
      characters read that far will be stored in len. if len == strlen(str), then
      the string only contains a float with/without whitespace. else, there's other
      characters.
    */
    int ret = sscanf(str, "%f %n", &ignore, &len);
    return ret == 1 && len == strlen(str);
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]){
    // if relName is equal to "RELATIONCAT" or "ATTRIBUTECAT"
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0)
    return E_NOTPERMITTED;

    // get the relation's rel-id using OpenRelTable::getRelId() method
    int relId = OpenRelTable::getRelId(relName);

    // if relation is not open in open relation table, return E_RELNOTOPEN
    // (check if the value returned from getRelId function call = E_RELNOTOPEN)
    if (relId < 0 || relId >= MAX_OPEN) return E_RELNOTOPEN;

    // get the relation catalog entry from relation cache
    // (use RelCacheTable::getRelCatEntry() of Cache Layer)
    RelCatEntry relCatBuffer;
    RelCacheTable::getRelCatEntry(relId, &relCatBuffer);

    // if relCatEntry.numAttrs != numberOfAttributes in relation,
    if (relCatBuffer.numAttrs != nAttrs) return E_NATTRMISMATCH;

    // let recordValues[numberOfAttributes] be an array of type union Attribute
    Attribute recordValues[nAttrs];

    // TODO: Converting 2D char array of record values to Attribute array recordValues 
    // iterate through 0 to nAttrs-1: (let i be the iterator)
    for (int attrIndex = 0; attrIndex < nAttrs; attrIndex++)
    {
        // get the attr-cat entry for the i'th attribute from the attr-cache
        // (use AttrCacheTable::getAttrCatEntry())
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, attrIndex, &attrCatEntry);

        int type = attrCatEntry.attrType;
        if (type == NUMBER)
        {
            // if the char array record[i] can be converted to a number
            // (check this using isNumber() function)
            if (isNumber(record[attrIndex]))
            {
                /* convert the char array to numeral and store it
                   at recordValues[i].nVal using atof() */
                recordValues[attrIndex].nVal = atof (record[attrIndex]);
            }
            else
                return E_ATTRTYPEMISMATCH;
        }
        else if (type == STRING)
        {
            // copy record[i] to recordValues[i].sVal
            strcpy((char *) &(recordValues[attrIndex].sVal), record[attrIndex]);
        }
    }

    // insert the record by calling BlockAccess::insert() function
    // let retVal denote the return value of insert call
    int ret = BlockAccess::insert(relId, recordValues);

    return ret;
}

int Algebra::join(char srcRelation1[ATTR_SIZE], char srcRelation2[ATTR_SIZE], char targetRelation[ATTR_SIZE], char attribute1[ATTR_SIZE], char attribute2[ATTR_SIZE]) {

    // Fetch the relation IDs of both source relations from the Open Relation Table.
    // Relations must be open before any operation can be performed on them.
    int srcRelId1 = OpenRelTable::getRelId(srcRelation1);
    int srcRelId2 = OpenRelTable::getRelId(srcRelation2);

    // If either relation is not open, we cannot proceed.
    if (srcRelId1 == E_RELNOTOPEN || srcRelId2 == E_RELNOTOPEN)
        return E_RELNOTOPEN;

    AttrCatEntry attrCatEntry1, attrCatEntry2;

    // Fetch the attribute catalog entries for the join attributes.
    // These entries contain metadata like type and offset needed for comparison and indexing.
    int ret1 = AttrCacheTable::getAttrCatEntry(srcRelId1, attribute1, &attrCatEntry1);
    int ret2 = AttrCacheTable::getAttrCatEntry(srcRelId2, attribute2, &attrCatEntry2);

    // If either join attribute does not exist in its respective relation, return error.
    if (ret1 == E_ATTRNOTEXIST || ret2 == E_ATTRNOTEXIST)
        return E_ATTRNOTEXIST;

    // Join is only valid between attributes of the same type (e.g., both INT or both STR).
    // Comparing attributes of different types is semantically meaningless.
    if (attrCatEntry1.attrType != attrCatEntry2.attrType)
        return E_ATTRTYPEMISMATCH;

    // Fetch relation catalog entries to know the number of attributes in each relation.
    RelCatEntry relCatEntry1, relCatEntry2;
    RelCacheTable::getRelCatEntry(srcRelId1, &relCatEntry1);
    RelCacheTable::getRelCatEntry(srcRelId2, &relCatEntry2);

    int numOfAttributes1 = relCatEntry1.numAttrs;
    int numOfAttributes2 = relCatEntry2.numAttrs;

    // Check for duplicate attribute names across the two relations.
    // If any two attributes (other than the join pair) share a name, the target
    // relation would have ambiguous columns, which is not allowed.
    for (int i = 0; i < numOfAttributes1; i++) {
        AttrCatEntry entry1;
        AttrCacheTable::getAttrCatEntry(srcRelId1, i, &entry1);

        for (int j = 0; j < numOfAttributes2; j++) {
            AttrCatEntry entry2;
            AttrCacheTable::getAttrCatEntry(srcRelId2, j, &entry2);

            // The join attribute pair (attribute1, attribute2) is intentionally
            // skipped — attribute2 will be dropped from the target anyway.
            if (strcmp(entry1.attrName, attribute1) == 0 &&
                strcmp(entry2.attrName, attribute2) == 0)
                continue;

            if (strcmp(entry1.attrName, entry2.attrName) == 0)
                return E_DUPLICATEATTR;
        }
    }

    // A B+ tree index on attribute2 of srcRelation2 is required for the inner
    // loop's search. If it doesn't exist, create one now.
    // This transforms the inner loop from a linear scan to an O(log n) index lookup,
    // making the overall join efficient.
    if (attrCatEntry2.rootBlock == -1) {
        int bPlusRet = BPlusTree::bPlusCreate(srcRelId2, attribute2);
        if (bPlusRet != SUCCESS)
            return bPlusRet; // Only possible error here is E_DISKFULL
    }

    // The target relation has (nAttrs1 + nAttrs2 - 1) attributes because
    // attribute2 from srcRelation2 is excluded — it is identical to attribute1
    // for every matched tuple, so retaining it would be redundant.
    int numOfAttributesInTarget = numOfAttributes1 + numOfAttributes2 - 1;

    char targetRelAttrNames[numOfAttributesInTarget][ATTR_SIZE];
    int targetRelAttrTypes[numOfAttributesInTarget];

    // Populate the target schema:
    // First, include ALL attributes from srcRelation1.
    int targetIdx = 0;
    for (int i = 0; i < numOfAttributes1; i++) {
        AttrCatEntry entry;
        AttrCacheTable::getAttrCatEntry(srcRelId1, i, &entry);
        strcpy(targetRelAttrNames[targetIdx], entry.attrName);
        targetRelAttrTypes[targetIdx] = entry.attrType;
        targetIdx++;
    }

    // Then include attributes from srcRelation2, skipping attribute2
    // since it carries no new information beyond what attribute1 already holds.
    for (int i = 0; i < numOfAttributes2; i++) {
        AttrCatEntry entry;
        AttrCacheTable::getAttrCatEntry(srcRelId2, i, &entry);
        if (strcmp(entry.attrName, attribute2) == 0)
            continue;
        strcpy(targetRelAttrNames[targetIdx], entry.attrName);
        targetRelAttrTypes[targetIdx] = entry.attrType;
        targetIdx++;
    }

    // Create the target relation on disk with the schema built above.
    int createRet = Schema::createRel(targetRelation, numOfAttributesInTarget,
                                      targetRelAttrNames, targetRelAttrTypes);
    if (createRet != SUCCESS)
        return createRet;

    // Open the target relation so we can insert records into it.
    int targetRelId = OpenRelTable::openRel(targetRelation);
    if (targetRelId < 0) {
        // If the Open Relation Table is full, clean up the created relation
        // to avoid leaving an empty, inaccessible relation on disk.
        Schema::deleteRel(targetRelation);
        return targetRelId;
    }

    Attribute record1[numOfAttributes1];
    Attribute record2[numOfAttributes2];
    Attribute targetRecord[numOfAttributesInTarget];

    // Reset the search index of srcRelation1 to start scanning from the beginning.
    RelCacheTable::resetSearchIndex(srcRelId1);

    // Outer loop: iterate over every record in srcRelation1 one by one.
    while (BlockAccess::project(srcRelId1, record1) == SUCCESS) {

        // For each record in srcRelation1, reset the search state of srcRelation2
        // so the inner loop starts fresh from the beginning of its index.
        RelCacheTable::resetSearchIndex(srcRelId2);
        AttrCacheTable::resetSearchIndex(srcRelId2, attribute2);

        // Inner loop: use the B+ index on attribute2 to efficiently find all
        // records in srcRelation2 where attribute2 == record1[attribute1].
        // This implements the equi-join condition: r1.attribute1 = r2.attribute2.
        while (BlockAccess::search(
            srcRelId2, record2, attribute2, record1[attrCatEntry1.offset], EQ
        ) == SUCCESS) {

            // Build the target record by combining:
            // - all attributes from record1 (srcRelation1)
            // - all attributes from record2 (srcRelation2) except attribute2
            targetIdx = 0;
            for (int i = 0; i < numOfAttributes1; i++)
                targetRecord[targetIdx++] = record1[i];

            for (int i = 0; i < numOfAttributes2; i++) {
                AttrCatEntry entry;
                AttrCacheTable::getAttrCatEntry(srcRelId2, i, &entry);
                if (strcmp(entry.attrName, attribute2) == 0)
                    continue;
                targetRecord[targetIdx++] = record2[i];
            }

            // Insert the joined record into the target relation.
            int insertRet = BlockAccess::insert(targetRelId, targetRecord);
            if (insertRet != SUCCESS) {
                // Insert can only fail if the disk is full.
                // Clean up: close and delete the partially filled target relation
                // to avoid leaving corrupted/incomplete data on disk.
                OpenRelTable::closeRel(targetRelId);
                Schema::deleteRel(targetRelation);
                return E_DISKFULL;
            }
        }
    }

    // All matching tuples have been inserted. Close the target relation.
    OpenRelTable::closeRel(targetRelId);
    return SUCCESS;
}