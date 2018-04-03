/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   btree_mem.c
 * Author: Oluwatosin V. Adewale
 * 
 * Created on September 23, 2017, 7:31 PM
 */

#include "relation.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Type declarations */
typedef struct BtNode BtNode;
typedef struct Entry Entry;
typedef union Child_or_Record Child_or_Record;
typedef struct Cursor Cursor;

/* Function Declarations */
static BtNode* createNewNode(Bool isLeaf);

static Bool isNodeParent (BtNode * currNode, unsigned long key);

static Bool insertKeyRecord(BtNode* node, unsigned long key, const void* record,
        Entry * const newEntryFromChild, Cursor* cursor, Relation_T relation,
        const int level);

static void putEntry(Cursor_T cursor,int level, Entry * newEntry, size_t key);

static Bool deleteKeyRecord(BtNode* parentNode, BtNode* node, unsigned long key,
        Entry* const oldEntryFromChild, Cursor* cursor, Relation_T relation, 
        const int level);

static Bool handleDeleteOfEntry(BtNode* parentNode, BtNode* node, 
        Entry* const oldEntryFromChild, Cursor* cursor,
        Relation_T relation, const int level);

static void redistributeOrMerge(BtNode* leftNode, BtNode* rightNode,
        Entry* const parentEntry, Bool isLeaf,  Bool* wasMerged);

static int findChildIndex(const Entry* entries, unsigned long key, int length);

static int findRecordIndex(const Entry* entries, unsigned long key, int length);

static Bool moveToKey(BtNode* node, unsigned long key, Cursor* cursor,
	const int level);

static Bool moveToFirstRecord(BtNode* node, Cursor* cursor, int level);

static void handleDeleteBtree(BtNode* node, void (* freeRecord)(void *));

static void ASSERT_NODE_INVARIANT(BtNode* node, Relation_T relation);

static void printTree(BtNode* node, int level);



/**********************************
 * Type Definitions               *
 **********************************/

/* A relation is implemented using a B-tree datastructure.
 * 
 * A B-tree is a self-balancing tree datastructure with efficient insertion, 
 * deletion and search. Because of their efficiency and high fan-out it is good 
 * for indexing data in secondary storage (files). Because of their high
 * fanout, B-trees have a small height (the distance from the root to the 
 * leaves is small). Btree is actually a B+ tree where records are stored in the
 * leaves and not in internal nodes. */
typedef struct Relation {
    /* Root node of btree */
    struct BtNode* root;
    size_t numRecords;
} Relation;

/* Each entry in a btree node has a key. 
 * Entries in the leaf have an associated record. */
union Child_or_Record {
    /* Key of entry */
    BtNode* child;
    /* Record in entry, if entry is in leaf node */
    const void* record;
};

struct Entry {
    unsigned long key;
    Child_or_Record ptr;
};

/* B-trees consist of nodes.
 * Each node of a btree has keys and pointers to its children (interior nodes) 
 * or records(leaf nodes). */
struct BtNode {
    /* Is this a leaf?*/
    Bool isLeaf;
    /* Current number of keys in the node */
    int numKeys;
    /* Ptr to first child */
    BtNode* ptr0;
    /* Array of entries in the Node. An entry consists of a key and a pointer.
     * The pointer is either a pointer to a child or a pointer to a record. 
     * A BtNode contains at most FANOUT keys and FANOUT + 1 children 
     * (including ptr0) records. */
    Entry entries[FANOUT];
};

struct Cursor {
    /* The relation that this cursor points to*/
    Relation* relation;
    /* The current Node BtCursor is pointing to */
    Bool isValid;
    /*What level is the cursor at*/
    int level;
    /* Arrays of index at ancestor node[i] containing child pointer to next 
     * ancestor node [i + 1]. */
    /* indices range from -1 to NUM_KEYS - 1. At leaf, index is entryIdx */
    int ancestorsIdx[MAX_TREE_DEPTH];
    /* Array of Current Nodes Ancestors*/
    BtNode* ancestors[MAX_TREE_DEPTH];
};

int entryIndex (Cursor_T cursor) {
  return cursor->ancestorsIdx[cursor->level];
}

BtNode* currNode (Cursor_T cursor) {
  return cursor->ancestors[cursor->level];
}

/* TODO: set cursor method */

/**********************************
 * Function Definitions           *
 **********************************/

Relation_T RL_NewRelation(void) {
    BtNode* pRootNode;
    Relation* pNewRelation;

    pRootNode = createNewNode(True);
    if (pRootNode == NULL) {
        return NULL;
    }

    pNewRelation = (Relation*) malloc(sizeof (Relation));
    if (pNewRelation == NULL) {
        free(pRootNode);
        return NULL;
    }

    pNewRelation->root = pRootNode;
    pNewRelation->numRecords = 0;

    return pNewRelation;
}

/* TODO: do this later. */
void RL_DeleteRelation(Relation_T relation, void (* freeRecord)(void *)){
    assert(relation != NULL);
    handleDeleteBtree(relation->root, freeRecord);
}

Cursor_T RL_NewCursor(Relation_T relation) {
    Cursor* cursor;
    size_t i;

    assert(relation != NULL);

    cursor = (Cursor*) malloc(sizeof (Cursor));
    if (cursor == NULL) {
        return NULL;
    }

    cursor->relation = relation;
    cursor->isValid = False;
    cursor->level = 0;
    (void) moveToFirstRecord(relation->root, cursor, 0);
    /* TODO: sets isValid, level and the first ancestors, ancestorsIdx (should be ok) */

    for (i = cursor->level+1; i < MAX_TREE_DEPTH; i++) {
        cursor->ancestorsIdx[i] = 0;
        cursor->ancestors[i] = NULL;
    }
    return cursor;
}

void RL_FreeCursor(Cursor_T btCursor) {
    free(btCursor);
}

Bool RL_CursorIsValid(Cursor_T cursor) {
    assert(cursor != NULL);
    return cursor->isValid;
}

void RL_PutRecord(Cursor_T cursor, unsigned long key, const void* record) {
    Bool success;		/* TODO: remove it? */
    Entry newEntry;
    assert(cursor != NULL);

    newEntry.ptr.record = record;
    newEntry.key = key;

    success = RL_MoveToKey(cursor, key);
    putEntry(cursor,cursor->level, &newEntry, key);
    success = RL_MoveToNext(cursor);

    /* cursor->isValid = success; */ /* TODO: RL_MoveToNext already sets isValid (should be done) */
    
    return;
}

