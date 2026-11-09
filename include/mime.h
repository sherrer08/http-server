#ifndef MIME_H
#define MIME_H

/*
 * Returns the Content-Type for a file path, based on the extension after
 * the last '.' in the file name, compared case-insensitively: for example,
 * "text/html: charset=utf-8" for "index.html".
 * Returns "application/octet-stream" if there is no extension or it is not
 * recognized. The returned string is a constant, valid for the life of the
 * program.
*/
const char *mime_type_for_path(const char *path);

#endif