# parmparser -- Generalized parameter parsing module
## Purpose:
Parse out a parameter file and build a multilevel parameter table.  

As I was building different types of systems, I realized that I needed a simple utility that could 
parse out parameter files and save them in memory in an easy way to get to the parameters.  I looked
at yaml parsers and json parsors, but they were way to complicated for what I generally needed in my 
projects.  So, I came up with a simple syntax of key/value pairs, where the value could be another
level of key/value pairs. 

## Description:
 * Parameters in the parameter file are key-value pairs 
 * A colon separates the key and the value
 * Keys should start at the beginning of a line (mostly for readability)
 * The value can be another level of key-value pairs 
 * The next level is surrounded by curly brackets 
 * The string to parse is a stream of characters  
 * Key or value strings cannot have embedded spaces unless the string is enclosed in qoutes 
 * There can be duplicate keys within a level.
 * You can "traverse" the key/value pairs. Every call to parmGetNext() will go to next key/value
at the current level
 * When you traverse, you stay at the current level
 * If you traverse to a key whose value is another level, you can call parmLevelDown() to traverse
through the value's key/value pairs.  Use parmLevelUp() to go back up and continue traversing where
you left off.
 * You can find specific keys within a level by using parmFindKey() and parmFindNextKey()
 * At any time you can call parmSetBegine

## An example of a parameter file:

    # Testing basic uploading and downloading
    email: john.overton@someplace.com
    password: d&*$(#02$
    download: {
       from: "John Overton/Other Things/httpclient-tutorial.pdf"
       to:   "C:/Users/joverton/Desktop/"
       translate: no
    }
    download: {
       from: "Shared/Division/Company/Rebit.docx"
       to:   "C:/Users/joverton/Desktop/"
       translate: no
    }
    upload: {
       from: "document1.pdf"
       to:   "Shared/Development/Development Teams/testing/"
       translate: no
    }


In the example above, the keys "email" and "password" are pretty obvious key/value examples.  
Then, the key "download" has its own set of key/value pairs enclosed in curly brackets.
Strings are generally enclosed in quotes and especially if there are embedded spaces.  
There can be multiple keys; in this example, there are two "download" keys.  There are
functions to help traverse the keys, which are kept in order in the in-memory table, once parsed.

## Functions:


`int parmParseFile(void** handle, char* filename);`
 
Parse out parameter file and return handle and results. 

`int parmSetBegin(    void* handle);`

Prepare for traversing the parameters. Just reset pointers in handle. This can be              
called at any time to restart traversing the key/value pairs from the beginning.

`int parmGetNext(     void* handle, char** key, char** value);`

Return the next key/value pair.  Returns the "type" of the value, which can be 
PRMP_END, PRMP_STRING or PRMP_NEXTLEVEL.


`int parmLevelDown(   void* handle );`

When you have traversed to a key/value whose value is another set of key/value pairs,
prepare to traverse through the value's key/value pairs. This function will return a
-1 if you are not currently at a key/value pair whose value is not another set of 
key/value pairs.

`int parmLevelUp(     void* handle );`
 
After you have completed traversing (or searching) through a value's key/value pairs,
go back up a level to continue traversing (or searching) where you left off

`int parmFindKey(     void* handle, char* key, char** value);`

Find first key within a level. If not found, PRMP_END is returned.

`int parmFindNextKey( void* handle, char* key, char** value);`

Find first/next key within a level.  If not found, PRMP_END is returned.


