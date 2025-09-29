#include "content.hh"
#include <cassert>

/*
 * True if it is a .dsp or a .lib source file
 */

bool isFaustFile(const fs::path& f)
{
    fs::path x = f.extension();
    bool     a = (x == ".dsp") || (x == ".lib");
    return a;
}

/*
 * True if it is a .wav or .flac audio file
 */

bool isAudioFile(const fs::path& f)
{
    fs::path x = f.extension();
    bool     a = (x == ".wav") || (x == ".flac");
    return a;
}

/*
 * Copy all Faust source files and additional resources (libraries and audio files) from src directory to destination
 * directory
 */

void copyFaustOrAudioFiles(const fs::path& src, const fs::path& dst)
{
    assert(is_directory(src));
    assert(is_directory(dst));
    for (const auto& entry : fs::directory_iterator(src)) {
        if (isFaustFile(entry.path())) {
            fs::copy_file(entry.path(), dst / entry.path().filename());
        } else if (isAudioFile(entry.path())) {
            fs::copy_file(entry.path(), dst / entry.path().filename());
        }
    }
}
