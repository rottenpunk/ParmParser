//----------------------------------------------------------------------
//                         parm parser
//
// Parse out a parameter file and build a multilevel parameter table.
//
// Parameters in the parameter file are key-value pairs, the value
// can be a pointer to another level of key-value pairs. The string
// to parse is a stream of characters.  Key or value strings cannot
// have embedded spaces unless the string is enclosed in qoutes. There
// can be duplicate keys within a level.
//
//
// An example of a parameter file:
//
// # Testing basic uploading and downloading
// email: john.overton@someplace.com
// password: c&*$(#01$
// download: {
//    from: "John Overton/Other Things/httpclient-tutorial.pdf"
//    to:   "C:/Users/joverton/Desktop/"
//    translate: no
// }
// download: {
//    from: "Shared/Stuff/Rebit.docx"
//    to:   "C:/Users/joverton/Desktop/"
//    translate: no
// }
// upload: {
//    from: "document1.pdf"
//    to:   "Shared/Team/Development Teams/testing/"
//    translate: no
// }
//
//----------------------------------------------------------------------
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
 
typedef int BOOL; 
#define TRUE (!(0))
#define FALSE (0)

 
#include "parmparser.h"
 
 
#define PRMP_MAX_LEVELS  5               // Maximum allowable parameter levels.
 
typedef struct _prmp_handle {
    struct _anchor* anchor;
    void*           gtms;
    struct _anchor* cur_anchor;
    struct _node*   cur_node;
    struct _node*   node_stack[PRMP_MAX_LEVELS];
    int             node_stack_index;
} PRMP_HANDLE;
 

typedef struct _node {
    struct _node* next;
    char* key;
    char  type;
    union {
        char* value;
        struct _anchor* nextlevel;
    };
} PRMP_NODE;
 

typedef struct _anchor {
    struct _anchor* up;
    struct _node* first;
    struct _node* last;
} PRMP_ANCHOR;
 

#define PRMP_STRING_WORK_SIZE  512
 

typedef struct _parse_block {
    FILE*  fs;                    // File "thread" handle.
    char*  next_buf;              // Current buffer that we just read.
    char*  buf;                   // Ptr within next_buf, yet to be parsed.
    int    len;                   // Remaining len to parse in buf;
    BOOL   is_eof;                // We've hit end-of-file.
    BOOL   have_line;             // If false, then we need to get first/next line.
    int    linenbr;               // Current line number (helpful for error msgs).
    int    offset;                // Offset into current line (helpful for error msgs).
    char*  str_wrk;               // A work area for parsed out string.
    int    str_wrk_len;           // Total size of work string.
    int    str_len;               // Size of string in work string.
    void*  gtms;                  // Handle for GTMS temp storage allocation.
    PRMP_ANCHOR* top_anchor;      // Anchor to all parsed nodes.
    PRMP_ANCHOR* current_anchor;  // Anchor parsed nodes at current level.
} PARSE_BLOCK;
 
 

static int parmParse(void** handle, PARSE_BLOCK* cbParms);
static int parmParseNode( PARSE_BLOCK* parms, PRMP_ANCHOR* anchor);
 
 

//----------------------------------------------------------------------
// Memory allocation routines...
//----------------------------------------------------------------------
//    parms = Gmem( sizeof(PARSE_BLOCK), "PRMP");  // Allocate a parse blok.
void* parmGmem( int size, char* id)    // Allocate a parse blok.
{
    // For now, do not include block id...
    return calloc(1, size);
}

void* parmGtms( void**gtms, int size, char* id) {
    // For now, do not include block id...
    return calloc(1, size);
}

void parmFmem( void* p) {
    free(p);
}