/* Returns true if we know for sure that currNode is a parent of the key
 * Returns False if we can't be sure */
static Bool isNodeParent (BtNode * currNode, unsigned long key) {

  int idx;
  idx = findChildIndex(currNode->entries, key, currNode->numKeys);
  if (idx == -1 || idx == currNode->numKeys -1) {
    return False;
  }
  return True;

}

Bool RL_MoveToKey(Cursor_T cursor, unsigned long key) {
    unsigned long lowest, highest;
    assert(cursor != NULL);

    /* If the root is a leaf node */
    if (cursor->level == 0)
      return moveToKey(currNode(cursor), key, cursor, cursor->level);
    
    /* Otherwise, we first check if the cursor should point to the same leaf node */
    if (cursor->isValid) { /* TODO: I tink we can remove this now that we removed the empty case */
      lowest = currNode(cursor)->entries[0].key;
      highest = currNode(cursor)->entries[currNode(cursor)->numKeys - 1].key;
        if (key >= lowest && key <= highest) {
	  return moveToKey(currNode(cursor), key, cursor, cursor->level);
        }
    }
    
    /* If not, we go up until we are sure to be in a parent node 
     * A parent node is either the root or has one lesser key and one greater key
     * than the desired one (i.e. findChildIndex isn't -1 or numKeys-1 */
    
    cursor->level --;
    
    while (cursor->level > 0 && isNodeParent(currNode(cursor), key) == False) {
      cursor->level --;
    }	   
    
    /* When the parent node is found, we go down to the desired key */
    return moveToKey(cursor->ancestors[cursor->level], key, cursor, cursor->level);
}

const void* RL_GetRecord(Cursor_T cursor) {
    assert(cursor->isValid);
    return (currNode(cursor)->entries)[entryIndex(cursor)].ptr.record;
}


Bool RL_DeleteRecord(Cursor_T cursor, unsigned long key) {
    Bool success;
    Entry oldEntryFromChild;
    assert(cursor != NULL);

    oldEntryFromChild.ptr.child = NULL;

    success = deleteKeyRecord(NULL, cursor->relation->root, key,
            &oldEntryFromChild, cursor, cursor->relation, 0);

    /*TODO: move cursor->isValid = True, etc into return statement here. */
    if (success) {
        cursor->isValid = False;
        cursor->relation->numRecords--;
        return success;
    }

    cursor->isValid = False;
    return False;
}


Bool RL_MoveToFirstRecord(Cursor_T btCursor) {
    
    Bool success;
    
    assert(btCursor);
    
    success = moveToFirstRecord(btCursor->relation->root, btCursor, 0);
    btCursor->isValid = success;
    return success;
}

Bool RL_MoveToNext(Cursor_T btCursor) {
        
    int numKeys, currLevel, newIdx;
    numKeys = currNode(btCursor)->numKeys;
    currLevel = btCursor->level;
        
    assert(btCursor != NULL);
    assert(btCursor->isValid);
    
    /* If cursor is not at last index, set it to next index.*/
    if(entryIndex(btCursor) < (numKeys - 1)) {
        btCursor->ancestorsIdx[currLevel] ++;
        btCursor->isValid = True;
        return True;
    }
    
    /* While below root and ancestor pointer is last pointer, ascend. */
    while(currLevel >= 0 && (btCursor->ancestorsIdx[currLevel] == 
            btCursor->ancestors[currLevel]->numKeys - 1)){
        currLevel--;
    }

    
    /* If at last record, currLevel would be at root.*/
    if (currLevel < 0) {
        btCursor->isValid = False;
        return False;
    }
    
    /* Go down next child to next level */
    newIdx = ++(btCursor->ancestorsIdx[currLevel]);
    btCursor->ancestors[currLevel+1] = btCursor->ancestors[currLevel]->entries[newIdx].ptr.child;
    currLevel++;
    
    /* Descend to correct leaf down leftmost child. All leaves have same level. */
    while(currLevel < btCursor->level){
        btCursor->ancestorsIdx[currLevel] = -1;
        btCursor->ancestors[currLevel+1] = btCursor->ancestors[currLevel]->entries[-1].ptr.child;
        currLevel++; 
    }
    
    btCursor->ancestorsIdx[currLevel] = 0;
    btCursor->isValid = True;
    
    return True; 
}


Bool RL_MoveToPrevious(Cursor_T btCursor) {
    
    int numKeys, currLevel, newIdx;
    numKeys = currNode(btCursor)->numKeys;
    currLevel = btCursor->level;
        
    assert(btCursor != NULL);
    assert(btCursor->isValid);
    
    /* If cursor is not at last index, set it to previous index.*/
    if(entryIndex(btCursor) > 0 ) {
      btCursor->ancestorsIdx[currLevel] --;
        btCursor->isValid = True;
        return True;
    } else {
        /* We are in leftmost entry of leaf. There is no entry -1. Go up a level. */
        currLevel--;
    }

    
    /* While below root and ancestor pointer is first pointer, ascend. */
    while(currLevel >= 0 && (btCursor->ancestorsIdx[currLevel] == -1)){
        currLevel--;
    }

    
    /* If at last record, currLevel would be at root.*/
    if (currLevel < 0) {
        return False;
    }
    
    /* Go down previous child to next (lower) level */
    newIdx = --(btCursor->ancestorsIdx[currLevel]);
    btCursor->ancestors[currLevel+1] = btCursor->ancestors[currLevel]->entries[newIdx].ptr.child;
    currLevel++;
    
    /* Descend to correct leaf down rightmost child. All leaves have same level. */
    while(currLevel < btCursor->level){
        int lastIdx = btCursor->ancestors[currLevel]->numKeys - 1;
        
        btCursor->ancestorsIdx[currLevel] = lastIdx;
        btCursor->ancestors[currLevel+1] = btCursor->ancestors[currLevel]->entries[lastIdx].ptr.child;
        currLevel++; 
    }
    
    numKeys = btCursor->ancestors[currLevel]->numKeys;

    btCursor->ancestorsIdx[currLevel] = 0;
    btCursor->isValid = True;
    
    return True; 
}

unsigned long RL_GetKey(Cursor_T cursor) {
    assert(cursor != NULL);
    assert(cursor->isValid);
    
    return currNode(cursor)->entries[entryIndex(cursor)].key;
}


