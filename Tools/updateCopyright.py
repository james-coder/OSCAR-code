#!/usr/bin/env python3
"""
Copyright (c) 2023-2024 The OSCAR Team

Find text files with the word "Copyright" ending with a year (4 digits) range like 2019-2022 and then
followed by a string containing OSCAR. The script changes the 2 year to the current year.
This script will search all files in the folder where the script is run:
1) From the Git top-level folder. The script will also access the translation files and Build file.
2) From the oscar folder (that contains oscar.pro). The script will also access the code files.
"""

import os
import re
import subprocess
import time
import filecmp
import inspect
import sys

current_year = time.localtime().tm_year
top_dir = subprocess.check_output(['git', 'rev-parse', '--show-toplevel']).decode('utf-8').strip()
relative_sync_dir = os.path.join(top_dir, '..')
verbose = False
changed = False
list = False
list_ignored = False
testExecution = False
debug = False
single = True

def validStr(str) :
    if str is None: return False
    tmp = not str
    return not tmp
    
def validYear(year) :
    if year is None: return False
    try:
        tmp = int(year)
    except ValueError:
        return False
    tmp = int(year)
    return  ((tmp > 2000) and (tmp < 2099) )

class Stats:
    def __init__(self, field1 , field2 , field3 , field4 , field5 ):
        self.files_date_changed              = field1
        self.files_date_already_changed      = field2
        self.files_needing_inspection        = field3
        self.files_ignored                   = field4
        self.files_total                     = field5

statistics = Stats(0,0,0,0,0)

class LineInfo:
    def __init__(self, field1 , field2 , field3 , field4 , field5 , field6 , field7 , field8):
        self.lineNumber     = field1        ## read only
        self.line           = field2        
        self.baseName       = field3        ## read only
        self.fileModified   = field4        ## numberlines Modified
        self.lineModified   = field5        ## True if line changed
        self.validSignature = field6        ## only set to true
        self.needReview     = field7        ## only set to true
        self.countUpdated   = field8        ## only set to true

def processLine(lineInfo):
    lineInfo.lineModified = False;
    baseNameLine = f"{lineInfo.baseName}[{lineInfo.lineNumber}]"
    baseNameLineJ = baseNameLine.ljust(30)
    
    ## read only properties
    global relative_sync_dir, current_year, verbose , list , debug , statstics

    #{ limit processing line 
    ## where * is any string 
    ## SOURCE:               * Copyright (c) YEAR - YEAR The OSCAR Team *
    ## Modifiable:           * Copyright * YEAR - YEAR The OSCAR Team *
    ## Modifiable:           * YEAR - YEAR The OSCAR Team *
    ## Modifiable:           * YEAR - YEAR OSCAR Team *
    ## Modifiable/WARNING:   * YEAR - YEAR OSCAR *
    ## WARNING:              * YEAR  OSCAR *
    ## WARNING:              * copyright * OSCAR *

    ## find copyright or year
    next = 0;
    pattern = r'copyright'
    copyright = False
    
    match = re.search(pattern,lineInfo.line,re.IGNORECASE)
    if (match) :
        copyright = True
        ###next = match.end()

    pattern = r'(^.*?)((\d{4})\s*(-)?\s*)(\d{4})?(\s*(The)?\s*(OSCAR)\s*(Team)?\b)(.*$)'   #works
    match = re.search(pattern,lineInfo.line,re.IGNORECASE)
    if match :  ## match 
        ## match of oscar copyright signature
        #{
        before        = match.group(1)
        year1dash     = match.group(2)
        year1         = match.group(3)
        dash          = match.group(4)
        year2         = match.group(5)
        theoscarteam  = match.group(6)
        the           = match.group(7)
        oscar         = match.group(8)
        team          = match.group(9)
        after         = match.group(10)

        if debug :
            sum = f"""
            before          1  {before} 
            year1dash       2  {year1dash} 
            year1           3  {year1} 
            dash            4  {dash} 
            year2           5  {year2} 
            TheOscarTeam    6  {theoscarteam} 
            The             7  {the} 
            OSCAR           8  {oscar} 
            Team            9  {team} 
            after           10 {after} 
            """
            print(sum);
        newCopyright = f"{year1dash}{current_year}{theoscarteam}"
        newLine = f"{before}{newCopyright}{after}\n"

        # Note  OSCAR is always valid and is assumed to be true
        validOscarCopyright   =  validYear(year1) and validStr(dash) and validYear(year2) 

        if (validOscarCopyright) :
            if not lineInfo.needReview : lineInfo.needReview = not ( copyright or validStr(the) or validStr(team) )
            fileValidOscarCopyright = True
            if (str(year2)==str(current_year)) :
                if not lineInfo.countUpdated :
                    statistics.files_date_already_changed += 1
                    lineInfo.countUpdated = True
                if verbose : print(f"  Already Modified:  {baseNameLineJ}")
                return;
            else :  ## need to modify date
                if not lineInfo.countUpdated :
                    statistics.files_date_changed += 1
                    lineInfo.countUpdated = True
                if verbose : print(f"  File Modified:     {baseNameLineJ}{newLine}",end='')
                lineInfo.lineModified = True;
                lineInfo.fileModified += 1;
                if debug :
                    print(lineInfo.line)
                    print(newLine)
                lineInfo.line = newLine
                return;
        #} end match of oscar copyright signature
    #} End limit processing line 