//----------------------------------------------------------------------
// Routine to read another line from file...
//----------------------------------------------------------------------
static int callbackIo(PARSE_BLOCK* parms)
{
    char *s;
    int   len;
 
    if( parms->next_buf == NULL ) {
        parms->next_buf = parmGmem(PRMP_STRING_WORK_SIZE, "WBUF");
    }

    do {            // Make sure we get a line (no blank lines after removing cr/lf)...

        if( (s = fgets( parms->next_buf, PRMP_STRING_WORK_SIZE, parms->fs )) == NULL ) {
            return -4;
        }

        if ( feof(parms->fs) ) {
            return -4;
        }

        len = strlen(parms->next_buf);

        // Actually, we do support larger lines since fgets() will divide lines.  We can
        // work with that.  For now, leaving this test in place...
        if( len >=  PRMP_STRING_WORK_SIZE) { 
            return -2;                   // Line too long error.
        }

        // Check for either windows or linux line terminators and remove...
        // Windows will have a line ending in 0x0d0a, linux lines end with just 0x0a.
        // There might be a chance that this line is larger than can fit in the buffer
        // which means there may not be any line terminators at the end, since the
        // line will continue with the next fgets() read.  There might be a case where
        // The line is just long enough that fgets() returns the CR but not the LF...

        if ( len > 0 && parms->next_buf[len - 1] == 0x0a ) {
            parms->next_buf[--len] = 0;
        }
        if ( len > 0 && parms->next_buf[len - 1] == 0x0d ) {
            parms->next_buf[--len] = 0;
        }

    } while ( len == 0 );               // No empty lines (after pealing off cr/lf).

    parms->linenbr++;
    parms->offset = 0;

    return len;
}
 
 
//----------------------------------------------------------------------
// Routine to allocate and initialize PARSE_BLOCK...
//----------------------------------------------------------------------
static PARSE_BLOCK* initParseBlock()
{
    PARSE_BLOCK* parms;
 
    parms = parmGmem( sizeof(PARSE_BLOCK), "PRMP");  // Allocate a parse blok.
 
    // Allocate memory for a workarea to be used for parsing strings...
    if(( parms->str_wrk = parmGmem(PRMP_STRING_WORK_SIZE, "PSTR")) == NULL) {
        return (PARSE_BLOCK*) -1;
    }
 
    // Allocate first gtms temp storage for top-level anchor block for our parsed nodes...
    if( (parms->top_anchor = parmGtms( &parms->gtms, sizeof(PRMP_ANCHOR), "PANC")) == NULL ) {
        return (PARSE_BLOCK*) -1;
    }
 
    parms->str_wrk_len = PRMP_STRING_WORK_SIZE;
 
    return parms;
}
 
 
//----------------------------------------------------------------------
// Routine to clean up and free PARSE_BLOCK...
//----------------------------------------------------------------------
static int freeParseBlock(PARSE_BLOCK* parms)
{
    if( parms->str_wrk )
        parmFmem(parms->str_wrk);
 
    parmFmem( (void*) parms);
 
    return 0;
}
 
 
 
//-----------------------------------------------------------------------
// Get next character from file. ignore comments if not in a quoted str...
//-----------------------------------------------------------------------
static unsigned char next_char( PARSE_BLOCK* parms, BOOL lookahead, BOOL in_quotes )
{
    unsigned char c;


    while( 1 ) {
        // do we need to get first or next line in file?
        if( !parms->have_line ) {
            int len;

            if( parms->len == 1 ) {  // If we had looked ahead to end of line?
                if( !lookahead )
                    parms->len = 0;
                return ' ';
            }
 
            if( parms->is_eof )
                return 255;             // Indicate end of file.
 
            if( (len = callbackIo( parms )) < 0) {
                // There needs to be a differenciation between error and eof?
                // For now, assume eof (-4)
                parms->is_eof = TRUE;
                return 255;             // return with error code.
            }
 
            if( len == 0 ) {            // Nothing more to read in. (EOF)?
                parms->is_eof = TRUE;
                return 255;             // Indicate end of file.
            }

            parms->have_line = TRUE;
 
            parms->buf = parms->next_buf;
            parms->len = len;
        }
 
        // Are we at end of current line or start of comment?
        // If so, then go to next line...
        if( parms->len == 0 || (*parms->buf == '#' && !in_quotes)) {

            parms->have_line = FALSE;
 
            if ( lookahead )
                parms->len = 1;
 
            // Return with just a space...
            return ' ';
        }
 
        c = *parms->buf;
 
        if (!lookahead) {
            parms->buf++;
            parms->len--;
        }
 
        return c;
    }
}
 
 
 
