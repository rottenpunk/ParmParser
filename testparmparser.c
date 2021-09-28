//-----------------------------------------------------------------------------
//  testprmp -- Test parameter parsing...
//                John Overton
//
//
//-----------------------------------------------------------------------------
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
 
#include "parmparser.h"     // Parameter Parsing routines.
 
// For the worst-case debugging scenario...
#define blow()   {int* p = 0; *p = 1;}
 
 
 
 
 
//-----------------------------------------------------------------------------
// This routine finds a couple of specific nodes...
//-----------------------------------------------------------------------------
void testSearchNodes(void* handle)
{
    char* value;
    int   type;
 
    parmSetBegin( handle);
 
    type = parmFindKey( handle, "password", &value);
    printf("looking for password. Type: %d Value: %s\n", type, value);
 
    type = parmFindKey( handle, "download", &value);
    printf("looking for first download. Type: %d Value: %p\n", type, value);
    parmLevelDown( handle );
    type = parmFindKey( handle, "from", &value);
    printf("looking for from. Type: %d Value: %s\n", type, value);
    parmLevelUp( handle );
 
    type = parmFindNextKey( handle, "download", &value);
    printf("looking for next download. Type: %d Value: %p\n", type, value);
    parmLevelDown( handle );
    type = parmFindKey( handle, "from", &value);
    printf("looking for from. Type: %d Value: %s\n", type, value);
    parmLevelUp( handle );
 
}
 
 
 
//-----------------------------------------------------------------------------
// This routine just traverses and prints all nodes...
//-----------------------------------------------------------------------------
void printNodes(void* handle)
{
    char* key;
    char* value;
    int   type;
 
    while( (type = parmGetNext( handle, &key, &value)) != PRMP_END) {
        printf("Type: %d\n", type);
        if( type == PRMP_STRING ) {
            printf("key: %s value: %s\n", key, value);
        } else if( type == PRMP_NEXTLEVEL ) {
            printf("Key: %s -- Going down a level\n", key);
            parmLevelDown( handle );
            printNodes( handle );
            printf("Going up a level\n");
            parmLevelUp( handle );
        } else {
            printf( "Invalid type received: %d\n", type);
            return;
        }
    }
}
 
 
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int  main(int argc, char* argv[])         
{
    int rc;
    void* handle;
 
    rc = parmParseFile( &handle, "testprms.ini" );
 
    printf("rc from parmParseFile: %d\n", rc);
 
    printf("Traverse nodes...\n");
    parmSetBegin( handle);
    printNodes( handle );
 
    printf("Try to find specific nodes...\n");
    testSearchNodes( handle );
 
    return 0;
}
 
 
 
