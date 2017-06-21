#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <windows.h>
#include <wchar.h>
#include <dirent.h>
#include <tchar.h>
#include <locale.h>
#include <sys/types.h>
#include "zip/zlib.h"
#include "zip/zip.h"
#include <fcntl.h>

#define SETTINGSFILE "config.dat"
#define TMPTXTFILE "templisttozip.tmp" // file that will contain a list of files to backup
#define OUTPUTPATH "test.zip.backup"

typedef struct
{
    char startdir[BUFSIZ];
    int n;
    int isinitialized; // if we load the settings, this flag should be set
    char *defextensions[26];
    char **extensions;
    // char **ignorelist; // nothingtoignore
} myconfig;

myconfig defconfig = { "C:\\", 26, 0, { "doc", "pdf", "djvu", "7z", "bat", "bmp", "bz2", "docx", "jpg", "jpeg",
                                  "odt", "png", "gif", "rar", "zip", "rtf", "sqlite3", "sqlitedb", "svg",
                                    "tar", "txt", "tiff", "cmd", "vbs", "xls", "xlsx",
                                  }, NULL};

FILE *fp; // settings file
FILE *tempfp; // temporary list of files ready to be in a zip

int cnt=0;

int file_exist(LPCTSTR fname)
{
    if (GetFileAttributesW(fname) != (DWORD)-1)
        return 1;
    return 0;
}

int DirectoryExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  if (dwAttrib != INVALID_FILE_ATTRIBUTES &&
         (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
      return 1;
  return 0;
}

size_t getline(char **lineptr, size_t *n, FILE *stream) {
    char *bufptr = NULL;
    char *p = bufptr;
    size_t size;
    int c;

    if (lineptr == NULL) {
        return -1;
    }
    if (stream == NULL) {
        return -1;
    }
    if (n == NULL) {
        return -1;
    }
    bufptr = *lineptr;
    size = *n;

    c = fgetc(stream);
    if (c == EOF) {
        return -1;
    }
    if (bufptr == NULL) {
        bufptr = (char *)malloc(128);
        if (bufptr == NULL) {
            return -1;
        }
        size = 128;
        memset(bufptr,'\0',sizeof(char)*size);
    }
    p = bufptr;
    while(c != EOF) {
        unsigned int diff = (p - bufptr);
        if (diff > (size - 1)) {
            size = size + 128;
            bufptr = (char *)realloc(bufptr, size);
            if (bufptr == NULL) {
                return -1;
            }
            memset(bufptr,'\0',sizeof(char)*size);
        }
        *p++ = c;
        if (c == '\n') {
            break;
        }
        c = fgetc(stream);
    }

    *p++ = '\0';
    *lineptr = bufptr;
    *n = size;

    return p - bufptr - 1;
}

size_t getwline(wchar_t **lineptr, size_t *n, FILE *stream) {
    wchar_t *bufptr = NULL;
    wchar_t *p = bufptr;
    size_t size;
    int c;

    if (lineptr == NULL) {
        return -1;
    }
    if (stream == NULL) {
        return -1;
    }
    if (n == NULL) {
        return -1;
    }
    bufptr = *lineptr;
    size = *n;

    c = fgetwc(stream);
    if (c == EOF) {
        return -1;
    }
    if (bufptr == NULL) {
        bufptr = (wchar_t *)malloc(128);
        if (bufptr == NULL) {
            return -1;
        }
        size = 128;
        wmemset(bufptr,L'\0',sizeof(wchar_t)*size);
    }
    p = bufptr;
    while(c != WEOF) {
        unsigned int diff = (p - bufptr);
        if (diff > (size - 1)) {
            size = size + 128;
            bufptr = (wchar_t *)realloc(bufptr, size);
            if (bufptr == NULL) {
                return -1;
            }
            wmemset(bufptr,L'\0',sizeof(wchar_t)*size);
        }
        *p++ = c;
        if (c == L'\n') {
            break;
        }
        c = fgetwc(stream);
    }

    *p++ = L'\0';
    *lineptr = bufptr;
    *n = size;

    return p - bufptr - 1;
}

size_t strlcpy(char *dst, const char *src, size_t siz)
{
    char *d = dst;
    const char *s = src;
    size_t n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0')
                break;
        }
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (siz != 0)
            *d = '\0';		/* NUL-terminate dst */
        while (*s++)
            ;
    }

    return(s - src - 1);	/* count does not include NUL */
}