//-----------------------------------------------------------------------
// Get next string. We collect next string and put in our str_wrk area...
//-----------------------------------------------------------------------
static int next_string( PARSE_BLOCK* parms, BOOL isLookingForKey )
{
    char  quote_char;
    BOOL  in_quotes = FALSE;
    unsigned char c;

    parms->str_len = 0;    // Reset string length.
 
    // Gooble up any leading spaces before something starts...
    while( (c = next_char( parms, FALSE, FALSE )) != 255 &&
                           ( c == ' ' ||      // If leading space
                             c == 0 ) );      // Or end-of-line.

    while(1) {
 
        if( parms->is_eof == TRUE ) {  // End of file?
            if( in_quotes )            // Are we also in qoutes?
                return -2;             // Yes.  So, syntax error!
            break;                     // Otherwise, then we are done.
        }
 
        // Check for start/end of quoted string...
        if( in_quotes ) {                     // Are we already in quotes?
            if( c == quote_char ) {           // Yes, and is this ending quote?
                break;                        // Yes, so we have a string now.
            }
 
        // Check for starting a quote...
        } else if ( c == '"' || c == '\'' ) {
            if( parms->str_len > 0 )          // Already started to collect a string?
                return -2;                    // Yes, so, error!
            quote_char = c;                   // Otherwise, starting a quoted string.
            in_quotes = TRUE;
            if( (c = next_char( parms, FALSE, TRUE )) == 255) // Skip over quote. EOF?
                return c;                     // Yes. Return with EOF.
 
        // Check for normal end of string...
        } else if ( c == ' ' || c == ':' || c == '{' || c == '}' ) {
            break;                     // Break out of loop.
        }
 
        // Otherwise, collect another character of the string...
        if( parms->str_len >= (parms->str_wrk_len - 1) )  // check if room.
            return -1;                                    // Too big!
 
        parms->str_wrk[parms->str_len++] = c;
 
        c = next_char( parms, FALSE, in_quotes );
    }
 
    // NULL terminate string in string workarea...
    parms->str_wrk[parms->str_len] = 0;
 
    // So, now, return terminating character if it is :, {, }
    // Gooble up any leading spaces before something else starts. We will lookahead.
    while( c == ' ' && !parms->is_eof) {
        if( (c = next_char( parms, TRUE, FALSE )) == 255 )     // Peek at next char.
            return c;                                          // If error, return.
        if( c == ':' || c == '{' || c == '}' ) {
            if( (c = next_char( parms, FALSE, FALSE )) == 255) // Actually get it.
                return c;                                      // If error, return.
            break;
        }
        if( c != ' ' ) {                                       // Anything else,
            break;                                             // Leave for next time.
        }
        if( (c = next_char( parms, FALSE, FALSE )) == 255)     // Gooble spaces.
            return c;                                          // If error, return.
    }

    if( c == ':' || c == '{' || c == '}' || c == 255 ) {
        return c;
    }
 
    return ' ';
}
 
 
 
//----------------------------------------------------------------------
// Parse out key...
//----------------------------------------------------------------------
static int parse_key( PARSE_BLOCK* parms, PRMP_NODE* node)
{
    unsigned char term_char;
 
    if( (term_char = next_string( parms, TRUE )) < 0) {
        return term_char;    // Return with error code.
    }

    //End of file?
    if( term_char == 255 ) {
        return term_char;
    }
 
    // Keys should end with :.  Anything else should be an error!
    if( term_char != ':' )
        return -2;                  // If there was a key parsed out, then error!
 
    if( parms->str_len == 0)
        return term_char;           // Let higher level deal with it.
 
    if( (node->key = parmGtms( &parms->gtms, parms->str_len + 1, "PSTR")) == NULL ) {
        return -3;           // Out of memory!
    }
 
    memcpy( node->key, parms->str_wrk, parms->str_len);
 
    return 0;
}
 
 
 
