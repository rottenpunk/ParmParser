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
#ifndef PARMPRSR_H_
#define PARMPRSR_H_
 
//--------------------------------------------------------------------
// PARMPRSR parameter file parsing...
//--------------------------------------------------------------------
 
#define PRMP_END        0
#define PRMP_STRING     1
#define PRMP_NEXTLEVEL  2
 
int parmParseFile(void** handle, char* filename);
 
int parmSetBegin(    void* handle);
int parmGetNext(     void* handle, char** key, char** value);
int parmLevelDown(   void* handle );
int parmLevelUp(     void* handle );
 
int parmFindKey(     void* handle, char* key, char** value);
int parmFindNextKey( void* handle, char* key, char** value);
 
 
#endif // PARMPRSR_H_
 