int dir_exist(char *path)
{
    if( _access( path, 0 ) == 0 ){
        struct stat status;
        stat( path, &status );

        return (status.st_mode & S_IFDIR) != 0;
    }
    return 0;
}

void writestructtofile(FILE *lfp)
{
    if (lfp == NULL)
        return;
   fprintf(lfp,"startdir=%s\n",defconfig.startdir);
   for (int i=0; i<defconfig.n; i++)
   {
       fprintf(lfp,"extension=%s\n",defconfig.defextensions[i]);
   }
}

int createdefaultconfigfile()
{
    if (file_exist(TEXT(SETTINGSFILE)))
    {
        fp = fopen(SETTINGSFILE,"r+");
        if (!fp)
        {
            printf("Error while opening a file " SETTINGSFILE " for reading and writing\n");
            return -1;
        }
        writestructtofile(fp);
        fclose(fp);
    }
    else {
        // create a file
        printf("creating a file\n");
        fp = fopen(SETTINGSFILE,"w");
        if (!fp)
        {
            printf("Error while creating a file " SETTINGSFILE "\n");
            return -1;
        }
        // write default structure to the file
        writestructtofile(fp);
        fclose(fp);
    }
    return 0;
}

int countlines(char *tfilepath)
{
    int lines = 0;
    FILE *lfp = fopen(tfilepath,"r");
    if (lfp != NULL)
    {
        int c;
        do {
            c = fgetc(lfp);
            if (c=='\n')
                lines++;
        } while (c != EOF);
    }
    fclose(lfp);
    return lines;
}

// load configuration settings from a file
// return values: -1 error, 0 - success, 1 - no config file, generate one
int loadsettings()
{
    if (file_exist(TEXT(SETTINGSFILE)))
    {
        fp = fopen(SETTINGSFILE,"r");
        defconfig.n = 0;
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        int nlines = countlines(SETTINGSFILE);
        //printf("%d\n",nlines);
        defconfig.extensions = (char **)malloc(nlines * sizeof(char*));

        if (defconfig.extensions == NULL) {
            printf("Error during allocation of memory for an array of strings which contains the settings");
            return -1;
        }
        // read the config file and allocate memory to store the settings
        while ((read = getline(&line, &len, fp)) != -1) {
            if (line == NULL)
            {
                printf("Error. The pointer line isn't initialized.\n");
                return -1;
            }
            line[read] = '\0';
            //printf("%s\n",line);
            if (strstr(line,"extension=")) {
                char *startpos = strchr(line,'=') + 1;
                int lenex = strlen(startpos);
                defconfig.extensions[defconfig.n] = (char*)malloc(lenex);
                memset(defconfig.extensions[defconfig.n],0,lenex);
                strlcpy(defconfig.extensions[defconfig.n],startpos,lenex);
                defconfig.n = defconfig.n + 1;
            }
            if (strstr(line,"startdir=")) {
                char *startpos = strchr(line,'=') + 1;
                int lenex = strlen(startpos);
                memset(defconfig.startdir,0,BUFSIZ);
                strlcpy(defconfig.startdir,startpos,lenex); // strlcpy
            }

            memset(line,0,read);
        }

        defconfig.isinitialized = 1;

        //print the settings we imported
        //printf("%s\n",defconfig.startdir);
        //for (int i = 0; i<defconfig.n; i++) {
        //    printf("%s\n",defconfig.extensions[i]);
        //}

        fclose(fp);
        return 0;
    }
    else {
        createdefaultconfigfile();
        return 1;
    }
}

LPWSTR get_filename_ext(wchar_t *filename)
{
    wchar_t *dot = wcsrchr(filename,'.');
    if(!dot || dot == filename) return L"";
    wchar_t *crlf = wcsrchr(dot,'\n');
    if(crlf) {
        //trim new line character
        *crlf = L'\0';
    }
    return dot + 1;
}