Bool RL_IsEmpty(Cursor_T btCursor) {
    BtNode* root;
    assert(btCursor != NULL);
    
    root = btCursor->relation->root;
    if (root->isLeaf && root->numKeys == 0) {
        return True;
    }
    return False;
}

size_t RL_NumRecords(Cursor_T btCursor) {
    assert(btCursor != NULL);
    
    return btCursor->relation->numRecords;
    
}

void RL_PrintTree(Relation_T relation) {

    assert(relation != NULL);
    assert(relation->root != NULL);
    
    printTree(relation->root, 0);
    
}


static BtNode* createNewNode(Bool isLeaf) {
    BtNode* newNode;

    newNode = (BtNode*) malloc(sizeof (BtNode));
    if (newNode == NULL) {
        return NULL;
    }

    newNode->numKeys = 0;
    newNode->isLeaf = isLeaf;
    newNode->ptr0 = NULL;

    return newNode;
}

/* Insert entry and split node. Return a new entry to be inserted in parent. 
 * Use if this is a leaf node, new entry's key is a copy of the first key to
 * in the second node. Otherwise, newEntry's key is the key between the last key
 * of the first node and the first key of the second node. In both cases ptr is 
 * a ptr to the newly created node. */
static Entry* splitnode(BtNode* node, Entry* entry, Bool isLeaf) {
    Entry allEntries[FANOUT + 1];
    Entry* newEntry;
    BtNode* newNode;
    int i, j, tgtIdx, startIdx;
    Bool inserted;
    
    /* Find first key that is greater than search key. Search key goes before this key. */
    /* Question: is this correct node? */
    tgtIdx = findRecordIndex(node->entries, entry->key, node->numKeys);
    
    j = 0; inserted = False;
    
    /* Build list of all entries. */
    for(i = 0; i < FANOUT + 1; i++) {  
        if(inserted = False && j == tgtIdx) {
            allEntries[i] = entry;
            inserted = True;
            continue;
        }
        allEntries[i] = node->entries[j];
        j++;  
    }
    
    /* if the new entry came before an entry in the first node, 
     * then we need to update those entries in the first node.*/
    if(tgtIdx < FANOUT / 2) {
        for(i = tgtIdx; i < FANOUT / 2; i++) {
            node->numKeys[i] = allEntries[i];   
        }
    }
    node->numKeys = FANOUT / 2;
    
    /* Create the new node. */
    newNode = createNewNode(isLeaf);
    assert(newNode);
    
    /* Select appropriate idx to start copying. */
    if(isLeaf) {
        startIdx = FANOUT / 2;
    } else {
        /* We push up middle node, so don't copy it into snd node. */
        startIdx = FANOUT / 2 + 1;
    }
    
    /*Copy entries to second node.*/
    j = 0;
    for (i = startIdx; i < FANOUT + 1; i++) {
        newNode->entries[j] = allEntries[i];
        j++;
    }
    newNode->numKeys = FANOUT + 1 - startIdx;
    
    newEntry = (Entry*) malloc(sizeof(Entry));
    assert(newEntry != NULL);
    
    /* If this is a leaf, copy up first entry on second node. */
    if(isLeaf) {
        newEntry->key = allEntries[startIdx].key
        newEntry->ptr.child = newNode;
    }
    /* Else we are pushing up entry before second node. */
    else {
        newEntry->key = allEntries[startIdx - 1].key
        newEntry->ptr.child = newNode;
    }

    return newEntry; /* TODO */
}
  
/* Inserting a new entry at a position given by the cursor.
 * The cursor should point to the correct location.
 * If the key already exists in the relation, its record will be updated. */
static void putEntry(Cursor_T cursor, int level, Entry * newEntry, size_t key) {
  BtNode * currNode;
  
  if (level==-1) {
    /* the root has been split, and newEntry should be the only ontry in the new root */
    currNode = createNewNode(False);
    assert(currNode);

    currNode->ptr0 = cursor->relation->root;
    currNode->numKeys = 1;
    currNode->entries[0] = *newEntry;

    cursor->relation->root = currNode;

    cursor->ancestors[0] = currNode;

    /* we need to update the cursor for the original inserted key */
    (void) moveToKey(currNode, key, cursor, 0);
    /* this has to return true because key was just inserted */
    return;
  }
  
  currNode = cursor->ancestors[level];

  if (currNode->isLeaf) { /* current node is a leaf node */
    
    if (currNode->entries[entryIndex(cursor)].key == newEntry->key) {
      /* the key already exists in the cursor */
      currNode->entries[entryIndex(cursor)].ptr = newEntry->ptr;
      return;
    }
    else {
      /* the key does not exist and must be inserted */

      if (currNode->numKeys < FANOUT) {
	const size_t tgtIdx = entryIndex(cursor);
	
	size_t i;
	/* Move all entries to the right of tgtIdx one to the right*/
	for (i=currNode->numKeys; i > tgtIdx; i--) {
	  currNode->entries[i] = currNode->entries[i-1];
	}
	currNode->entries[tgtIdx] = *newEntry;
	currNode->numKeys++;
	cursor->relation->numRecords++;
	return;
      }
      else {
	/* the leaf node must be split */
	newEntry = splitnode(currNode, newEntry, True);

	/* recursive call to insert the newEntry from splitnode a level above */
	putEntry(cursor, level-1, newEntry, key);
      }
    }
  }
  else { /* current node is an intern node */

    if (currNode->numKeys < FANOUT) {
      /* the current intern node has enough space to insert a new entry */
      const size_t tgtIdx = cursor->ancestorsIdx[level] +1;
      /* this is a correct index because there is enough space in the node */
      
      size_t i;
      /* Move all entries to the right of tgtIdx one to the right*/
      for (i=currNode->numKeys; i > tgtIdx; i--) {
	currNode->entries[i] = currNode->entries[i-1];
      }
      currNode->entries[tgtIdx] = *newEntry;
      currNode->numKeys++;
      cursor->relation->numRecords++; /* is that the good place to put it? */

      /* update the cursor to make it point to the inserted key */
      (void) moveToKey(currNode, key, cursor, level);

      return;
    }
    else {
      /* the node must be split */
      newEntry = splitnode(currNode, newEntry, False);
      
      /* recursive call to insert the newEntry from splitnode a level above */
      putEntry(cursor, level-1, newEntry, key);
      
    }
  }
}

