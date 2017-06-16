# documentsbackuper

This little console utility scans a directory and its subdirectories and puts all the files with specific extensions into a zip file. 
For now it's Windows-only because I used WinAPI functions (C has no any standart crossplatform library to walk a directory). As to creating zip file, I used some code from minizip project https://github.com/madler/zlib/tree/master/contrib/minizip


To see some general information, you can run it this way:
documentsbackuper.exe -h

You can also pass a directory to scan by parameter:
documentsbackuper.exe D:\testfolder

If you run it with no arguments, it would try to read a config file to load the settings. If there is no config file in the same folder, it will be created with default settings (with C:\ as default directory).

This version can read UTF-8 filenames but can't create such names inside zip due to some restrictions of minizip and zlib. This means that all non-ASCII characters in filenames will be replaced with other symbols.