// write a line to a text file
void addTempFileLine(FILE *tmpfp, int leng, wchar_t *line)
{
    //_setmode(_fileno(tmpfp), _O_U8TEXT);
    //wprintf(L"%ls\n",line);
    //fwprintf(tmpfp,L"%s\n",line);
    leng=lstrlenW(line);
    size_t bsize=WideCharToMultiByte(CP_UTF8,0,line,leng,NULL,0,NULL,NULL);
    wchar_t outputbuf[bsize];
    wmemset(outputbuf,L'\0',bsize);
    WideCharToMultiByte(CP_UTF8,0,line,leng,outputbuf,bsize,NULL,NULL);
    fprintf(tmpfp,outputbuf);
    fprintf(tmpfp,L"\n");
}

void scanfiles(wchar_t *wstart_path)
{
    //wprintf(L"Start directory is %s\n",wstart_path);

    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;
    // file mask *.* = everything!
    size_t mlen = wcslen(wstart_path) + 5;
    wchar_t fsbuf[mlen];
    //wmemset(fsbuf,'\0',mlen);
    swprintf_s(fsbuf,mlen,L"%ls\\*.*",wstart_path);
    if((hFind = FindFirstFileW(fsbuf, &fdFile)) == INVALID_HANDLE_VALUE)
    {
        wprintf(L"Path not found: [%ls]\n", wstart_path);
        return;
    }

    size_t sbufsize = 256;
    wchar_t *s = (wchar_t*)malloc(sbufsize*sizeof(wchar_t));

    do
    {
        //Find first file will always return "."
        //    and ".." as the first two directories.

        if(wcscmp(fdFile.cFileName, L".") != 0
                && wcscmp(fdFile.cFileName, L"..") != 0)
        {

            mlen = wcslen(wstart_path) + wcslen(fdFile.cFileName) + 1;
            if (mlen > sbufsize) {
                s = (wchar_t*)calloc(mlen,sizeof(wchar_t));
                wmemset(s,L'\0',mlen);
            }
            else {
                wmemset(s,L'\0',sbufsize);
            }
            //Build up our file path using the passed in
            //  wstart_path and the file/foldername we just found:
            wsprintf(s, L"%ls\\%ls\n", wstart_path, fdFile.cFileName);

            //Is the entity a File or Folder?
            if(fdFile.dwFileAttributes &FILE_ATTRIBUTE_DIRECTORY)
            {
                //printf("Directory:\n");
                //trim the new line character
                wchar_t *crlf = wcsrchr(s,'\n');
                if (crlf) {
                    *crlf = L'\0';
                }
                scanfiles(s); // recursion
            }
            else {

                wchar_t *extension = get_filename_ext(s);
                if (wcslen(extension) > 0) {
                    // if this file has the necessary extension
                    for (int i=0; i<defconfig.n; i++)
                    {
                        if (defconfig.isinitialized)
                        {

                            size_t extlen = strlen(defconfig.extensions[i]);
                            wchar_t samplext[extlen];
                            wmemset(samplext,L'\0',wcslen(samplext));
                            MultiByteToWideChar(CP_UTF8,0,defconfig.extensions[i],-1,samplext,extlen);
                            //wprintf(L"%ls %ls\n",extension,samplext);
                            if (wcscmp(samplext,extension) == 0) {
                                // we can add this file to our zip archive
                                s[mlen]=L'\0';
                                addTempFileLine(tempfp,mlen,s);
                            }

                        }
                        else {
                            // we use default settings here (26 extension patterns)
                            size_t extlen = strlen(defconfig.defextensions[i]);
                            wchar_t samplext[extlen];
                            MultiByteToWideChar(CP_UTF8,0,defconfig.defextensions[i],-1,samplext,extlen);

                            if (wcscmp(samplext,extension) == 0)
                            {
                                // add to zip
                                s[mlen]=L'\0';
                                addTempFileLine(tempfp,mlen,s);
                            }
                        }
                    }
                }
            }
        }
    } while(FindNextFileW(hFind, &fdFile)); //Find the next file.


    free(s);

    FindClose(hFind); //Always, Always, clean things up!
    return;
}

