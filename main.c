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
    wint_t c;

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

void loadsettings()
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
            return;
        }
        // read the config file and allocate memory to store the settings
        while ((read = getline(&line, &len, fp)) != -1) {
            if (line == NULL)
            {
                printf("Error. The pointer line isn't initialized.\n");
                return;
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
    }
    else {
        createdefaultconfigfile();
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

// f - name of file to get info on, tmzip - return value: access, modific. and creation times, dt - dostime
uLong filetime(wchar_t *f, uLong *dt)
{
    int ret = 0;
    {
        FILETIME ftLocal;
        HANDLE hFind;
        WIN32_FIND_DATAW ff32;

        hFind = FindFirstFileW(f,&ff32);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            FileTimeToLocalFileTime(&(ff32.ftLastWriteTime),&ftLocal);
            FileTimeToDosDateTime(&ftLocal,((LPWORD)dt)+1,((LPWORD)dt)+0);
            FindClose(hFind);
            ret=1;
        }
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
    zipFile zf = zipOpen(pathtozip,APPEND_STATUS_CREATE);
    if (zf == NULL) {
        // if something went wrong
        printf("couldn't create empty zip");
        return zf;
    }
    return zf;
}

void closeZip(zipFile zf) {
    zipClose(zf,NULL);
}

// zip a file to an open zip archive
// buf is a memory buffer we allocate before so that we won't need to allocate it for each file
// buflen is the size of memory buffer we allocate
// returns the size of the file in case of success or -1 in there was any error
int addToOpenZip(zipFile zf, wchar_t *pathtofile, char *buf, size_t size_buf)
{
    int tlen = wcslen(pathtofile);
    char s1251[tlen];
    WideCharToMultiByte(1251, 0, pathtofile, -1, s1251, tlen, NULL, NULL);
    char *sch;
    while ((sch=strchr(s1251,'\?')))
    {
        *sch='_';
    }


    // get filename that zip will contain
    //const char *savefilenameinzip;
    zip_fileinfo zi;
    //unsigned long crcFile=0;
    zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour =
            zi.tmz_date.tm_mday = zi.tmz_date.tm_mon
            = zi.tmz_date.tm_year = 0;
    zi.dosDate = 0;
    zi.internal_fa = 0;
    zi.external_fa = 0;

    filetime(pathtofile,&zi.dosDate);
    // create empty file inside zip
    // the path shouldn't start with a leading slash
    while( pathtofile[0] == L'\\' || pathtofile[0] == L'/' )
    {
        pathtofile++;
    }

    int err = zipOpenNewFileInZip(zf,s1251,&zi,
                              NULL,0,NULL,0,NULL, Z_DEFLATED,
                              Z_NO_COMPRESSION);

    //#define Z_NO_COMPRESSION         0
    //#define Z_BEST_SPEED             1
    //#define Z_BEST_COMPRESSION       9
    //#define Z_DEFAULT_COMPRESSION  (-1)

    FILE *fin;

    int fs = 0;
    if (err != ZIP_OK) {
        wprintf(L"error while opening %s in zipfile\n",pathtofile);
        return -1;
    }
    else {
        //fin = fopen(pathtofile,"rb");
        fin = _wfopen(pathtofile,L"rb");
        if (fin == NULL)
        {
            err = ZIP_ERRNO;
            wprintf(L"couldn't open %s on the disk\n",pathtofile);
            return -1;
        }

        fs = fsize(fin);

    }
    unsigned long size_read=0;

    if (err == ZIP_OK)
    {
        // fill our empty file with the data
        do {
            err = ZIP_OK;
            size_read = (int)fread(buf,1,size_buf,fin);
            if (size_read < size_buf)
            {
                if (feof(fin) == 0)
                {
                    wprintf(L"error while reading %s\n",pathtofile);
                    err = ZIP_ERRNO;
                }
            }

            if (size_read>0)
            {
                err = zipWriteInFileInZip(zf,buf,size_read);
                if (err<0)
                {
                    wprintf(L"error while writing %s in the zip",pathtofile);
                }
            }
        } while ((err == ZIP_OK) && (size_read > 0));

        if (fin)
           fclose(fin);
        if (err < 0)
            err = ZIP_ERRNO;
        else {
            err = zipCloseFileInZip(zf);
            if (err != ZIP_OK)
            {
                wprintf(L"couldn't close %s in the zip",pathtofile);
                return -1;
            }
        }
    }
    return fs;
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

        size_t totalsize = 0;

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
                newsize = addToOpenZip(zf,line,buf,size_buf);
                size_t news = totalsize + newsize;
                if (news > totalsize)
                {
                    // overflow safe
                    totalsize = news;
                }
                else {
                    // overflow, can't write more than 4Gb to standart zip
                    // close the current zip
                    closeZip(zf);
                    // open new zip
                    zf = openZip(OUTPUTPATH "2");
                    totalsize = 0;
                }
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
    //_setmode(_fileno(stdout), _O_U16TEXT);
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
        loadsettings();
        // chech input argument as a path to a directory
        if (dir_exist(argv[1])) {
            memset(defconfig.startdir,0,BUFSIZ);
            strlcpy(defconfig.startdir,argv[1],sizeof(argv[1]) / sizeof(argv[1][0]));
        }

        tempfp = fopen(TMPTXTFILE,"w");
        // scan files
        printf("Scanning the path...\n");
        int startdirlen = strlen(defconfig.startdir);
        wchar_t wstart_path[startdirlen];
        MultiByteToWideChar(CP_UTF8,0,defconfig.startdir,-1,wstart_path,startdirlen);
        scanfiles(wstart_path);
        fclose(tempfp);
        createZipfromList();
        // we don't need to release memory because we didn't use it to initialize config
        // fp is already closed

        return 0;
    }
    // no arguments
    // try to parse the config file
    loadsettings();
    // use our default settings

    tempfp = fopen(TMPTXTFILE,"w");
    // scan files
    printf("Scanning the path...\n");
    int startdirlen = strlen(defconfig.startdir);
    wchar_t wstart_path[startdirlen];
    MultiByteToWideChar(CP_UTF8,0,defconfig.startdir,-1,wstart_path,startdirlen);
    wstart_path[startdirlen]='\0';
    scanfiles(wstart_path);
    fclose(tempfp);

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

    return 0;
}

