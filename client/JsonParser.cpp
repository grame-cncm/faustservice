/************************************************************************
 FAUST Architecture File
 Copyright (C) 2017 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 This Architecture section is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 3 of
 the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; If not, see <http://www.gnu.org/licenses/>.
 
 EXCEPTION : As a special exception, you may create a larger work
 that contains this FAUST architecture section and distribute
 that work under terms of your choice, so long as this FAUST
 architecture section is not modified.
 
 ************************************************************************
 ************************************************************************/

#include <fstream>
#include <iostream>
#include <ctype.h>
#include "JsonParser.h"

// ---------------------------------------------------------------------
// Parse full JSON record describing available operating systems and
// associated architectures :
// {"os1" : ["arch1, "arch2",...], "os2" : ["arch1, "arch2",...],...}
// and store the result in map M. Returns true if parsing was successfull.
// This function is used by targetsDescriptionReceived() to parse the JSON
// record sent by the Web service.
//
bool parseOperatingSystemsList(const char*& p, vector<string>& platforms, map<string, vector<string> >& M)
{
    parseChar(p, '{');
    
    do {
        string          os;
        vector<string>  archlist;
        if (parseOperatingSystem(p, os, archlist)) {
            platforms.push_back(os);
            M[os] = archlist;
        } else {
            return false;
        }
    } while (tryChar(p,','));
    
    return parseChar(p, '}');
}

// ---------------------------------------------------------------------
// Parse operating system record :
//  "os" : ["arch1, "arch2",...]
// and store the result in os and al
//
bool parseOperatingSystem(const char*& p, string& os, vector<string>& al)
{
    return  parseString(p,os) && parseChar(p,':')
            && parseChar(p,'[')
            && parseArchitecturesList(p,al)
            && parseChar(p,']');
}

// ---------------------------------------------------------------------
// Parse an architecture list
//  "arch1, "arch2",...
// and store the result in a vector v
//
bool parseArchitecturesList(const char*& p, vector<string>& v)
{
    string s;
    do {

        if (parseString(p,s)) {
            v.push_back(s);
        } else {
            return false;
        }

    } while ( tryChar(p,','));
    return true;
}

// ---------------------------------------------------------------------
//                          Elementary parsers
// ---------------------------------------------------------------------

// Advance pointer p to the first non blank character
void skipBlank(const char*& p)
{
    while (isspace(*p)) { p++; }
}

// Report a parsing error
bool parseError(const char*& p, const char* errmsg )
{
    cerr << "Parse error : " << errmsg << " here : " << p << endl;
    return true;
}

// Parse character x, but don't report error if fails
bool tryChar(const char*& p, char x)
{
    skipBlank(p);
    if (x == *p) {
        p++;
        return true;
    } else {
        return false;
    }
}

//Parse character x, reports an error if it fails
bool parseChar(const char*& p, char x)
{
    skipBlank(p);
    if (x == *p) {
        p++;
        return true;
    } else {
        cerr << "parsing error : expected character '" << x << "'" << ", instead got : " << p << endl;
        return false;
    }
}

// Parse a quoted string "..." and store the result in s, reports an error if it fails
bool parseString(const char*& p, string& s)
{
    string str;
    skipBlank(p);

    const char* saved = p;

    if (*p++ == '"') {
        while ((*p != 0) && (*p != '"')) {
            str += *p++;
        }
        if (*p++=='"') {
            s = str;
            return true;
        }
    }
    p = saved;
    std::cerr << "parsing error : expected quoted string, instead got : "<< p << std::endl;
    return false;
}