/* Algorithm from page 259 of Database Management Systems Second Edition. 
 * Insert (update) key and record into leaf in nodePtr. Use newEntryFromChild to detect if a 
 * split has occurred. It is a constant pointer to an entry. It should be filled
 * in with the correct value. It contains a middle key from split and a pointer 
 * to the second node resulting from the split.  Let cursor point to newly 
 * created record. 
 * 
 * Return True on success. Return False on failure. */
static Bool insertKeyRecord(BtNode* node, unsigned long key, const void* record,
        Entry * const newEntryFromChild, Cursor* cursor, Relation_T relation,
        const int level){
    Bool success;
    /* Index of pointer to child tree containing inserted key */
    int childTreePtrIdx;

    assert(node != NULL);
    assert(cursor != NULL);
    /* Container to store new entry should never be NULL.*/
    assert(newEntryFromChild != NULL);
    assert(level < MAX_TREE_DEPTH);
    
    ASSERT_NODE_INVARIANT(node, relation);

    /* Assign current node as ancestor node at current level */
    cursor->ancestors[level] = node;

    /* If node is non-leaf node*/
    if (node->isLeaf == False) {
        BtNode* childNode;
        childTreePtrIdx = findChildIndex(node->entries, key,
                node->numKeys);

        if (childTreePtrIdx == -1) {
            childNode = node->ptr0;
        } else {
            childNode = node->entries[childTreePtrIdx].ptr.child;
        }
    
        /* set index of pointer in current node that leads to next ancestor */
        cursor->ancestorsIdx[level] = childTreePtrIdx;
        
        success = insertKeyRecord(childNode, key,
                record, newEntryFromChild, cursor, relation, level + 1);

        /* if insert failed, return that insert failed */
        if (success == False) {
            return False;
        }
        /*  else if successful and no split, return success.*/
        if (newEntryFromChild->ptr.child == NULL) {
            return success;
        }


        /* If enough space to insert newChildEntry, insert it */
        if (node->numKeys < FANOUT) {
            size_t i;
            unsigned long newKey = newEntryFromChild->key;

            /* Find first entry which newEntry is greater than or equal to
             * child goes to the right of this index. */
            const size_t targetIdx = findChildIndex(node->entries, newKey, node->numKeys);
                        
            
            /* Move all entries to the right of childIndex one to the right*/
            for (i = node->numKeys; i > targetIdx + 1; i--) {
                /* move keys and pointers to the right */
                node->entries[i] = node->entries[i - 1];
            }
            node->entries[targetIdx + 1] = *newEntryFromChild;
            node->numKeys++;
            
            /* if key is greater than or equal to new entry from child record in
             * subtree pointed to by newEntryFromChild->child
             */
            if (key >= newKey) {
                cursor->ancestorsIdx[level] = targetIdx + 1;
            }
            

            newEntryFromChild->ptr.child = NULL;

            return True;
        }
            /* Else split node. We have FANOUT + 1 keys and FANOUT + 2 pointers.
             * Keep first d (floor of FANOUT / 2) keys in the first Node with d+1
             * pointers. Set d+1 key to newEntryFromChild. Move the rest to second node.
             * Set pointer to second node in newEntryFromChild. 
             * If the node is the root, create a new root node from child entry and 
             * let it point to both nodes from split. */
        else {
            /* Create an Array that can hold all entries. */
            Entry allEntries[FANOUT + 1];
            BtNode* newNode;
            int i, j, newChildIdxAllEntries;
            int newEntryIdx = FANOUT / 2;
            /* index of first key that new Key is greater than or equal to  */
            const int newTargetIdx = findChildIndex(node->entries,
                    newEntryFromChild->key, node->numKeys);

            /* To make code simpler, we will store address of child node that
             * record was inserted into. Use this to make it easier to track 
             * which node subtree child is in */
            BtNode* childNode;
            /* If key is smaller than new entry from child, 
             * record inserted in old child tree*/
            if (key < newEntryFromChild->key) {
                if (childTreePtrIdx == -1) {
                    childNode = node->ptr0;
                }
                else {
                    childNode = node->entries[childTreePtrIdx].ptr.child;
                }  
            }
            /* Else record inserted in new child's tree */
            else {
                childNode = newEntryFromChild->ptr.child;
            }
            
            /* copy 0 to targetIdx of node's keys to allEntries, insert 
             * newKeyFromChild, copy remaining targetIdx to MAXNUMKEYS - 1 
             * keys to allEntries. Find index of subtree record was inserted to,
             */
            j = 0, newChildIdxAllEntries = -1;
            for (i = 0; i < FANOUT + 1; i++) {
                if (i == (int) newTargetIdx + 1) {
                    allEntries[i] = *newEntryFromChild;
                    
                } else {
                    allEntries[i] = node->entries[j];
                    j++;
                }
                if (allEntries[i].ptr.child == childNode) {
                    newChildIdxAllEntries = i;
                }   
            }

            /* overwrite current node with floor of first half of keys and 
             * pointers. 
             * TODO: Optimize later! Unnecessary copying over.
             */
            for (i = 0; i < newEntryIdx; i++) {
                node->entries[i] = allEntries[i];
            }
            node->numKeys = newEntryIdx;

            /* create a new non-leaf node to contain second half (not including entry at 
             * newEntryIdx) of split key.
             * copy them to new node */
            newNode = createNewNode(False);
            /* TODO: handle memory error better*/
            assert(newNode);

            j = 0;
            for (i = newEntryIdx + 1; i < FANOUT + 1; i++) {
                newNode->entries[j] = allEntries[i];
                j++;
            }
            newNode->numKeys = j;
            newNode->ptr0 = allEntries[newEntryIdx].ptr.child;

            /*Set newEntryFromChild*/
            newEntryFromChild->key = allEntries[newEntryIdx].key;
            newEntryFromChild->ptr.child = newNode;

            /* set ancestor node and next ancestor pointer depending on 
             * location of pointer to child subtree containing inserted record. 
             * Setting ancestors[level] might be unnecessary in some cases where
             * child subtree containing record does not change after split. */
            if (newChildIdxAllEntries < newEntryIdx) {
                cursor->ancestors[level] = node;
                cursor->ancestorsIdx[level] = newChildIdxAllEntries;
            }
            else {
                cursor->ancestors[level] = newNode;
                cursor->ancestorsIdx[level] = newChildIdxAllEntries - 
                        (newEntryIdx + 1);
            }
            

            /* If this is the root level, create new root, let it point to this node and
             * new Child entry. */
            if (relation->root == node) {
                BtNode* newRootNode = createNewNode(False);
                int i;
                assert(newRootNode);

                newRootNode->ptr0 = node;
                newRootNode->numKeys = 1;
                newRootNode->entries[0] = *newEntryFromChild;

                relation->root = newRootNode;

                newEntryFromChild->ptr.child = NULL;
                
                /* new level created */
                for(i = cursor->level + 1; i > 0; i--){
                    cursor->ancestors[i] = cursor->ancestors[i-1];
                    cursor->ancestorsIdx[i] = 
                            cursor->ancestorsIdx[i-1];
                }
                
                cursor->ancestors[0] = relation->root;
                if (key < newEntryFromChild->key) {
                    cursor->ancestorsIdx[0] = -1;
                } else {
                    cursor->ancestorsIdx[0] = 0;
                }
                
                cursor->level++;
            }
            return True;
        }
    }        
    /* If node is leaf node. */
    else {
        int targetIdx;
        /* If first key, insert record and return success*/
        if (node->numKeys == 0) {
            node->entries[0].key = key;
            node->entries[0].ptr.record = record;
            node->numKeys++;

            cursor->isValid = True;
            cursor->relation->numRecords++;
            
            /*Set index of pointer in current node that leads to next ancestor*/
            cursor->ancestorsIdx[level] = 0;
            cursor->level = level;
            
            newEntryFromChild->ptr.child = NULL;
            
            return True;
        }


        /* If key already exists, update record and return success = True */
        targetIdx = findChildIndex(node->entries, key, node->numKeys);
        if (targetIdx != -1 && node->entries[targetIdx].key == key) {
            node->entries[targetIdx].ptr.record = record;

            cursor->isValid = True;
                 
            /*Set index of pointer in current node that leads to next ancestor*/
            cursor->ancestorsIdx[level] = targetIdx;
            cursor->level = level;
            
            newEntryFromChild->ptr.child = NULL;

            return True;
        }

        /* if the leaf has space, insert key and record */
        if (node->numKeys < FANOUT) {
            int i;

            /* Find first entry which newEntry is greater than or equal to
             * key goes to the right of this index. */
            const int targetIdx = findChildIndex(node->entries, key, node->numKeys);


            /* Move all entries to the right of targetIdx one to the right*/
            for (i = node->numKeys; i > targetIdx + 1; i--) {
                /* move keys and pointers to the right */
                node->entries[i] = node->entries[i - 1];
            }

            /* insert entry */
            node->entries[targetIdx + 1].key = key;
            node->entries[targetIdx + 1].ptr.record = record;
            node->numKeys++;

            newEntryFromChild->ptr.child = NULL;


            /* Update cursor */
            cursor->isValid = True;
            cursor->relation->numRecords++;
            
            /*Set index of pointer in current node that leads to next ancestor*/
            cursor->ancestorsIdx[level] = targetIdx + 1;
            cursor->level = level;
           
            return True;
        }
        /* else split the leaf */
        else {

            /* Create an Array that can hold all entries. */
            Entry allEntries[FANOUT + 1];
            BtNode* newNode;
            int i, j;
            /* let the first node hold the floor of half of the FANOUT*/
            int firstNodeSize = (FANOUT / 2);

            /* index of first key that new Key is greater than or equal to  */
            const int targetIdx = findChildIndex(node->entries, key, node->numKeys);

            /* copy 0 to targetIdx of node's keys to allEntries, insert 
             * newKeyFromChild, copy remaining targetIdx to MAXNUMKEYS - 1 
             * keys to allEntries */
            j = 0;
            for (i = 0; i < FANOUT + 1; i++) {
                if (i == (int) targetIdx + 1) {
                    allEntries[i].key = key;
                    allEntries[i].ptr.record = record;
                } else {
                    allEntries[i] = node->entries[j];
                    j++;
                }
            }

            /* overwrite current node with floor of first half of keys and 
             * pointers. 
             * TODO: Optimize later! Unnecessary copying over.
             */
            for (i = 0; i < firstNodeSize; i++) {
                node->entries[i] = allEntries[i];
            }
            node->numKeys = firstNodeSize;

            /* create a new leaf node to contain second half (including entry at 
             * newEntryIdx) of split keys.
             * copy them to new node */
            newNode = createNewNode(True);
            /* TODO: handle memory error better*/
            assert(newNode);
            j = 0;
            for (i = firstNodeSize; i < FANOUT + 1; i++) {
                newNode->entries[j] = allEntries[i];
                j++;
            }
            newNode->numKeys = j;
            newNode->ptr0 = NULL;

            /*Set newEntryFromChild*/
            newEntryFromChild->key = newNode->entries[0].key;
            newEntryFromChild->ptr.child = newNode;


            /* If this leaf is the root level, create new root, let it point to this 
             * node and new Child entry. A new level has been created so move 
             * all ancestor nodes and next ancestor ptr index to down one level.
             */
            if (relation->root == node) {
                BtNode* newRootNode = createNewNode(False);
                assert(newRootNode);

                newRootNode->ptr0 = node;
                newRootNode->numKeys = 1;
                newRootNode->entries[0] = *newEntryFromChild;

                relation->root = newRootNode;

                newEntryFromChild->ptr.child = NULL;
                                                
                /* New level 0 ancestor, levels have increased*/
                cursor->ancestors[0] = relation->root;    
                cursor->level++;

                if (targetIdx < firstNodeSize) {
                    cursor->ancestorsIdx[0] = -1;
                } else {
                    cursor->ancestorsIdx[0] = 0;
                }          
                
            }

            /* If target index is less than firstNodeSize, then new key in 
             * first node */
            if (targetIdx + 1 < firstNodeSize) {
                /* nextAncestorIdx of parent might be new if*/
                cursor->ancestors[cursor->level] = node;
                cursor->ancestorsIdx[cursor->level] = targetIdx + 1;
            }                
            /* Else, it is in the second node.*/
            else {
                
                /* nextAncestorIdx of parent might be new if*/
                cursor->ancestors[cursor->level] = newNode;
                cursor->ancestorsIdx[cursor->level] = targetIdx - (firstNodeSize - 1);
            }

            cursor->isValid = True;
            cursor->relation->numRecords++;

            return True;

        }
    }
}


