/************************************************************************
 FAUST Service
 Copyright (C) 2018, GRAME, Centre National de Creation Musicale
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

 ************************************************************************
 ************************************************************************/

#ifndef _MATCH_
#define _MATCH_

#include <vector>
#include <string>

void segmentUrl(const char* url);

std::vector<std::string> decomposeURL(const char* url);

bool matchURL(const char* url, const char* pat);

bool matchURL(const char* url, const char* pat, std::vector<std::string>& data);

#endif