uint32_t get_file_date(const wchar_t *path, uint32_t *dos_date)
{
     int ret = 0;
     FILETIME ftm_local;
     HANDLE find = NULL;
     WIN32_FIND_DATAA ff32;
     find = FindFirstFileW(path, &ff32);
     if (find != INVALID_HANDLE_VALUE)
     {
         FileTimeToLocalFileTime(&(ff32.ftLastWriteTime), &ftm_local);
         FileTimeToDosDateTime(&ftm_local, ((LPWORD)dos_date) + 1, ((LPWORD)dos_date) + 0);
         FindClose(find);
         ret = 1;
     }
     return ret;
}

void setwtitle(int percent)
{
    if ((percent <= 100) && (percent >= 0)) {
        char txt[256];
        sprintf(txt,"%d%% complete",percent);
        SetConsoleTitleA(txt);
    }
}


// calculate size of open file without closing it
size_t fsize(FILE *fp){
    if (fp == NULL)
        return -1;
    int prev=ftell(fp);
    fseek(fp, 0L, SEEK_END);
    int sz=ftell(fp);
    fseek(fp,prev,SEEK_SET); // restore the file pointer
    return sz;
}

// calculate size of closed file and close it
size_t fclsize(char *filepath) {
    FILE *fpt = fopen(filepath,"rb");
    if (fpt == NULL)
        return -1;
    fseek(fpt,0L,SEEK_END);
    int sz=ftell(fpt);
    fclose(fpt);
    return sz;
}


int fwclsize(wchar_t *filepath) {
    HANDLE hFile;
    hFile = CreateFileW(filepath,
                 GENERIC_READ, FILE_SHARE_READ,
                 NULL, OPEN_EXISTING,
                 FILE_ATTRIBUTE_NORMAL, NULL);
    LONGLONG nFileLen = 0;
    if (hFile != INVALID_HANDLE_VALUE)
    {
       DWORD dwSizeHigh=0, dwSizeLow=0;
       dwSizeLow = GetFileSize(hFile, &dwSizeHigh);
       nFileLen = (dwSizeHigh * (MAXDWORD+1)) + dwSizeLow;
       CloseHandle(hFile);
    }
    size_t sz=nFileLen;
    return sz;
}

// create an empty zip file
// return value: 0 - success, 1 - failure
// pathtozip = "C:\\Android\\test.zip"
zipFile openZip(char *pathtozip) {
    // create empty zip file
    zipFile zf = zipOpen64(pathtozip,APPEND_STATUS_CREATE);
    if (zf == NULL) {
        // if something went wrong
        printf("couldn't create empty zip");
        return zf;
    }
    return zf;
}

void closeZip(zipFile zf) {
    zipClose_64(zf,NULL);
}

int is_large_file(const wchar_t *path)
{
    uint64_t pos = fwclsize(path);
    return (pos >= UINT32_MAX);
}

int get_file_crc(const wchar_t *path, void *buf, uint32_t size_buf, uint32_t *result_crc)
{
    FILE *handle = NULL;
    uint32_t calculate_crc = 0;
    uint32_t size_read = 0;
    int err = 0;

    handle = fopen64(path, "rb, ccs=UTF-8");
    if (handle == NULL)
    {
        err = -1;
    }
    else
    {
        do
        {
            size_read = (int)fread(buf, 1, size_buf, handle);

            if ((size_read < size_buf) && (feof(handle) == 0))
            {
                printf("error in reading %s\n", path);
                err = -1;
            }

            if (size_read > 0)
                calculate_crc = (uint32_t)crc32(calculate_crc, buf, size_read);
        }
        while ((err == Z_OK) && (size_read > 0));
        fclose(handle);
    }

    printf("file %s crc %x\n", path, calculate_crc);
    *result_crc = calculate_crc;
    return err;
}