/* Algorithm from page 262 of Database Management Systems Second Edition. 
 * Delete key and record from the appropriate leaf in nodePtr. Use parentNode
 * to get siblings if redistribution or merging of children nodes is necessary.
 * Use oldEntryFromChild to detect if a merge occurred in the current node's 
 * child. It is a constant pointer to an entry. It should be filled in with the 
 * correct value. It contains a key and its pointer. It is the entry in the 
 * current node that points into the (right) child node merged into its (left) 
 * sibling node.
 *  
 * Return True on success. Return False on failure.
 * TODO: Ancestor tracking code.
 */
static Bool deleteKeyRecord(BtNode* parentNode, BtNode* node, unsigned long key,
        Entry* const oldEntryFromChild, Cursor* cursor, Relation_T relation, 
        const int level){
    
    Bool success;
    int i;
    
    /* Index of pointer to child tree containing key to be deleted */
    int childTreePtrIdx;
    
    assert(node != NULL);
    assert(cursor != NULL);
    /* Container to store new entry should never be NULL.*/
    assert(oldEntryFromChild != NULL);
    ASSERT_NODE_INVARIANT(node, relation);
        
    if (node->isLeaf == False) {
        BtNode* childNode;
        
        /* Get index of child node that contains. */
        childTreePtrIdx = findChildIndex(node->entries, key, node->numKeys);
        
        /* Get child node*/
        if (childTreePtrIdx == -1) {
            childNode = node->ptr0;
        } else {
            childNode = node->entries[childTreePtrIdx].ptr.child;
        }
                
        /* Recursively delete */
        success = deleteKeyRecord(node, childNode, key, 
                oldEntryFromChild, cursor, relation, level+1);
                
        /* if delete failed, return that delete failed */
        if (success == False) {
            return False;
        }           
        
        /* else if successful and no merges, return success.*/
        if (oldEntryFromChild->ptr.child == NULL) {
            return success;
        }
        
        /* Handle case where entry / key has to be deleted in node because
         * children were merged. */
        return handleDeleteOfEntry(parentNode, node, 
                oldEntryFromChild, cursor, relation, level);    
        
    } 
    /* Handle delete in child. */
    else {
        for (i = 0; i < node->numKeys; i++) {
            if (node->entries[i].key == key) {
                *oldEntryFromChild = node->entries[i];
                return handleDeleteOfEntry(parentNode, node, oldEntryFromChild,
                        cursor, relation, level);
            }
        }
        return False;
    }
}


