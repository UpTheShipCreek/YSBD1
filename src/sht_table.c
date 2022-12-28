#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bf.h"
#include "sht_table.h"
#include "ht_table.h"
#include "record.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

int secondary_hash(char* name, SHT_info* header_info){
    int hash = 0;
    for (int i = 0; i < strlen(name); i++) {
        hash = hash + (int)name[i];
    }
    return hash % header_info->numBuckets;
}

int SHT_CreateSecondaryIndex(char *sfileName,  int buckets, char* fileName){
  //What do we need the Hashfile for, if we are to treat it as if it were empty? Not me, I don't need it.
  CALL_OR_DIE(BF_CreateFile(sfileName));
  //printf("%d Created\n", __LINE__);
  //CALL_OR_DIE(BF_CreateFile(fileName));


  int sfile_desc,file_desc;
  CALL_OR_DIE(BF_OpenFile(sfileName, &sfile_desc));
  //printf("%d Opened\n", __LINE__);
  BF_Block* block;
  BF_Block_Init(&block);

  CALL_OR_DIE(BF_AllocateBlock(sfile_desc, block));
  //printf("%d Allocated block\n", __LINE__);
  void* data = BF_Block_GetData(block);
  SHT_info* sht_info = data;

  sht_info->file_desc = sfile_desc;
  sht_info->numBuckets = buckets;
  //printf("%d Buckets: %d\n", __LINE__, buckets);
  sht_info->num_o_blocks = 1;
  sht_info->max_duplets = floor((BF_BLOCK_SIZE - sizeof(SHT_block_info))/sizeof(Duplet)); 
  //sht_info->hashtable = malloc(buckets * sizeof(int));
  ////printf("%d hashtable:%ld\n", __LINE__,sizeof(sht_info->hashtable[buckets]));

  BF_Block_SetDirty(block);
  CALL_OR_DIE(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
  //printf("%d Freed block\n", __LINE__);
  return 0;
}

SHT_info* SHT_OpenSecondaryIndex(char *indexName){ //why couldn't all those open functions just return the file descriptor? It's literally all we need
  int file_desc, i;  
  void* data;
  CALL_OR_DIE(BF_OpenFile(indexName, &file_desc));
  //printf("%d Opened file\n", __LINE__);

  BF_Block* block0;
  BF_Block_Init(&block0);
  CALL_OR_DIE(BF_GetBlock(file_desc,0,block0));
  //printf("%d Got first block\n", __LINE__);

  void* d = BF_Block_GetData(block0);
  SHT_info* p_info = d; //we need this just to get the number of buckets so we can properly initialize the structure 

  //...something was going horribly wrong using static, back to malloc lol
  SHT_info* sht_info = malloc(sizeof(SHT_info)+(p_info->numBuckets*sizeof(int))); //allocating memory size of struct plus the size of our hashtable
  memcpy(sht_info,d,sizeof(SHT_info));
  //printf("%d Copying the metadata\n", __LINE__);

  int block_info_margin = sht_info->max_duplets*sizeof(Duplet); //literally just duplets instead of records

  for(i = 0; i < sht_info->numBuckets; i++){
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_AllocateBlock(file_desc, block));

    //printf("%d Allcate temp block\n", __LINE__);

    data = BF_Block_GetData(block);
    SHT_block_info* block_info = data+block_info_margin;

    //printf("%d Get the info margin\n", __LINE__);

    block_info->num_o_duplets = 0;
    block_info->bucket = i;
    sht_info->hashtable[i] = i+1; // we add one since we can't have any bucket match with block0, this block is for our metadata only
    //printf("%d Trying to initialize the hashtable\n", __LINE__);

    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
  }
  memcpy(d,sht_info,sizeof(SHT_info)); //updating the internal structure as well
  CALL_OR_DIE(BF_UnpinBlock(block0));
  BF_Block_Destroy(&block0);
  return sht_info;
}


int SHT_CloseSecondaryIndex(SHT_info* sht_info ){
  //printf("%d Closing file\n", __LINE__);
  CALL_OR_DIE(BF_CloseFile(sht_info->file_desc));
  free(sht_info);
  return 0;
}