//----------------------------------------------------------------------
// Parse out a value (which could recursively parse next level)...
//----------------------------------------------------------------------
static int parse_value( PARSE_BLOCK* parms, PRMP_NODE* node)
{
    unsigned char term_char;
    int  rc;
 
    if( (term_char = next_string( parms, FALSE )) < 0) {
        return term_char;    // Return with error code.
    }

    //End of file?
    if( term_char == 255 ) {
        return term_char;
    }
 
    if( term_char == '{' )  {
 
        if( parms->str_len > 0 )   // Did we parse out a string?
            return -2;             // Then syntax error!
        if( (node->nextlevel = parmGtms( &parms->gtms, sizeof(PRMP_ANCHOR), "PANC")) == NULL ) {
            return -3;           // Out of memory!
        }
        node->nextlevel->up = parms->current_anchor;
        node->type = PRMP_NEXTLEVEL;
 
        if( (rc = parmParseNode( parms, node->nextlevel )) < 0) {
            return rc;           // Some error during parsing!
        }
 
    } else {
 
        if( parms->str_len <= 0 )    // No string?  Then we don't have a value. Error!
            return -2;               // Syntax error.
 
        node->type = PRMP_STRING;
        if( (node->value = parmGtms( &parms->gtms, parms->str_len + 1, "PSTR")) == NULL ) {
            return -3;           // Out of memory!
        }
 
        memcpy( node->value, parms->str_wrk, parms->str_len);
 
        // Are we at end of this level?
        if( term_char == '}' )
            return +1;
    }
 
    return 0;
}
 
 
//----------------------------------------------------------------------
// Parse out a node...
//----------------------------------------------------------------------
static int parmParseNode( PARSE_BLOCK* parms, PRMP_ANCHOR* anchor)
{
    PRMP_NODE* node;
    int rc;
 
    parms->current_anchor = anchor;
 
    while( 1 ) {

        // now, get a node...
        if( (node = parmGtms( &parms->gtms, sizeof(PRMP_NODE), "PNOD")) == NULL ) {
            return -3;           // Out of memory!
        }
 
        if( (rc = parse_key(parms, node)) < 0 ) {
            return rc;
        }
 
        if ( parms->is_eof )      // End of input, then just return.
            return 0;
 
        if( (rc = parse_value(parms, node)) < 0 ) {
            return rc;
        }

        if ( parms->is_eof )      // End of input at this point is a syntax error.
            return -2;
 
        // Alright, now we have a complete node.  Add to chain off anchor...
        if( anchor->first == NULL ) {
            anchor->first = node;
            anchor->last  = node;
        } else {
            anchor->last->next = node;
            anchor->last       = node;
        }
 
        if( rc > 0 )          // If we received }, then break.  *REFACTOR*
            break;
    }
 
    parms->current_anchor = anchor->up;
 
}
 
 
 