/* Code to handle deletion of entry in non-leaf or leaf node when children 
 * nodes have been merged. 
 * TODO: ancestor tracking code.*/
static Bool handleDeleteOfEntry(BtNode* parentNode, BtNode* node, 
        Entry* const oldEntryFromChild, Cursor* cursor,
        Relation_T relation, const int level) {

    int i, idx;
    Bool found = False, wasMerged = False;
    BtNode* leftSibling = NULL; 
    BtNode* rightSibling = NULL;

    assert(node != NULL);
    assert(cursor != NULL);
    assert(oldEntryFromChild != NULL);
    
    /* find the entry that matches oldEntry.*/
    for (i = 0; i < node->numKeys; i++) {
        /* if these entries match */
        if ((oldEntryFromChild->key == node->entries[i].key) &&
                (oldEntryFromChild->ptr.child == node->entries[i].ptr.child)) {
            idx = i;
            found = True;
            break;
        } 
    }
    
    assert(found == True);
    
    /* if found, delete the entry */
    for(i = idx; i < node->numKeys - 1; i++) {
        node->entries[i] = node->entries[i+1];
    }
    node->numKeys--;
    
    /* MAKE SURE ENTRY IS NOT ROOT. Root is allowed to have less than FANOUT / 2
     * entries. */
    
    /* If this is root. */
    if(relation->root == node) {
        /* if the root is now empty. and this is not a leaf. */
        if (node->numKeys == 0 && node->isLeaf == False) {
            /* Set the new root. */
            relation->root = node->ptr0;
            /* Free the old root. */
            free(node);
        }
        /* Otherwise if this is a leaf, leave leaf node as root for insertion.*/
        
        /* set oldEntryFromChild('s ptr) to NULL. */
        oldEntryFromChild->ptr.child = NULL;
        return True;  
    }
    
    
    /* if this not the root and the node had enough entries, set old entries 
     * pointer to null to indicate that there is no node deleted at this level.
     */
    if (node->numKeys >= FANOUT/2) {
        oldEntryFromChild->ptr.child = NULL;
        return True;
    }
    
    /* Else redistribute or merge. */
    
    found = False;
    for(i=0; i < parentNode->numKeys; i++) {
        if(parentNode->entries[i].ptr.child == node) {
            found = True;
            idx = i;
            break;
        }
    }
    
    if (parentNode->ptr0 != node) {
        assert(found == True);
    }
    
    /* First get sibling(s). Non-root nodes must have at least one sibling. */
    /* if node is leftmost or rightmost child, only one sibling */
    if(parentNode->ptr0 == node) {
        rightSibling = parentNode->entries[0].ptr.child;
    } 
    else if (parentNode->entries[parentNode->numKeys - 1].ptr.child == node) {
        leftSibling = parentNode->entries[parentNode->numKeys - 2].ptr.child;
    } 
    /* otherwise, one or two siblings*/
    else {
        
        /* Get left sibling. */
        if(idx == 0){
            leftSibling = parentNode->ptr0;
        } else {
            leftSibling = parentNode->entries[idx - 1].ptr.child;
        }
        
        /* Get right sibling. */
        if(idx + 1 < parentNode->numKeys) {
            rightSibling = parentNode->entries[idx + 1].ptr.child;
        }
    }
    
    /* There must be a sibling as this is not the root. */
    assert(leftSibling != NULL || rightSibling != NULL);
    
    /* Now attempt redistribute or merge using node and smallest sibling. We use
     * the smallest sibling as this increases the chance of redistributions 
     * which is more favorable as redistributions do not propagate*/
    
    if(rightSibling == NULL) {
        rightSibling = node;
    } else if (leftSibling == NULL) {
        leftSibling = node;
    } else if (leftSibling->numKeys <= rightSibling->numKeys) {
        rightSibling = node;
    } else {
        leftSibling = node;
    }
    
    /* Find splitting parent entry. */
    found = False;
    for(idx = 0; idx < parentNode->numKeys; idx++) {
        if(parentNode->entries[idx].ptr.child == rightSibling) {
            found = True;
            break;
        }
    }
    
    assert(found == True);
    
    redistributeOrMerge(leftSibling, rightSibling, &(parentNode->entries[idx]), 
                        leftSibling->isLeaf, &wasMerged);
    
    /* If nodes weren't merged, set oldEntryFromChild to false, return True*/
    if(wasMerged == False){
        oldEntryFromChild->ptr.child = NULL;
        return True;
    }
    /* Else splitting entry has to be deleted in parent. And right node
     * should be freed as its entries have been merged into left node. */
    *oldEntryFromChild = parentNode->entries[idx];
    free(rightSibling);
    return True;
}