// zip a file to an open zip archive
// buf is a memory buffer we allocate before so that we won't need to allocate it for each file
// buflen is the size of memory buffer we allocate
int addToOpenZip(zipFile zf, wchar_t *pathtofile, char *buf, size_t size_buf)
{
    int tlen = wcslen(pathtofile); // utf16 length
    size_t bsize=WideCharToMultiByte(CP_UTF8, 0, pathtofile+3, tlen-3, NULL, 0, NULL, NULL); // utf8 length
    wchar_t utf8text[bsize+1];
    wmemset(utf8text,L'\0',bsize);
    WideCharToMultiByte(CP_UTF8,0,pathtofile+3,tlen-3,utf8text,bsize,NULL,NULL);
    // the path shouldn't start with a leading slash

    // get filename that zip will contain
    zip_fileinfo zi = { 0 };
    uint32_t crc_for_crypting = 0;
    int size_read = 0;
    int zip64 = 0;
    int err = ZIP_OK;
    FILE *fin;

    /* Get information about the file on disk so we can store it in zip */
    get_file_date(pathtofile, &zi.dos_date);

    char *password=NULL;

    if ((password != NULL) && (err == ZIP_OK))
        err = get_file_crc(utf8text, buf, sizeof(buf), &crc_for_crypting);

    zip64 = is_large_file(pathtofile);

    int level=0;

    err = zipOpenNewFileInZip4_64(zf, utf8text, &zi, NULL, 0, NULL, 0, NULL,
                                  (level != 0) ? Z_DEFLATED : 0, level, 0,
                                  -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                  password, crc_for_crypting, 36, 1<<11, zip64);



    //#define Z_NO_COMPRESSION         0
    //#define Z_BEST_SPEED             1
    //#define Z_BEST_COMPRESSION       9
    //#define Z_DEFAULT_COMPRESSION  (-1)

    if (err != ZIP_OK) {
        wprintf(L"error in opening %s in zipfile (%d)\n", pathtofile, err);
        return -1;
    }
    else {
        fin = _wfopen(pathtofile, L"rb");
        if (fin == NULL) {
            err = ZIP_ERRNO;
            wprintf(L"error in opening %s for reading\n", pathtofile);
        }
    }


    if (err == ZIP_OK)
    {
        /* Read contents of file and write it to zip */
        do  {
            size_read = (int)fread(buf, 1, sizeof(buf), fin);
            if ((size_read < (int)sizeof(buf)) && (feof(fin) == 0))
            {
                wprintf(L"error in reading %s for reading\n", pathtofile);
                err = ZIP_ERRNO;
            }
            if (size_read > 0)  {
                err = zipWriteInFileInZip(zf, buf, size_read);
                if (err < 0)
                    wprintf(L"error in writing %s in the zipfile (%d)\n", pathtofile, err);
            }
        } while ((err == ZIP_OK) && (size_read > 0));
    }


    if (fin)
        fclose(fin);
    if (err < 0)
    {
        err = ZIP_ERRNO;
    }
    else {
        err = zipCloseFileInZip(zf);
        if (err != ZIP_OK)
            wprintf(L"error in closing %s in the zipfile (%d)\n", pathtofile, err);
    }

    return err;
}

int ansifile_exists(const char *path)
{
    FILE *ffp;

    if ( !fopen_s(&ffp, path, "r") )
    {
        fseek(ffp, 0, SEEK_END);
        fclose(ffp);
        return 1;
    }
    return 0;
}

