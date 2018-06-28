#include <string>

using namespace std;

string askpage_head =
    "<html><body>\n\
                       Upload a Faust file, please.<br>\n\
                       There are ";

string askpage_tail =
    " clients uploading at the moment.<br>\n\
                       <form action=\"/filepost\" method=\"post\" enctype=\"multipart/form-data\">\n\
                       <input name=\"file\" type=\"file\">\n\
                       <input type=\"submit\" value=\" Send \"></form>\n\
                       </body></html>";

string cannotcompile =
    "<html><body>Could not execute the provided DSP program for the given architecture file.</body></html>";

string nosha1present = "<html><body>The given SHA1 key is not present in the directory.</body></html>";

string invalidosorarchitecture =
    "<html><body>You have entered either an invalid operating system, an invalid architecture, or an invalid makefile command.\
    Requests should be of the form:<br/>os/architecture/command<br/>For example:<br/>osx/csound/binary</body></html>";

string invalidinstruction =
    "<html><body>The server only can generate binary, source, or svg for a given architecture.</body></html>";

string busypage = "<html><body>This server is busy, please try again later.</body></html>";

string completebuterrorpage =
    "<html><body>The upload has been completed but your Faust file is corrupt. It has not been "
    "registered.</body></html>";

string completebutmorethanoneDSPfile =
    "<html><body>The upload has been completed but there is more than one DSP file in your archive. Only one is "
    "allowed..</body></html>";

string completebutnoDSPfile =
    "<html><body>The upload has been completed but there is no DSP file in your archive. You must have one and only "
    "one.</body></html>";

string completebutdecompressionproblem =
    "<html><body>The upload has been completed but the server could not decompress the archive.</body></html>";

string completebutendoftheworld =
    "<html><body>An internal server error of epic proportions has occurred. This likely portends the end of the "
    "world.</body></html>";

string completebutnopipe = "<html><body>Could not create a PIPE to faust on the server.</body></html>";

string completebutnohash =
    "<html><body>The upload is completed but we could not generate a hash for your file.</body></html>";

string completebutcorrupt_head =
    "<html><body><p>The upload is completed but the file you uploaded is not a valid Faust file. \
  Make sure that it is either a file with an extension .dsp or an archive (tar.gz, tar.bz, tar \
  or zip) containing one .dsp file and potentially .lib files included by the .dsp file. \
  Furthermore, the code in these files must be valid Faust code.</p> \
  <p>Below is the STDOUT and STDERR for the Faust code you tried to compile. \
  If the two are empty, then your file structure is wrong. Otherwise, they will tell you \
  why Faust failed.</p>";

string completebutcorrupt_tail = "</body></html>";

string completebutalreadythere_head =
    "<html><body>The upload is completed but it looks like you have already uploaded this file.<br />Here is its SHA1 "
    "key: ";

string completebutalreadythere_tail = "<br />Use this key for all subsequent GET commands.</body></html>";

string completepage_head = "<html><body>The upload is completed.<br />Here is its SHA1 key: ";

string completepage_tail = "<br />Use this key for all subsequent GET commands.</body></html>";

string errorpage = "<html><body>This doesn't seem to be right.</body></html>";

string servererrorpage = "<html><body>An internal server error has occured.</body></html>";

string fileexistspage = "<html><body>This file already exists.</body></html>";

string debugstub = "<html><body>Rien ne s'est cass&eacute; la figure. F&eacute;licitations !</body></html>";
