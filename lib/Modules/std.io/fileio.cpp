//
// Created by BraxtonN on 5/25/2018.
//

#include "../../runtime/oo/string.h"
#include "../../../stdimports.h"
#include "../../runtime/List.h"
#include  <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <dirent.h>
#include <utime.h>

#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif


native_string resolve_path(native_string& path) {
    native_string fullPath;

    char full_path[MAX_PATH];

    GetFullPathName(path.str().c_str(), MAX_PATH, full_path, NULL);

    for(int i = 0; i < MAX_PATH; i++) {
        if(full_path[i] != '\000')
            fullPath += full_path[i];
        else
            break;
    }
    return fullPath;
}

int FILE_EXISTS    = 0x01;
int FILE_REGULAR   = 0x02;
int FILE_DIRECTORY = 0x04;
int FILE_HIDDEN    = 0x08;
struct stat result;
long long get_file_attrs(native_string& path) {
    if(stat(path.str().c_str(), &result)==0)
    {
        long long mode = result.st_mode, attrs=0;

        // regular file
        if(S_ISREG(mode))
            attrs |= FILE_REGULAR;

        // directory
        if(S_ISDIR(mode))
            attrs |= FILE_DIRECTORY;

        // exists
        attrs |= FILE_EXISTS;

        long attributes = GetFileAttributes(path.c_str());
        if (attributes & FILE_ATTRIBUTE_HIDDEN)
            attrs |= FILE_HIDDEN;

        return attrs;
    }
    return 0;
}

/**
 *
 * @param path
 * @param access_flg
 * @return
 */
int check_access(native_string& path, int access_flg) {
    return _access( path.str().c_str(), access_flg );
}

long long last_update(native_string& path) {
    if(stat(path.str().c_str(), &result)==0)
    {
        return result.st_mtime;
    }
    return 0;
}

long long file_size(native_string &path)
{
    int rc = stat(path.str().c_str(), &result);
    return rc == 0 ? result.st_size : -1;
}

void create_file(native_string &path)
{
    std::ofstream o(path.str().c_str());
    o.close();
}

long delete_file(native_string &path)
{
    return remove(path.str().c_str());
}

List<native_string> get_file_list(native_string &path) {
    DIR *dir;
    List<native_string> files;
    struct dirent *ent;
    if ((dir = opendir (path.str().c_str())) != NULL) {
        /* print all the files and directories within directory */
        while ((ent = readdir (dir)) != NULL) {
            native_string file;
            for(long i = 0; i < ent->d_namlen; i++) {
                file += ent->d_name[i];
            }
            files.push_back(file);
        }
        closedir (dir);
    } else {
        /* could not open directory */
        return files;
    }
}

long make_dir(native_string &path)
{
    return _mkdir(path.str().c_str());
}

long delete_dir(native_string &path)
{
    return _rmdir(path.str().c_str());
}

long rename_file(native_string &path, native_string &newName)
{
    return rename(path.str().c_str(), newName.str().c_str());
}

time_t update_time(native_string &path, time_t time)
{
    struct utimbuf new_times;

    if(stat(path.str().c_str(), &result)==0) {
        new_times.actime = result.st_atime; /* keep atime unchanged */
        new_times.modtime = time;    /* set mtime to current time */
        utime(path.str().c_str(), &new_times);
        return time;
    }
    return -1;
}

#ifdef _WIN32

/// @Note If STRICT_UGO_PERMISSIONS is not defined, then setting Read for any
///       of User, Group, or Other will set Read for User and setting Write
///       will set Write for User.  Otherwise, Read and Write for Group and
///       Other are ignored.
///
/// @Note For the POSIX modes that do not have a Windows equivalent, the modes
///       defined here use the POSIX values left shifted 16 bits.

static const mode_t S_ISUID      = 0x08000000;           ///< does nothing
static const mode_t S_ISGID      = 0x04000000;           ///< does nothing
static const mode_t S_ISVTX      = 0x02000000;           ///< does nothing
#   ifndef STRICT_UGO_PERMISSIONS
static const mode_t S_IRGRP      = mode_t(_S_IREAD);     ///< read by *USER*
static const mode_t S_IWGRP      = mode_t(_S_IWRITE);    ///< write by *USER*
static const mode_t S_IXGRP      = 0x00080000;           ///< does nothing
static const mode_t S_IROTH      = mode_t(_S_IREAD);     ///< read by *USER*
static const mode_t S_IWOTH      = mode_t(_S_IWRITE);    ///< write by *USER*
static const mode_t S_IXOTH      = 0x00010000;           ///< does nothing
#   else
static const mode_t S_IRGRP      = 0x00200000;           ///< does nothing
static const mode_t S_IWGRP      = 0x00100000;           ///< does nothing
static const mode_t S_IXGRP      = 0x00080000;           ///< does nothing
static const mode_t S_IROTH      = 0x00040000;           ///< does nothing
static const mode_t S_IWOTH      = 0x00020000;           ///< does nothing
static const mode_t S_IXOTH      = 0x00010000;           ///< does nothing
#   endif
static const mode_t MS_MODE_MASK = 0x0000ffff;           ///< low word


int ACCESS_READ    = 0x04;
int ACCESS_WRITE   = 0x02;
int ACCESS_EXECUTE = 0x01;
int ACCESS_OK      = 0x00;
int __chmod(native_string &path, mode_t set_mode, bool enable, bool userOnly)
{
    if(stat(path.str().c_str(), &result)==0) {
        long long mode = result.st_mode;

        if (set_mode & ACCESS_READ) {

            if(enable)
                mode |= _S_IREAD;
            else
                mode ^= _S_IREAD;
        }

        if (set_mode & ACCESS_WRITE) {

            if(enable)
                mode |= _S_IWRITE;
            else
                mode ^= _S_IWRITE;
        }

        int result = _chmod(path.str().c_str(), mode);

        if (result != 0) {
            result = errno;
        }

        return (result);
    }

    return -1;
}
#else
int my_chmod(const char * path, mode_t mode)
{
    int result = chmod(path, mode);

    if (result != 0)
    {
        result = errno;
    }

    return (result);
}
#endif

const int SPACE_TOTAL  = 0;
const int SPACE_FREE   = 1;
const int SPACE_USABLE = 2;

long long disk_space(long request) {
    long long lpFreeBytesAvailable=0,
            lpTotalNumberOfBytes=0,
            lpTotalNumberOfFreeBytes=0;

    GetDiskFreeSpaceEx(NULL, (PULARGE_INTEGER)&lpFreeBytesAvailable,
                       (PULARGE_INTEGER)&lpTotalNumberOfBytes, (PULARGE_INTEGER)&lpTotalNumberOfFreeBytes);
    switch (request) {
        case SPACE_TOTAL:
            return lpTotalNumberOfBytes;
        case SPACE_FREE:
            return lpTotalNumberOfFreeBytes;
        case SPACE_USABLE:
            return lpFreeBytesAvailable;
        default:
            return 0;
    }
}

