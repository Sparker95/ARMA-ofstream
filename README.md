# ARMA-ofstream

Intercept-based addon for ARMA to output text to external files

## SQF commands

### ofstream_new
Opens a file for output inside (Arma.exe root)\Logs folder, returns file handle to the opened file. Will return the same handle if called on the same file name multiple times.

Syntax: `ofstream_new fileName`  
Parameters: fileName - file name to open for output  
Return value string, file handle of the opened file

Example:
`g_myRpt = ofstream_new "myRpt.rpt";`

### ofstream_write
Writes text to a previously opened file.

Syntax: `fileHandle ofstream_write text`  
Parameters: fileHandle - string, file handle returned by ofstream_new  
text - string to output to file
Return value string, file handle of the file

Examples:
```
g_myRpt = ofstream_new "myRpt.rpt";
g_myRpt ofstream_write "Some text";

// Or can use it this way, with slightly worse performance
(ofstream_new "myRpt.rpt") ofstream_write "Some text";

// We can use it with format too
g_myRpt ofstream_write format ["Player's object: %1", player];
```

## References:
https://github.com/intercept/intercept
https://forums.bohemia.net/forums/topic/189144-intercept-a-library-for-writing-arma-addons-in-native-c/
https://github.com/intercept/intercept-plugin-template