def processFile(filename):
    """
    Process the file to update the copyright year if necessary.
    Args:
    filename: name of the file to be processed
    """
    global relative_sync_dir, current_year, verbose , list , debug , statistics

    statistics.files_total =  statistics.files_total +1
    relativeFileName = os.path.relpath(filename, relative_sync_dir)
    baseName = os.path.basename(filename)
    lines = []
    lineNumber = 0;
    fileValidOscarCopyright = False
    needReview = False

    ##if debug : print(f" processFile {baseName}")

    lineInfo = LineInfo(0 , "" , baseName , 0 , False , False ,False , False);
    codeFile = baseName.endswith(".cpp") or baseName.endswith(".h") 
    try:
    #{ try loop
        ##  skip files not be changed.
        ##  only do .h and .cpp files and not third party software or auto generated files.
        if ( 
            ## Folder that should be excluded
            ("thirdparty" in filename.split(os.path.sep)) or ("tests" in filename.split(os.path.sep)) or ("git_info.h" == baseName )
            ## file types that should be excluded
            or (not codeFile)
            ## for test or (not ("aboutdialog.h" == baseName or  "newprofile.cpp" == baseName) )
            )    :
            statistics.files_ignored += 1
            lineInfo.countUpdated = True     ## insure a file is counted only once.
            if verbose or list_ignored: print(f"  Ignored:           {relativeFileName}")
            return;
        ## Common encodings to try: utf-8 ,  latin-1 , iso-8859-1 , cp1252 , utf-16 , big5 , gb18030 .
        if debug : print(f" processFile {baseName}")
        with open(filename, 'r+' , encoding="latin-1") as file_handle:   ## latin-1 works  utf-8 fails
            #{ start of processing all lines
            ## only search until 1st signature is found. except newprofile where copyright is displayed
            single = not ( "newprofile.cpp" == baseName )  
            if (list)  :
                print(f" Opened {relativeFileName}")
            elif (not single or lineInfo.fileModified == 0) :
                for line in file_handle:
                    #{ start of processing line
                        lineNumber += 1
                        lineInfo.lineModified  = False
                        lineInfo.lineNumber = lineNumber;
                        lineInfo.line = line;
                        processLine(lineInfo);
                        if lineInfo.lineModified : 
                            lines.append(lineInfo.line)
                        else :
                            lines.append(line)
                    #} end processing line
                ##if ( lineInfo.needReview or (codeFile and not lineInfo.countUpdated) ) :
                ## ALways print code files without copyRight info.
                if ( lineInfo.needReview or (codeFile and not lineInfo.countUpdated) ) :
                    print(f"  Copyright Check    {relativeFileName}")
                    if not lineInfo.countUpdated : 
                        statistics.files_needing_inspection += 1
                        lineInfo.countUpdated = True
                if lineInfo.fileModified == 0:
                    if not lineInfo.countUpdated : ## already modified is excluded here.
                        statistics.files_ignored += 1
                        lineInfo.countUpdated = True
                        if verbose or list_ignored: print(f"  Ignored:           {relativeFileName}")
                    return;
                if testExecution : return
                file_handle.seek(0)
                file_handle.truncate()
                file_handle.write(''.join(lines))
                file_handle.flush()
                os.fsync(file_handle.fileno())
                return
            #} end of processing all lines
    except IOError as e:
        print(f"  File Open Error:   {relativeFileName}")
    #} try loop