// here we should have a txt file with the list of files
void createZipfromList()
{
    // if we can't find the list of files to backup
    if (!file_exist(TEXT(TMPTXTFILE)))
        return;
    int n = countlines(TMPTXTFILE);
    if (n == 0)
        return;
    int cnt=0;
    setwtitle((cnt*100)/n);
    // main workhorse loop
    FILE *lfp = _wfopen(TEXT(TMPTXTFILE),L"r,ccs=UTF-8");
    if (lfp != NULL) {
        // here we are sure to create zip
        zipFile zf = openZip(OUTPUTPATH);

        ssize_t read;
        wchar_t *line = (wchar_t*)malloc(128);
        size_t len = 128;
        size_t size_buf = 10*1024*1024; // let's allocate 10 Mb
        char *buf = (char*)malloc(size_buf);

        if (buf == NULL)
        {
            printf("Error while allocating memory for a buf variable\n");
            fclose(lfp);
            free(line);
            return;
        }

        while ((read = getwline(&line, &len, lfp)) != -1) {
            if (line == NULL)
            {
                printf("Error. The pointer line isn't initialized.\n");
                return;
            }
            if (read == 0)
            {
                break;
            }
            line[read] = L'\0';
            wchar_t *crlf = wcsrchr(line,L'\n');
            if(crlf) {
                //trim new line character
                *crlf = L'\0';
            }
            // line - filepath
            // we have to calculate the size of the file before we add it
            int bookcheck = file_exist(line);

            if (bookcheck == 1)
            {
                size_t newsize = fwclsize(line);
                if (newsize > size_buf)
                {
                    // we need more memory to process the file
                    // it's supposed that any file should be mapped no matter how big is. But we could set a limit here because if the size of a buffer isn't enough, it will be used several times in several iterations
                    char *resbuf = (char*)realloc(buf,newsize);
                    if (resbuf == NULL)
                    {
                        free(buf);
                        printf("Error. Couldn't realloc a buffer in createZipfromList()\n");
                        return;
                    }
                    buf=resbuf;
                    size_buf = newsize;
                }
                // we have enough memory in the buf

                // check the current zip size
                int tryf = addToOpenZip(zf,line,buf,size_buf);
            }
            else {
                wprintf(L"Warning! File %s doesn't exist\n",line);
            }
            cnt++;
            setwtitle((cnt*100)/n);

        }
        closeZip(zf);
        free(buf);
    }
    else {
        wprintf(L"Error while open %s\n",TEXT(TMPTXTFILE));
        return;
    }
    fclose(lfp);
}

void w_printf(char *txt)
{
    size_t mlen = strlen(txt) + 1;
    wchar_t s[mlen];
    MultiByteToWideChar(CP_UTF8,0,txt,-1,s,mlen);
    wprintf(s);
}

int main(int argc, char *argv[])
{
    setlocale(LC_CTYPE,"RUS");
    // if we received any parameters
    if (argc > 1) {
        // print help
        if (strstr(argv[1],"-h"))
        {
            char *fname = strrchr(argv[0],'\\') + 1;
            printf("%s v1.0\n",fname);
            printf("Author: Nekludov Konstantin odexed@mail.ru\n");
            printf("This program will scan your hard drive and create a zip archive with no extension that contains your documents according to their extension. You can set some settings using the " SETTINGSFILE " file (it should be created automatically if not exists)\n");
            printf("Basic syntax is %s [path_to_the_directory]\n",fname);

            return 0;
        }
        // try to parse the config file
        if (loadsettings() == -1)
            exit(-1);
        // check input argument as a path to a directory
        if (dir_exist(argv[1])) {
            memset(defconfig.startdir,0,BUFSIZ);
            strlcpy(defconfig.startdir,argv[1],sizeof(argv[1]) / sizeof(argv[1][0]));
        }

        tempfp = fopen(TMPTXTFILE,"w");
        // scan files
        printf("Scanning the path %s...\n",defconfig.startdir);
        int startdirlen = strlen(defconfig.startdir);
        wchar_t wstart_path[startdirlen];
        MultiByteToWideChar(CP_UTF8,0,defconfig.startdir,-1,wstart_path,startdirlen);
        scanfiles(wstart_path);
        fclose(tempfp);
        printf("Done. Creating a zip archive...\n");
        createZipfromList();
        printf("Finished. Press any key to exit");
        // we don't need to release memory here because we didn't use it to initialize config
        // fp is already closed
        return 0;
    }
    // no arguments
    // try to parse the config file
    if (loadsettings() == -1)
        exit(-1);
    // use our default settings

    tempfp = fopen(TMPTXTFILE,"w");
    // scan files
    printf("Scanning the path %s...\n",defconfig.startdir);
    int startdirlen = strlen(defconfig.startdir);
    wchar_t wstart_path[startdirlen];
    MultiByteToWideChar(CP_UTF8,0,defconfig.startdir,-1,wstart_path,startdirlen);
    wstart_path[startdirlen]='\0';
    scanfiles(wstart_path);
    fclose(tempfp);
    printf("Done. Creating a zip archive...\n");
    createZipfromList();

    // release memory and end the program
    if (defconfig.isinitialized)
    {
        // release the memory we used for the settings
        for (int i = 0; i<defconfig.n; i++)
        {
            free(defconfig.extensions[i]);
        }
        free(defconfig.extensions);
    }
    printf("Finished. Press any key to exit");
    return 0;
}