/* Redistributes entries between siblings if one sibling has 
 * enough entries to spare. If this is not possible, it merges both siblings.
 * Both siblings must have at least FANOUT / 2 entries. If the nodes were 
 * merged, set wasMerged to True. Otherwise, set wasMerged to False. 
 * isLeaf is True if the left and right nodes are leaf nodes, otherwise the 
 * nodes are non-leaf nodes. parentEntry is a pointer to the entry splitting both nodes in 
 * the parent node. Algorithm from Database Management Systems Textbook. 
 * 
 * TODO: redistribution code can be optimized to copy right amount at once 
 * instead of using while loops.
 */
static void redistributeOrMerge(BtNode* leftNode, BtNode* rightNode,
        Entry* const parentEntry, Bool isLeaf,  Bool* wasMerged) {
    int totalKeys = leftNode->numKeys + rightNode->numKeys;
    int i;
    
    assert(leftNode != NULL);
    assert(rightNode != NULL);
    /* Assert there are enough keys for redistribution or a merge.*/
    assert(totalKeys >= FANOUT / 2);
    
    /* Algorithm for redistributing or merging in non-Leaf. */
    if (isLeaf == False) {
        /* If the total number of keys is greater than FANOUT, we can and 
         * should redistribute. */
        if(totalKeys >= FANOUT) {
            /* if the left node has less keys, redistribute from right to left. */
            if (leftNode->numKeys < rightNode->numKeys) {
                /* while the left node is smaller than the right node, redistribute*/
                while(leftNode->numKeys < rightNode->numKeys){
                    /* copy parentEntry key into new space on left node*/
                    leftNode->entries[leftNode->numKeys].key = parentEntry->key;
                    /* copy right node's key into parentEntry key*/
                    parentEntry->key = rightNode->entries[0].key;

                    /* Move Pointers appropriately. */
                    leftNode->entries[leftNode->numKeys].ptr.child = rightNode->ptr0;
                    rightNode->ptr0 = rightNode->entries[0].ptr.child;
                    
                    /* Delete first entry*/
                    for (i = 1; i < rightNode->numKeys; i++) {
                        rightNode->entries[i-1] = rightNode->entries[i];
                    }
                    leftNode->numKeys++;
                    rightNode->numKeys--;
                }
            }
            /* else right node has less keys, so redistribute from left to right. */
            else {
                /* while the right node is smaller than the left node, redistribute*/
                while(rightNode->numKeys < leftNode->numKeys){
                    /* make space for a new entry on the right node */
                    for (i = rightNode->numKeys; i > 0; i--) {
                        rightNode->entries[i] = rightNode->entries[i-1];
                    }

                    /* copy parentEntry key into new space on right node*/
                    rightNode->entries[0].key = parentEntry->key;
                    /* copy left node's last key into parentEntry key*/
                    parentEntry->key = leftNode->entries[leftNode->numKeys-1].key;
                    
                    /* Move Pointers appropriately. */
                    rightNode->entries[0].ptr.child = rightNode->ptr0;
                    rightNode->ptr0 = leftNode->entries[leftNode->numKeys-1].ptr.child;
                    

                    leftNode->numKeys--;
                    rightNode->numKeys++;
                }
            }

            *wasMerged = False;
        }
        /* else if not enough total keys for two nodes, merge both nodes. */
        else {
            /* First copy parent splitting key and left most pointer in right node
             * as new entry in left node. */
            leftNode->entries[leftNode->numKeys].key = parentEntry->key;
            leftNode->entries[leftNode->numKeys].ptr.child = rightNode->ptr0;
            leftNode->numKeys++;

            /* Move all entries from right node and add them to left node. */
            for(i = 0; i < rightNode->numKeys; i++) {
                leftNode->entries[leftNode->numKeys] = rightNode->entries[i];
                leftNode->numKeys++;
            }

            *wasMerged = True;
        }
    }
    /* Algorithm for redistributing or merging in leaf. */
    else {
        /* If the total number of keys is greater than FANOUT, we can and 
         * should redistribute. */
        if(totalKeys >= FANOUT) {
            /* if the left node has less keys, redistribute from right to left. */
            if (leftNode->numKeys < rightNode->numKeys) {
                /* while the left node is smaller than the right node, redistribute*/
                while(leftNode->numKeys < rightNode->numKeys){
                    /* copy entry from the right node to the left node. */
                    leftNode->entries[leftNode->numKeys] = rightNode->entries[0];
                   
                    /* Delete first entry*/
                    for (i = 1; i < rightNode->numKeys; i++) {
                        rightNode->entries[i-1] = rightNode->entries[i];
                    }
                    
                    /* Set new splitting key in parent. */
                    parentEntry->key = rightNode->entries[0].key;
                    
                    leftNode->numKeys++;
                    rightNode->numKeys--;
                }
            }
            /* else right node has less keys, so redistribute from left to right. */
            else {
                /* while the right node is smaller than the left node, redistribute*/
                while(rightNode->numKeys < leftNode->numKeys){
                    /* make space for a new entry on the right node */
                    for (i = rightNode->numKeys; i > 0; i--) {
                        rightNode->entries[i] = rightNode->entries[i-1];
                    }
                    
                    /* copy entry from the left node to the right node. */
                    rightNode->entries[0] = leftNode->entries[leftNode->numKeys-1];
                    
                    /* set new splitting key in parent. */
                    parentEntry->key = rightNode->entries[0].key;

                    leftNode->numKeys--;
                    rightNode->numKeys++;
                }
            }

            *wasMerged = False;
        }
        /* else if not enough total keys for two nodes, merge both nodes. */
        else {

            /* Move all entries from right node and add them to left node. */
            for(i = 0; i < rightNode->numKeys; i++) {
                leftNode->entries[leftNode->numKeys] = rightNode->entries[i];
                leftNode->numKeys++;
            }

            *wasMerged = True;
        }
    }
    
}