def isBinaryFile(file_path):
    with open(file_path, 'rb') as f:
        for block in f:
            if b'\0' in block:
                return True
    return False

"""
def xisBinaryFile(file_path):
    result = subprocess.run(['file', '--mime-encoding', file_path], capture_output=True, text=True)
    return 'binary' in result.stdout
"""

def handleFile(file_path):
    """
    Process the file if it is a text file.
    Args:
    file_path (str): The path of the file to be processed.
    """
    if os.path.isfile(file_path) :
        if not isBinaryFile(file_path):
            processFile(file_path)

def help_menu():
    """
    Display the help menu.
    """
    help_msg = """
    Help Menu
    {}

    # uses the current year to modifed files with copyright.
    # This script modifies the first line with the following signature (case insensative).
        YYYY and ZZZZ are sequences of 4 digits representing year
        asterisk " means any sequence of charaters.
    # signature: * YYYY-ZZZZ * OSCAR *
    # The script will only change ZZZZ to the current year. No file size change.
    # No other lines will be modfied.

    --help          displays help message
    --execute       allows script to execute
    -v --verbose    displays status for each file accessed
    --changed       displays each line modified
    --list          displays filenames to be search and exits
    --ignored       List files that are ignored.
    --test          Execute code but skips the actual file write
    --year <year>   Overrides system year 
    --code          starts working at OSCAR-code/oscar
    --base          starts working at OSCAR-code
    <folderName>    starts working at OSCAR-code/folderName
    defaultFolder   starts working at the current folder
    """.format(__file__)
    print(help_msg)
    exit()

####################################################################################################

start_dir = os.getcwd().rstrip('\n')
options = ""
execute = None
verbose = False
list = False

while len(sys.argv) > 1:
    arg = sys.argv.pop(1)
    options += " " + arg
    if arg == '--help':
        help_menu()
    elif arg == '--debug' or arg == '-d':
        debug = True
    elif arg == '--verbose' or arg == '-v':
        verbose = True
    elif arg == '--changed':
        changed = True
    elif arg == '--year':
        year = 0;
        if len(sys.argv) > 1:
            tmp = sys.argv.pop(1)
            try:
                year = int(tmp)
                if not validYear(year) :
                    print("Invalid year: " + tmp)
                    help_menu()
                current_year = year;
            except ValueError:
                year =0
                print("Invalid year: " + tmp)
                help_menu()
    elif arg == '--test':
        testExecution = True
    elif arg == '--list':
        list = True
    elif arg == '--ignored':
        list_ignored = True
    elif arg == '--code':
        ## starts form topLevelFolder/oscar
        start_dir = os.path.join(top_dir, 'oscar')
    elif arg == '--execute':
        execute = True
    elif arg == '--base':
        start_dir = top_dir
    else:
        tmp_dir = os.path.join(top_dir, arg)
        if os.path.isdir(tmp_dir):
            start_dir = tmp_dir
        else:
            print("Invalid Parameter: " + arg)
            help_menu()

relativeStartDir = os.path.relpath(start_dir, relative_sync_dir)

if execute is None:
    print("Requires --execute parameter to execute script")
    help_menu()
    exit()


# Call the walk function to process all files recursively
for root, dirs, files in os.walk(start_dir):
    for file in files:
        filename=os.path.join(root, file)
        if os.path.isfile(filename) and not isBinaryFile(filename):
            processFile(filename)

print(f"{os.path.basename(__file__)} {relativeStartDir} {options}")

if list : exit()

summary = f"""
Summary of Text Files searched               {statistics.files_total}
Number of files with date modified:          {statistics.files_date_changed}
Number of files with date already modified:  {statistics.files_date_already_changed}
Number of files with Copyright Check:        {statistics.files_needing_inspection}
Number of files ignored                      {statistics.files_ignored}
"""

print(summary)