int SHT_SecondaryInsertEntry(SHT_info* sht_info, Record record, int block_id){
  ////printf("LETS SEE THEM FUCKING IDS %d\n", block_id);
  void* data;
  int block_info_margin = sht_info->max_duplets*sizeof(Duplet);
  Duplet* duplet = malloc(sizeof(Duplet)); //don't forget to free

  // duplet->name = record.name;
  memcpy(duplet,&record.name,sizeof(record.name)); //the name is 15 characters at most and each character is 1 byte
  duplet->block = block_id;

  int bucket = secondary_hash(duplet->name,sht_info);
  int sblock_id = sht_info->hashtable[bucket];

  BF_Block* block;
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_GetBlock(sht_info->file_desc,sblock_id,block));
  ////printf("%d Loaded block X in memory\n", __LINE__);

  data = BF_Block_GetData(block);
  SHT_block_info* block_info = data + block_info_margin;

  if(block_info->num_o_duplets < sht_info->max_duplets){
    int margin = block_info->num_o_duplets*sizeof(Duplet); 
    memcpy(data+margin, duplet, sizeof(Duplet));
    ////printf("%d Block has enoough space, copied record into block\n", __LINE__);

    block_info->num_o_duplets++;
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    ////printf("%d Destroyed block X, exiting\n", __LINE__);
  }
  else{
    int new_block_id;
    CALL_OR_DIE(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    ////printf("%d Not enough space, destroyed block X\n", __LINE__);

    BF_Block* new_block;
    BF_Block_Init(&new_block);
    CALL_OR_DIE(BF_AllocateBlock(sht_info->file_desc, new_block)); //allocate new block
    CALL_OR_DIE(BF_GetBlockCounter(sht_info->file_desc,&new_block_id));
    ////printf("%d Allocated new block Y\n", __LINE__);

    void* new_data = BF_Block_GetData(new_block);
    SHT_block_info* new_block_info = new_data + block_info_margin; 
    memcpy(new_data, duplet, sizeof(Duplet));

    new_block_info->num_o_duplets=1;
    new_block_info->bucket = bucket;
    new_block_info->previous_block = block_id; // so we must somewhere save the previous block as well
    ////printf("%d Copied the record and updated block's Y metadata\n", __LINE__);
    //the bucket now must try and fill the new block 
    sht_info->hashtable[bucket] = new_block_id-1;  //we subtract one because the block's id is one less than the number of blocks             
    sht_info->num_o_blocks++; //increment the number of blocks
    ////printf("%d New:%d Prev:%d \n", __LINE__,new_block_id,new_block_info->previous_block);
    BF_Block_SetDirty(new_block);
    CALL_OR_DIE(BF_UnpinBlock(new_block));
    BF_Block_Destroy(&new_block);
    ////printf("%d Updated files metadata and destroyed block Y\n", __LINE__);
    sblock_id = new_block_id-1;
  } 
  BF_Block* block0;
  BF_Block_Init(&block0);
  CALL_OR_DIE(BF_GetBlock(sht_info->file_desc,0,block0));
  void* d = BF_Block_GetData(block0);

  memcpy(d,sht_info,sizeof(SHT_info)); //update the internal ht_info structure with the resolution of the operation(The opposite was better for the heap since I didn't need to play with memory in between calls in the equivelent function)
  BF_Block_SetDirty(block0);
  CALL_OR_DIE(BF_UnpinBlock(block0));
  BF_Block_Destroy(&block0);
  ////printf("%d Destroyed block zero, we are done here\n", __LINE__);
  free(duplet); //remembered it, see?
  return sblock_id;
}

int SHT_SecondaryGetAllEntries(HT_info* ht_info, SHT_info* sht_info, char* name){
  int block_info_margin = sht_info->max_duplets*sizeof(Duplet);
  int block_counter = 0;
  int i, block_id,sblock_id;
  SHT_block_info* block_info; 
  Duplet* duplet;
  void* data;
  int bucket = secondary_hash(name, sht_info); //find the bucket

  sblock_id = sht_info->hashtable[bucket]; //find the block

  BF_Block* block;
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_GetBlock(sht_info->file_desc,sblock_id,block));

  data = BF_Block_GetData(block);
  block_info = data + block_info_margin;

  block_id = SHT_GetMainBlock(sht_info, name, sblock_id); //getting the ID of the block in the hashtable
  if(SHT_GetRecordinBlock(ht_info,name,block_id)) block_counter++; //fetching the full entry from the block

  //go through all the blocks associated with the bucket
  int prev = block_info->previous_block;
  while(prev != 0){
    block_id = SHT_GetMainBlock(sht_info, name, prev); //check the main block's id
    //the block counter doesn't count the correct thing, neither here nor on the ht_table
    if(SHT_GetRecordinBlock(ht_info,name,block_id)) block_counter++; //get the full entry from the main database
    prev--;
  } 
  if(block_counter!=0) return block_counter; //this returns the number of blocks on which we found the name instead, if I have time'll fix this
  else return -1;
}

int SHT_GetMainBlock(SHT_info* sht_info, char* name, int sblock_id){
  Duplet* duplet;
  int i, block_id = 0;
  int block_info_margin = sht_info->max_duplets*sizeof(Duplet);
  bool flag = false;

  BF_Block* block;
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_GetBlock(sht_info->file_desc,sblock_id,block));

  void* data = BF_Block_GetData(block);
  SHT_block_info* block_info = data + block_info_margin; //getting the block's metadata the good ol' way

  for(i = 0; i<block_info->num_o_duplets; i++){ //for every duplet
    duplet = data + i*sizeof(Duplet);
    //printf("%d DupName: %s Given: %s\n", __LINE__,duplet->name,name);
    if(strncmp(duplet->name,name,sizeof(name)) == 0){ //meaning if we found the name
      //printf("%d Strcmp: %d\n", __LINE__,strncmp(duplet->name,name,sizeof(name)));
      block_id = duplet->block; //fetch the block
      //printf("%d Block_Id %d\n", __LINE__,block_id);
      break;
    }
  }
  CALL_OR_DIE(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
  //printf("%d Returning with block %d\n", __LINE__,block_id);
  return block_id;
}

bool SHT_GetRecordinBlock(HT_info* ht_info, char* name, int block_id){
  //printf("%d Entering with Block_Id %d\n", __LINE__,block_id);
  Record* record;
  int j;
  int block_info_margin = ht_info->max_records*sizeof(Record);
  bool flag = false;
  BF_Block* block;
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_GetBlock(ht_info->file_desc,block_id,block));

  //printf("%d Got block from HT\n", __LINE__);

  void* data = BF_Block_GetData(block);
  HT_block_info* block_info = data + block_info_margin; //getting the block's metadata the good ol' way

  for(j = 0; j < block_info->num_o_records; j++){ //and for every record in this block
    //printf("%d Searching block's records\n", __LINE__);
    record = data + j*sizeof(Record); 
    //printf("%d Strcmp: %d\n", __LINE__,strncmp(record->name,name,sizeof(name)));
    if(strncmp(record->name,name,sizeof(name)) == 0){ 
      flag = true;
      printf("Id: %d\nName: %s\nSurname: %s\nCity: %s\n", record->id,record->name,record->surname,record->city);
    }
  }
  CALL_OR_DIE(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  return flag;
}


