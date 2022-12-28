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
  printf("%d Created\n", __LINE__);
  //CALL_OR_DIE(BF_CreateFile(fileName));


  int sfile_desc,file_desc;
  CALL_OR_DIE(BF_OpenFile(sfileName, &sfile_desc));
  printf("%d Opened\n", __LINE__);
  BF_Block* block;
  BF_Block_Init(&block);

  CALL_OR_DIE(BF_AllocateBlock(sfile_desc, block));
  printf("%d Allocated block\n", __LINE__);
  void* data = BF_Block_GetData(block);
  SHT_info* sht_info = data;

  sht_info->file_desc = sfile_desc;
  sht_info->numBuckets = buckets;
  printf("%d Buckets: %d\n", __LINE__, buckets);
  sht_info->num_o_blocks = 1;
  sht_info->max_duplets = floor((BF_BLOCK_SIZE - sizeof(SHT_block_info))/sizeof(Duplet)); 
  //sht_info->hashtable = malloc(buckets * sizeof(int));
  //printf("%d hashtable:%ld\n", __LINE__,sizeof(sht_info->hashtable[buckets]));

  BF_Block_SetDirty(block);
  CALL_OR_DIE(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);
  printf("%d Freed block\n", __LINE__);
  return 0;
}

SHT_info* SHT_OpenSecondaryIndex(char *indexName){
  int file_desc, i;
  void* data;
  CALL_OR_DIE(BF_OpenFile(indexName, &file_desc));
  printf("%d Opened file\n", __LINE__);

  BF_Block* block0;
  BF_Block_Init(&block0);
  CALL_OR_DIE(BF_GetBlock(file_desc,0,block0));
  printf("%d Got first block\n", __LINE__);

  void* d = BF_Block_GetData(block0);
  SHT_info* p_info = d; //we need this just to get the number of buckets so we can properly initialize the structure 

  //...something went horribly wrong with using static, back to malloc, could't avoid it afterall
  SHT_info* sht_info = malloc(sizeof(SHT_info)+(p_info->numBuckets*sizeof(int))); //allocating memory size of struct plus the size of our hashtable
  memcpy(sht_info,d,sizeof(SHT_info));
  printf("%d Copying the metadata\n", __LINE__);

  int block_info_margin = sht_info->max_duplets*sizeof(Duplet); //literally just duplets instead of records

  for(i = 0; i < sht_info->numBuckets; i++){
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_OR_DIE(BF_AllocateBlock(file_desc, block));

    printf("%d Allcate temp block\n", __LINE__);

    data = BF_Block_GetData(block);
    SHT_block_info* block_info = data+block_info_margin;

    printf("%d Get the info margin\n", __LINE__);

    block_info->num_o_duplets = 0;
    block_info->bucket = i;
    sht_info->hashtable[i] = i+1; // we add one since we can't have any bucket match with block0, this block is for our metadata only
    printf("%d Trying to initialize the hashtable\n", __LINE__);

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
  printf("%d Closing file\n", __LINE__);
  CALL_OR_DIE(BF_CloseFile(sht_info->file_desc));
  free(sht_info);
  return 0;
}

int SHT_SecondaryInsertEntry(SHT_info* sht_info, Record record, int block_id){
  
}

int SHT_SecondaryGetAllEntries(HT_info* ht_info, SHT_info* sht_info, char* name){

}