/* Given an array of entries, find the index of the last entry whose key is
 * less than or equal to the search key. */
static int findChildIndex(const Entry* entries, unsigned long key, int length) {
    int i = 0;

    assert(entries != NULL);
    assert(length > 0);

    /* if key less than first element, return index of first*/
    if (key < entries[0].key) {
        return -1;
    }
    /* else see if key falls in between any two keys, return index of first key*/
    for (i = 0; i <= length - 2; i++) {
        if (key >= entries[i].key && key < entries[i + 1].key)
            return i;
    }
    /* if key greater or equal to last element, return index of last element*/
    return length - 1;
}

/* Given an array of entries, find the index of the first entry whose key is
 * greater than or equal to the search key. */
static int findRecordIndex(const Entry* entries, unsigned long key, int length) {
  int i = 0;

  assert(entries != NULL);
  assert(length > 0);

  for (i = 0; i <= length - 2; i++) {
    if (key <= entries[i].key)
      return i;
  }

  /* what should we retun when strictly greater than the last key? */
  return length - 1;
}


/* move cursor to key in node. On finding key's record, return True and update cursor. 
 * If relation empty or key not in B+-tree, return False */
static Bool moveToKey(BtNode* node, unsigned long key, Cursor* cursor, 
        const int level) {
    cursor->ancestors[level] = node;
   
    if (node->isLeaf) {
        int i;
        
        if (node->numKeys == 0) {
            /* Should never have a non-root leaf node with zero numkeys*/
            assert(level == 0);
            cursor->isValid = False;
            return False;
        }
        
        i = findRecordIndex(node->entries, key, node->numKeys);
	
	/* if the key is greater than the last key at the correct Leaf, then it is greater than ay key in the tree */
	if (key > node->entries[node->numKeys -1].key) {
	  cursor->isValid = False;
	} else {
	  cursor->isValid = True;
	}
	
        cursor->level = level;
	cursor->ancestorsIdx[level] = i;
	
        if (node->entries[i].key == key) {
            /* key at cursor loc is same as desired key*/
	    return True;;
        } else {
            /* key at cursor loc less than desired key*/
            return False;
        }
        
        
    } else {
        int i;
        BtNode* child;
        
        i = findChildIndex(node->entries, key, node->numKeys);
        cursor->ancestorsIdx[level] = i;
        
        if (i == -1) {
            child = node->ptr0;
        } else {
            child = node->entries[i].ptr.child;
        }
        
        return moveToKey(child, key, cursor, level + 1);
    }
}

/* Move to the first record in the B-tree*/
static Bool moveToFirstRecord(BtNode* node, Cursor* cursor, int level) {
    assert(node != NULL);
    assert(cursor != NULL);
    assert(level >= 0);
    
    /* Track ancestors as you go to first node*/
    cursor->ancestors[level] = node;
    cursor->ancestorsIdx[level] = -1;
            
    if (node->isLeaf) {
        if (node->numKeys == 0) {
	  /* A Leaf Node with no key can only be the root of an empty tree */
	    cursor->ancestorsIdx[level] = 0;
	    cursor->level = 0;
	    cursor->isValid = False;
            return False;
        }
        
        cursor->ancestorsIdx[level] = 0;
        cursor->level = level;
        cursor->isValid = True;
        return True;
    }
    
    return moveToFirstRecord(node->ptr0, cursor, level+1);
}

static void handleDeleteBtree(BtNode* node, void (* freeRecord)(void *)) {
    int i;
    
    /* Part of base case. In leaf node, free all records with 
     * freeRecord function, if any. */
    if (node->isLeaf) {
        if (freeRecord == NULL){
            return;
        }
        
        for (i = 0; i < node->numKeys; i++) {
            freeRecord((void *)node->entries[i].ptr.record);
        }
        
        return;
    }
    
    /* Recursively delete every child subtree. If the child is a leaf, after 
     * recursively deleting every child subtree, free the node. */
    handleDeleteBtree(node->ptr0, freeRecord);
    if (node->ptr0->isLeaf) {
        free(node->ptr0);
    }
    for(i = 0; i < node->numKeys; i++) {
        handleDeleteBtree(node->entries[i].ptr.child, freeRecord);
        if (node->entries[i].ptr.child->isLeaf) {
            free(node->entries[i].ptr.child);
        }
    }
}

/* Chck node size invariants */
static void ASSERT_NODE_INVARIANT(BtNode* node, Relation_T relation) {
    
    /* If not root, must have at least fanout / 2 keys*/
    if(relation->root != node) {
        assert(node->numKeys >= FANOUT / 2);
    }
    assert(node->numKeys <= FANOUT);
}

/* TODO: refactor out common code etc. maybe create library for them 
 * TODO: pass in schema object to btree on initialization to determine 
 * types of record. Also pass in schema type into record init so btree can 
 * assert that an input record is of correct type. 
 * careful of typedefs and all
 * 
 * Todo: notify cursors on update.
 * TODO: make some strings const??
 * Make indices const were possible
 * check for use of semi colons
 * 
 * What id we pass max index level? weird cases like that.. check for such
 * bugs.
 * 
 * Todo leaf would split 15 to 7 and 9
 */

static void printTree(BtNode* node, int level) {
    int i;
    
    if(node->isLeaf) {
        fprintf(stderr, "Level: %d ", level);
        for(i = 0; i < node->numKeys; i++) {
            fprintf(stderr, " %lu", node->entries[i].key);
        }
        fprintf(stderr, "\n");
        return;
    }
    
    fprintf(stderr, "Level: %d ", level);
    for(i = 0; i < node->numKeys; i++) {
        fprintf(stderr, " %lu", node->entries[i].key);
    }
    fprintf(stderr, "\n");
    
    printTree(node->ptr0, level + 1);
    for(i = 0; i < node->numKeys; i++) {
        printTree(node->entries[i].ptr.child, level+1);
    }
}
