Panagiotopoulos_Georgios_1115201700113


HEAP FILE:
    HP_info: It keeps the file descriptor of course but also the index of the last block as well as the
    maximum records that can be stored in a heap block. Since in this data structure we fill block after 
    block the last block is the only one we are interested at any given moment when inserting.
    The maximum records is also a very useful information for easy checking if we need to allocate another block
    when inserting

    HP_block_info: It just keeps track of the number of records that it has currently. 

    Create: Allocates the first block and initializes the variables of the HP_info structure.
    I use the floor() function of the math.h in order to ensure that I don't overestimate the number of 
    records, which would cause problems, contrary to underestimating. 

    Open: I allocate a bit of memory that carries our metadata, so we are always able to reach the file_descriptor mainly. 
    The memory gets freed with on the close function.

    Close: Frees the external hp_info structure as well as closing the file

    InsertEntry: Here I use a pointer to the hp_info structure for all the elements of the structure that need to change and updated mid-call, so 
    as to not call memcpy again and again. For the elements that remain unchanged I still use the external hp_info structure that I malloced. Although
    it is a bit of a waste of space, the only thing I essentially needed the external structure for was the file descriptor.
    I also define again and again the block info margin, which basically points at the end of the space reserved for records and at the start of the space 
    reserved for the metadata. I define it again at the start of the GetAllEntries.
    The insertion itself works as described in the lectures. We check the last block for space, if there isn't any or if the last block is also block0 
    which only carries the metadata, we allocate a new block to the file and insert the record there.

    GetAllEntries: This basically functions as a double for loop, where we iterate through all the blocks and through all the records inside the blocks.

HASH FILE:
    HT_info: This now also holds the number of buckets and the hashtable

    HT_block_info: This now holds, along with the number of records, the index of the matched bucket and the overflowed block before it, if there is one

    hash: Just a hash function

    Create: Again allocates the first block to the file and initializes the variables. 
    Most important thing now is that we need to initialize the hashtable, so that it has an actual size

    Open: Again I allocate a bit of memory for the external structure holding our metadata. 
    This time I need to allocate space for the hashtable also, so I call a pointer to the metadata structure first
    in order to reach that information. The values of the hashtable are also initialized with bucket i matching with block i+1, 
    so as to avoid matching any bucket with block 0.

    InsertEntry: On this I use exclusevely the external strucure for all the "calculations" since we only need to update it once. 
    In the end I just memcpy all the updated data from the external structure to the structure of block0 and then dirty it up.
    Of course now the insertion is a bit more sophisticated, the records are inserted according to the bucket that is assigned to 
    them by the hash function. Every bucket is matched to a certain block but if that block fills up, a new one is allocated,
    which keeps the index of the overflowed one and the bucket points to the new one instead, so that any new records go directly to fill
    the new one.

    GetAllEntries: A bit more efficient this time, we can immediately narrow down the search by getting the bucket from the hash function. 
    That bucket leads us to some blocks, so then the only thing we need to do is search only those blocks for our entry. Here I've also used a 
    helper function HT_GetRecordinBlock, that basically returns if a record is on a certain block or not. If it is then it prints it as well. 
    All we need to do then is iterate through the overflowed blocks calling this helper function with each iteration.


SECONDARY HASH FILE:
    Identical to the HASH file in almost every way. Of course it has each own hash function, based on the name of the record this time. 
    It also has its own structure Duplet, which basically matches a name with its block in the hash file. Those Duplets basically 
    replace our initial `records` and we function only with them. 
    The most different thing is the search. Here I have defined yet another helper function GetMainBlock, which given a certain name 
    and its block in the secondary hashtable, returns the block in the main hash file, by searching through all the entries in that block. 
    Then a similar helper function as the HT_GetRecordinBlock, only now it compares names instead of ids. If it finds the name in the specific block
    it returns true and prints the entire record. 

    Finally the main GetAllEntries function, finds the bucket and iterates through all the blocks in that bucket utilizes the two functions described above in 
    conjuction with one another. 

    Also, in the Create and Open I never use the main HT file cause we are to assume that it is empty and it has no entries, 
    meaning that we only need to worry about the entries to come, so there is really nothing to do with it. Especially since 
    the SecondaryInsert is given the main block in its call. 


The functions I tested with the example tests that were provided as well as with the obligatory prints for debugging.