//----------------------------------------------------------------------
// Top level parsing...
//----------------------------------------------------------------------
static int parmParse(void** handle, PARSE_BLOCK* parms)
{
    int   rc;
    PRMP_HANDLE* prmp_handle;
 
 
    if( (rc = parmParseNode( parms, parms->top_anchor )) < 0) {
        return rc;           // Some error during parsing!
    }
 
    if( (prmp_handle = parmGtms( &parms->gtms, sizeof(PRMP_HANDLE), "PHND")) == NULL ) {
        return -3;           // Out of memory!
    }
 
    prmp_handle->anchor = parms->top_anchor;
    prmp_handle->gtms   = parms->gtms;
    *handle             = (void*) prmp_handle;
 
    return 0;
}
 
 
//----------------------------------------------------------------------
// Parse out parameter file and return handle and results. If memname
// is NULL, then input dataset is not a PDS, but a sequencial file.
//----------------------------------------------------------------------
int parmParseFile(void** handle, char* filename)
{
    int   rc = 0;
    PARSE_BLOCK* parms;
 
 
    if( (parms = initParseBlock()) == NULL) {
        rc = -16;
    } else {

        parms->fs = fopen ( filename, "r");
 
        if( parms->fs == NULL) {
            fprintf(stderr, "Could not open configuration file %s\n", filename);
            return -4;
        } else {
 
            parms->linenbr = 0;

            rc = parmParse(handle, parms );
        }
 
        freeParseBlock( parms );
    }
 
    //if (rc < 0 && parms->gtms != NULL) {
    //    freetms( &parms->gtms );
    //}
 
    return rc;
}
 
 
//----------------------------------------------------------------------
// parmSetBegin() -- Prepare for traversing the parameters. Just
//                       reset pointers in handle.
//----------------------------------------------------------------------
int parmSetBegin(    void* handle)
{
    PRMP_HANDLE* prmp = (PRMP_HANDLE*) handle;
 
    prmp->cur_anchor       = NULL;
    prmp->cur_node         = NULL;
    prmp->node_stack_index = 0;
}
 
 
//----------------------------------------------------------------------
// parmGetNext() --
//----------------------------------------------------------------------
int parmGetNext(     void* handle, char** key, char** value)
{
    PRMP_HANDLE* prmp = (PRMP_HANDLE*) handle;
 
    if( prmp->cur_anchor == NULL ) {
        prmp->cur_anchor = prmp->anchor;
        prmp->cur_node   = NULL;
    }
 
    if( prmp->cur_node   == NULL ) {
        if( (prmp->cur_node = prmp->cur_anchor->first) == NULL)
            return PRMP_END;
    } else {
        if( (prmp->cur_node = prmp->cur_node->next) == NULL)
            return PRMP_END;
    }
 
    *key = prmp->cur_node->key;
    if( prmp->cur_node->type == PRMP_STRING ) {
        *value = prmp->cur_node->value;
        return PRMP_STRING;
    } else {
        return (int) prmp->cur_node->type;
    }
}
 
 
//----------------------------------------------------------------------
// parmLevelDown() -- Go down a level. We can only go down a level
//                        if the current node has a next level, other-
//                        wise, a -1 is returned. So that we can later
//                        get back to the node we were on, we keep a
//                        stack of pointers to nodes...
//----------------------------------------------------------------------
int parmLevelDown(   void* handle )
{
    PRMP_HANDLE* prmp = (PRMP_HANDLE*) handle;
 
    if( prmp->cur_node != NULL && prmp->cur_node->type == PRMP_NEXTLEVEL &&
        prmp->node_stack_index < PRMP_MAX_LEVELS ) {
        prmp->cur_anchor = prmp->cur_node->nextlevel;
        prmp->node_stack[prmp->node_stack_index++] = prmp->cur_node;
        prmp->cur_node   = NULL;
        return 0;
    }
 
    return -1;
}
 
 
//----------------------------------------------------------------------
// parmLevelUp() -- Go up a level...
//----------------------------------------------------------------------
int parmLevelUp(     void* handle )
{
    PRMP_HANDLE* prmp = (PRMP_HANDLE*) handle;
 
    if( prmp->cur_anchor->up != NULL && prmp->node_stack_index > 0 ) {
        prmp->cur_anchor = prmp->cur_anchor->up;
        prmp->cur_node   = prmp->node_stack[--prmp->node_stack_index];
        return 0;
    }
 
    return -1;
}
 
 
//----------------------------------------------------------------------
// parmFindKey() -- Find first key within a level...
//----------------------------------------------------------------------
int parmFindKey( void* handle, char* key, char** value)
{
    PRMP_HANDLE* prmp = (PRMP_HANDLE*) handle;
 
    // If we haven't done a search yet, then start at the top level...
    if( prmp->cur_anchor == NULL ) {
        prmp->cur_anchor = prmp->anchor;
        prmp->cur_node   = NULL;
    }
 
    // First node to check within this level...
    prmp->cur_node = prmp->cur_anchor->first;
 
    // Go through the nodes at this level and find first that matches...
    while( prmp->cur_node != NULL ) {
 
        if( strcmp(prmp->cur_node->key, key) == 0 ) {
            if( prmp->cur_node->type == PRMP_STRING ) {
                *value = prmp->cur_node->value;
                return PRMP_STRING;
            } else {
                return (int) prmp->cur_node->type;
            }
        }
        prmp->cur_node = prmp->cur_node->next;
    }
 
    return PRMP_END;
}
 
 
//----------------------------------------------------------------------
// parmFindNextKey() -- Find next key within a level...
//----------------------------------------------------------------------
int parmFindNextKey( void* handle, char* key, char** value)
{
    PRMP_HANDLE* prmp = (PRMP_HANDLE*) handle;
 
    // If we haven't done a search yet, then start at the top level...
    if( prmp->cur_anchor == NULL ) {
        prmp->cur_anchor = prmp->anchor;
        prmp->cur_node   = NULL;
    }
 
    // Next node to check. Its possible we are to start from the beginning...
    if( prmp->cur_node   == NULL ) {
        prmp->cur_node = prmp->cur_anchor->first;
    } else {
        prmp->cur_node = prmp->cur_node->next;
    }
 
    // Go through rest of nodes at this level that matches...
    while( prmp->cur_node != NULL ) {
        if( strcmp(prmp->cur_node->key, key) == 0 ) {
            if( prmp->cur_node->type == PRMP_STRING ) {
                *value = prmp->cur_node->value;
                return PRMP_STRING;
            } else {
                return (int) prmp->cur_node->type;
            }
        }
        prmp->cur_node = prmp->cur_node->next;
    }
 
    return PRMP_END;
}
