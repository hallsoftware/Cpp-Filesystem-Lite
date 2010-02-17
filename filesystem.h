/* BSD - License

Copyright (c) 2010, Kevin Hall
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

* The names of contributors may not be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _FILESYSTEM_H__
#define _FILESYSTEM_H__

#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <fstream>

#if defined(_WIN32) || defined(_WIN64)

#define FS_WINDOWS_

#define WIN32_LEAN_AND_MEAN
#define NOMCX //no Modem Configuration Extensions
#define NOIME //no Input Method Manager
#define NOGDI //no GDI defines and routines
#define NOUSER //no USER defines and routines
#define NOHELP //no Help engine interface.
//#define NOSERVICE ???
//#define NOSYSPARAMSINFO ???
//#define NOWINABLE ???
#include <windows.h>
#include <winioctl.h>
#include <direct.h>
#include <ctype.h>

extern "C" {

struct TMN_REPARSE_DATA_BUFFER
{
    DWORD  ReparseTag;
    WORD   ReparseDataLength;
    WORD   Reserved;

	// IO_REPARSE_TAG_MOUNT_POINT specifics follow
    WORD   SubstituteNameOffset;
    WORD   SubstituteNameLength;
    WORD   PrintNameOffset;
    WORD   PrintNameLength;
    WCHAR  PathBuffer[1];
};

} //extern "C"

#define TMN_REPARSE_DATA_BUFFER_HEADER_SIZE \
			FIELD_OFFSET(TMN_REPARSE_DATA_BUFFER, SubstituteNameOffset)

#else

#define FS_POSIX_

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#endif

namespace filesystem
{

#if 1 // REGION: Exceptions
class string_conversion_error : public std::runtime_error
{
	const char*	location_file;
	int			location_line;

public:
	string_conversion_error(const char* what_, const char* file_, int line_)
		: std::runtime_error(what_)
		, location_file(file_)
		, location_line(line_)
	{
	}

	virtual ~string_conversion_error() throw()
	{
	}
	
	const char* source_file() const
	{ return location_file;}
	
	int source_line() const
	{ return location_line;}
};

class filesystem_error : public std::runtime_error
{
	const char*	location_file;
	int			location_line;
	std::string	path_1;
	std::string	path_2;
public:
	filesystem_error(const char* what_, const char* file_, int line_, const std::string& path1_, const std::string& path2_)
		: std::runtime_error(what_)
		, location_file(file_)
		, location_line(line_)
		, path_1(path1_)
		, path_2(path2_)
	{
	}

	virtual ~filesystem_error() throw()
	{
	}
	
	const char* source_file() const
	{ return location_file;}
	
	int source_line() const
	{ return location_line;}

	const std::string& path1() const
	{ return path_1; }

	const std::string& path2() const
	{ return path_2; }
};

namespace internal
{
#define GENERARE_STRING_CONVERSION_ERROR(msg)			generate_string_conversion_error(msg, __FILE__, __LINE__)

template <class T>
void generate_string_conversion_error(const char* msg, const char* file, T line);

template <>
void generate_string_conversion_error(const char* msg, const char* file, int line)
{
	throw string_conversion_error(msg, file, line);
}

#define GENERARE_FILESYSTEM_ERROR0()				generate_filesystem_error(__FILE__, __LINE__, "", "")
#define GENERARE_FILESYSTEM_ERROR1(path1)			generate_filesystem_error(__FILE__, __LINE__, path1, "")
#define GENERARE_FILESYSTEM_ERROR2(path1, path2)	generate_filesystem_error(__FILE__, __LINE__, path1, path2)

template <class T>
void generate_filesystem_error(const char* file, T line, const std::string& path1, const std::string& path2);

template <>
void generate_filesystem_error(const char* file, int line, const std::string& path1, const std::string& path2)
{
#ifdef FS_WINDOWS_
    char* msgBuffer;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msgBuffer,
        0, NULL );
	std::string msg(msgBuffer);
    LocalFree(msgBuffer);
	throw filesystem_error(msg.c_str(), file, line, path1, path2);
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
	throw filesystem_error(strerror(errno), file, line, path1, path2);
#endif //#ifdef FS_POSIX_
}
} //namespace internal

#endif // REGION: Exceptions

#if 1 // REGION: narrow/wide string conversion
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
void to_narrow_string(const std::wstring& in, T& out);

template<>
void to_narrow_string(const std::wstring& in, std::string& out)
{
	if (in.empty())
	{
		out.clear();
		return;
	}

	int wcharCount = static_cast<int>( 1+ in.size()); // including null terminator
	int bufferSize = ::WideCharToMultiByte(
						CP_UTF8,
						0,
						in.c_str(),
						wcharCount,
						NULL,
						0,
						NULL, NULL
					);

	std::vector<char> buffer(bufferSize);
	int result = ::WideCharToMultiByte(
						CP_UTF8,
						0,
						in.c_str(),
						wcharCount,
						&buffer[0],
						static_cast<int>(buffer.size()),
						NULL, NULL
					); 

	if (result == 0)
	{
		GENERARE_STRING_CONVERSION_ERROR("Could not convert wide string to narrow string");
	}
	out.assign(&buffer[0]);
}

template<class T>
void to_wide_string(const std::string& in, T& out);

template<>
void to_wide_string(const std::string& in, std::wstring& out)
{
	if (in.empty())
	{
		out.clear();
		return;
	}

	std::vector<wchar_t> buffer(in.size()+1, 0);
	if (!MultiByteToWideChar(CP_THREAD_ACP,
							 MB_PRECOMPOSED,
							 in.c_str(),
							 static_cast<int>(in.size()),
							 &buffer[0],
							 static_cast<int>(in.size())))
	{
		GENERARE_STRING_CONVERSION_ERROR("Could not convert narrow string to wide string");
	}

	out = &buffer[0];
}

#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
void to_narrow_string(const std::wstring& in, T& out);

template<>
void to_narrow_string(const std::wstring& in, std::string& out)
{
	if (in.empty())
	{
		out.clear();
		return;
	}

	size_t bufferSize = wcstombs(NULL,in.c_str(),0)+1; // including null terminator
	if (bufferSize == 0)
	{
		GENERARE_STRING_CONVERSION_ERROR("Could not convert wide string to narrow string");
	}

	std::vector<char> buffer(bufferSize);
	wcstombs(&buffer[0], in.c_str(), buffer.size());
	out.assign(&buffer[0]);
}

template<class T>
void to_wide_string(const std::string& in, T& out);

template<>
void to_wide_string(const std::string& in, std::wstring& out)
{
	if (in.empty())
	{
		out.clear();
		return;
	}

	size_t bufferSize = mbstowcs(NULL,in.c_str(),0)+1; // including null terminator
	if (bufferSize == 0)
	{
		GENERARE_STRING_CONVERSION_ERROR("Could not convert narrow string to wide string");
	}

	std::vector<wchar_t> buffer(bufferSize);
	mbstowcs(&buffer[0], in.c_str(), buffer.size());
	out.assign(&buffer[0]);
}
#endif //#ifdef FS_POSIX_

template<class T>
void to_narrow_string(const std::vector<std::wstring>& in, std::vector<T>& out);

template<>
void to_narrow_string(const std::vector<std::wstring>& in, std::vector<std::string>& out)
{
	out.clear();

	std::vector<std::wstring>::const_iterator it	= in.begin();
	std::vector<std::wstring>::const_iterator itEnd	= in.end();
	std::string temp;

	for (; it!=itEnd; ++it)
	{
		internal::to_narrow_string(*it, temp);
		out.push_back(temp);
	}
}

template<class T>
void to_wide_string(const std::vector<std::string>& in, std::vector<T>& out);

template<>
void to_wide_string(const std::vector<std::string>& in, std::vector<std::wstring>& out)
{
	out.clear();

	std::vector<std::string>::const_iterator it	= in.begin();
	std::vector<std::string>::const_iterator itEnd	= in.end();
	std::wstring temp;

	for (; it!=itEnd; ++it)
	{
		internal::to_wide_string(*it, temp);
		out.push_back(temp);
	}
}
} //namespace internal
#endif // REGION: narrow/wide string conversion

#if 1 // REGION: POSIX Glob
namespace internal
{
#ifdef FS_POSIX_
//template<class T>
//bool glob_match(std::basic_string<T>::const_iterator i_begin, std::basic_string<T>::const_iterator i_end,
//				std::basic_string<T>::const_iterator p_begin, std::basic_string<T>::const_iterator p_end)
template<class T>
bool glob_match(T i_begin, T i_end,
				T p_begin, T p_end)
{
	if (i_begin == i_end)
	{
		return (p_begin == p_end);
	}
	while ((i_begin != i_end) && (p_begin != p_end))
	{
		switch (*p_begin)
		{
			case '*':
			{
				return 
					glob_match(i_begin+1, i_end, p_begin, p_end)
					||
					glob_match(i_begin, i_end, p_begin+1, p_end) // needed since '*' can be non-consuming
					||
					glob_match(i_begin+1, i_end, p_begin+1, p_end);
			}
			case '\\':
			{
				++p_begin;
				if (p_begin == p_end)
				{
					return false;
				}
				// fall through
			}
			default:
			{
				if (*p_begin != *i_begin)
				{
					return false;
				}
				// fall through
			}
			case '?':
			{
				++i_begin;
				++p_begin;
				break;
			}
		}
	}
	return (i_begin == i_end);
}

template<class T>
bool glob_match(const std::basic_string<T>& in, const std::basic_string<T>& pattern)
{
	return glob_match(in.begin(), in.end(), pattern.begin(), pattern.end());
}
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: POSIX Glob

#if 1 // REGION: Win32 Utility Functions
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
void to_win32_path(T& path_str);

template<>
void to_win32_path(std::string& path_str)
{
	std::string::iterator it	= path_str.begin();
	std::string::iterator itEnd	= path_str.end();
	for (; it!=itEnd; ++it)
	{
		if (*it == '/')
		{
			*it = '\\';
		}
	}
}

template<>
void to_win32_path(std::wstring& path_str)
{
	std::wstring::iterator it		= path_str.begin();
	std::wstring::iterator itEnd	= path_str.end();
	for (; it!=itEnd; ++it)
	{
		if (*it == '/')
		{
			*it = '\\';
		}
	}
}

template<class T>
void remove_extended_fs_indicator(T& path_str);

template<>
void remove_extended_fs_indicator(std::string& path_str)
{
	std::string extended_fs_indicator("\\\\?\\");
	if (path_str.find(extended_fs_indicator) == 0)
	{
		path_str.erase(0, extended_fs_indicator.size());
	}
}

template<>
void remove_extended_fs_indicator(std::wstring& path_str)
{
	std::wstring extended_fs_indicator(L"\\\\?\\");
	if (path_str.find(extended_fs_indicator) == 0)
	{
		path_str.erase(0, extended_fs_indicator.size());
	}
}

template<class T>
void prepend_extended_fs_indicator(T& path_str);

template<>
void prepend_extended_fs_indicator(std::wstring& path_str)
{
	// Prevent expansion if path contains "$" (indicating environment variable replacement)
	if (path_str.find('$') != std::wstring::npos)
	{
		std::string npath;
		to_narrow_string(path_str, npath);
		throw filesystem_error("Path strings with environment variable indicators ('$') cannot be used in this context.", __FILE__, __LINE__, npath, "");
	}

	// Don't bother if a path is less than (MAX_PATH-12) characters.
	// MAX_PATH - 12 is needed when creating directories (due to the mandatory 8.3 minimum filename requirements).
	// Source: http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx#maxpath
	if (path_str.size() < (MAX_PATH - 12))
	{
		return;	// if the path is empty, we'll return here...
	}

	// Prevent expansion if path contains "." or ".." elements.
	// The extended indicator "\\?\" prevents expansion of "." and ".." path elements.
	// Source: http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx#maxpath
	//
	//	We don't have to worry about the path being exactly ".." or "." since these path
	//	strings are too short and the function will return inside the previous if block.
	//	We need to test for the following:
	//	* occurrances of "\..\"
	//	* occurrances of "\.\"
	//	* occurrances of "..\" at the beginning of the path string
	//	* occurrances of ".\" at the beginning of the path string
	//	* occurrances of "\.." at the end of the path string
	//	* occurrances of "\." at the end of the path string
	if ((path_str.find(L"\\..\\") != std::wstring::npos) ||
		(path_str.find(L"\\.\\") != std::wstring::npos) ||
		(path_str.find(L"..\\") == 0) ||
		(path_str.find(L".\\") == 0) ||
		(path_str.find(L"\\..", path_str.size()-3) != std::wstring::npos) ||
		(path_str.find(L"\\.", path_str.size()-2) != std::wstring::npos))
	{
		std::string npath;
		to_narrow_string(path_str, npath);
		throw filesystem_error("Path is too long to contain \".\" or \"..\".  Windows cannot properly expand the path string.", __FILE__, __LINE__, npath, "");
	}

	std::wstring extended_fs_indicator(L"\\\\?\\");
	std::wstring extended_unc_indicator(L"\\\\?\\UNC");
	if (path_str.find(extended_fs_indicator) != 0) // will also match the UNC extended filesystem indicator
	{
		if (path_str.find(L"\\\\") == 0) // "\\" found at the beginning of the string -- indicative of a UNC path
		{
			// "\\server\share" should become "\\?\UNC\server\share".
			// Source: http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx#maxpath
			path_str.swap(extended_unc_indicator.append(path_str.begin()+1, path_str.end()));
		}
		else
		{
			path_str.swap(extended_fs_indicator.append(path_str));
		}
	}
}

template<class T>
T cur_drive_path(const T& in);

template<>
std::wstring cur_drive_path(const std::wstring& in)
{
	std::wstring cpath;
	int	drive, curdrive;

	// Save current drive.
	curdrive = _getdrive();

	if ((in[0] >= 'A') && (in[0] <= 'Z'))
	{
		drive = 1 + (in[0] - 'A');
	}
	else if ((in[0] >= 'a') && (in[0] <= 'z'))
	{
		drive = 1 + (in[0] - 'a');
	}
	else
	{
		std::string nin;
		to_narrow_string(in, nin);
		GENERARE_FILESYSTEM_ERROR1(nin);
	}

	if(!_chdrive(drive))
	{
		wchar_t* buffer = _wgetdcwd(drive, 0, 0);

		if (buffer == NULL)
		{
			std::string nin;
			to_narrow_string(in, nin);
			GENERARE_FILESYSTEM_ERROR1(nin);
		}
		cpath = buffer;
		free(buffer);
	}
	else
	{
		std::string nin;
		to_narrow_string(in, nin);
		GENERARE_FILESYSTEM_ERROR1(nin);
	}

	_chdrive( curdrive ); // Restore original drive.

	return cpath;
}

template<>
std::string cur_drive_path(const std::string& in)
{
	std::string		result;
	std::wstring	wresult = cur_drive_path(std::wstring(1, in[0]));
	to_narrow_string(wresult, result);
	return result;
}
#endif //#ifdef FS_WINDOWS_
} //namespace internal
#endif // REGION: Win32 Utility Functions

#if 1 // REGION: compare_path_element
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
bool compare_path_element(T e1, T e2);

template<>
bool compare_path_element(std::string e1, std::string e2)
{
	std::string::iterator it	= e1.begin();
	std::string::iterator itEnd	= e1.end();
	for (; it!=itEnd; ++it)
	{
		if (__isascii(*it) && isupper(*it))
		{
			*it = _tolower(*it);
		}
	}
	it		= e2.begin();
	itEnd	= e2.end();
	for (; it!=itEnd; ++it)
	{
		if (__isascii(*it) && isupper(*it))
		{
			*it = _tolower(*it);
		}
	}

	return (e1 == e2);
}

template<>
bool compare_path_element(std::wstring e1, std::wstring e2)
{
	std::wstring::iterator it		= e1.begin();
	std::wstring::iterator itEnd	= e1.end();
	for (; it!=itEnd; ++it)
	{
		*it = _tolower(*it);
	}
	it		= e2.begin();
	itEnd	= e2.end();
	for (; it!=itEnd; ++it)
	{
		*it = towlower(*it);
	}

	return (e1 == e2);
}
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
bool compare_path_element(T e1, T e2)
{
	return (e1 == e2);
}
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: compare_path_element

#if 1 // REGION: current_working_dir
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
void current_working_dir(T& path);

template<>
void current_working_dir(std::wstring& path)
{
	DWORD bufferSize = GetCurrentDirectoryW(0, 0)+1;
	std::vector<wchar_t> buffer(bufferSize);
	if (0==GetCurrentDirectoryW(bufferSize, &buffer[0]))
	{
		GENERARE_FILESYSTEM_ERROR0();
	}
	path.assign(&buffer[0]);
}

template<>
void current_working_dir(std::string& path)
{
	std::wstring wpath;
	current_working_dir(wpath);
	to_narrow_string(wpath, path);
}
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
void current_working_dir(T& path);

template<>
void current_working_dir(std::string& path)
{
	char* pbuffer = getcwd(0, 0);
	path = pbuffer;
	free(pbuffer);
}

template<>
void current_working_dir(std::wstring& path)
{
	std::string npath;
	current_working_dir(npath);
	to_wide_string(npath, path);
}
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: current_working_dir

#if 1 // REGION: full_pathname
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
T full_pathname(const T& in);

template<>
std::wstring full_pathname(const std::wstring& in)
{
	std::wstring extended_in = in;
	prepend_extended_fs_indicator(extended_in);
	DWORD bufferSize = GetFullPathNameW(extended_in.c_str(), 0, 0, NULL);
	std::vector<wchar_t> buffer(bufferSize);
	if (0==GetFullPathNameW(extended_in.c_str(), bufferSize, &buffer[0], NULL))
	{
		std::string nin;
		to_narrow_string(in, nin);
		GENERARE_FILESYSTEM_ERROR1(nin);
	}
	return std::wstring(&buffer[0]);
}

template<>
std::string full_pathname(const std::string& in)
{
	std::wstring win;
	to_wide_string(in, win);
	std::wstring wresult = full_pathname(win);
	std::string	result;
	to_narrow_string(wresult, result);
	return result;
}
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
T full_pathname(const T& in);

template<>
std::string full_pathname(const std::string& in)
{
	char* buffer = realpath(in.c_str(), 0);
	std::string result(buffer);
	free(buffer);
	return result;
}

template<>
std::wstring full_pathname(const std::wstring& in)
{
	std::string nin;
	to_narrow_string(in, nin);
	std::string nresult = full_pathname(nin);
	std::wstring result;
	to_wide_string(nresult, result);
	return result;
}
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: full_pathname

#if 1 // REGION: is_directory_empty
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
bool is_directory_empty(T dir);

template<>
bool is_directory_empty(std::wstring dir)
{
	bool result = true; // we'll assume the directory is empty

	to_win32_path(dir);
	prepend_extended_fs_indicator(dir);
	dir.append(L"\\*");

	WIN32_FIND_DATAW FindFileData;
	HANDLE hFind;

	hFind = FindFirstFileW(dir.c_str(), &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) 
	{
		throw; // failed to access directory (directory doesn't exist?)
	}
	BOOL foundEntry = TRUE;
	do
	{
		if ((wcscmp(FindFileData.cFileName, L".") != 0) &&
			(wcscmp(FindFileData.cFileName, L"..") != 0))
		{
			result = false;
			break;
		}
		foundEntry = FindNextFileW(hFind, &FindFileData);
	}
	while (foundEntry);

	FindClose(hFind);
	return result;
}

template<>
bool is_directory_empty(std::string dir)
{
	std::wstring wdir;
	to_wide_string(dir, wdir);
	return is_directory_empty(wdir);
}
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
bool is_directory_empty(T dir);

template<>
bool is_directory_empty(std::string directory)
{
    DIR*			dir;
    struct dirent*	ent;
    struct stat		st;
	bool			result = true;

    dir = opendir(directory.c_str());
	if (dir == NULL)
	{
		GENERARE_FILESYSTEM_ERROR1(directory);
	}
	while (result && ((ent = readdir(dir)) != NULL)) {
		std::string file_name = ent->d_name;
		std::string full_file_name = directory + "/" + file_name;
		
		if ((file_name == ".") || (file_name == ".."))
			continue;
			
		if (stat(full_file_name.c_str(), &st) == -1)
			continue;

		result = false;
	}
    closedir(dir);
	return result;
}

template<>
bool is_directory_empty(std::wstring dir)
{
	std::string ndir;
	to_narrow_string(dir, ndir);
	return is_directory_empty(ndir);
}
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: is_directory_empty

#if 1 // REGION: is_file, is_directory
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
bool is_file(const T&);

template<>
bool is_file(const std::wstring& path)
{
	std::wstring ext_path = path;
	to_win32_path(ext_path);
	prepend_extended_fs_indicator(ext_path);

	DWORD result = GetFileAttributesW(ext_path.c_str());
	if ((result == INVALID_FILE_ATTRIBUTES) ||
		((result & FILE_ATTRIBUTE_DEVICE) != 0) ||
		((result & FILE_ATTRIBUTE_DIRECTORY) != 0) ||
		((result & FILE_ATTRIBUTE_OFFLINE) != 0)) // For now, we won't allow access to offline files.
	{
		DWORD result = GetFileAttributesW(ext_path.c_str());
		if ((result == INVALID_FILE_ATTRIBUTES) ||
			((result & FILE_ATTRIBUTE_DEVICE) != 0) ||
			((result & FILE_ATTRIBUTE_DIRECTORY) != 0) ||
			((result & FILE_ATTRIBUTE_OFFLINE) != 0)) // For now, we won't allow access to offline files.
		{
			return false;
		}
	}
	return true;
}

template<>
bool is_file(const std::string& path)
{
	std::wstring wpath;
	to_wide_string(path, wpath);
	return is_file(wpath);
}

template<class T>
bool is_directory(const T&);

template<>
bool is_directory(const std::wstring& path)
{
	std::wstring ext_path = path;
	to_win32_path(ext_path);
	prepend_extended_fs_indicator(ext_path);

	DWORD result = GetFileAttributesW(ext_path.c_str());
	return ((result != INVALID_FILE_ATTRIBUTES) &&
			((result & FILE_ATTRIBUTE_DIRECTORY) != 0));
}

template<>
bool is_directory(const std::string& path)
{
	std::wstring wpath;
	to_wide_string(path, wpath);
	return is_directory(wpath);
}
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
bool is_file(const T&);

template<>
bool is_file(const std::string& path)
{
	struct stat	info;
	if (lstat(path.c_str(), &info) != 0)
	{
		GENERARE_FILESYSTEM_ERROR1(path);
	}
	return S_ISREG(info.st_mode);
}

template<>
bool is_file(const std::wstring& path)
{
	std::string npath;
	to_narrow_string(path, npath);
	return is_file(npath);
}

template<class T>
bool is_directory(const T&);

template<>
bool is_directory(const std::string& path)
{
	struct stat	info;
	if (lstat(path.c_str(), &info) != 0)
	{
		GENERARE_FILESYSTEM_ERROR1(path);
	}
	return S_ISDIR(info.st_mode);
}

template<>
bool is_directory(const std::wstring& path)
{
	std::string npath;
	to_narrow_string(path, npath);
	return is_directory(npath);
}
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: is_file, is_directory

#if 1 // REGION: dir_get_subdirs, dir_get_files
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
void scan_directory(std::basic_string<T> dir, const std::basic_string<T>& pattern, std::vector<std::basic_string<T> >& results, bool search_directories);

template<>
void scan_directory(std::wstring dir, const std::wstring& pattern, std::vector<std::wstring>& results, bool search_directories)
{
	results.clear();

	DWORD fileAttrMask = FILE_ATTRIBUTE_DIRECTORY;
	DWORD fileAttrComp = FILE_ATTRIBUTE_DIRECTORY;
	if (!search_directories)
	{
		fileAttrMask = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_OFFLINE;
		fileAttrComp = 0;
	}

	to_win32_path(dir);
	prepend_extended_fs_indicator(dir);
	dir.push_back('\\');
	if (pattern.empty())
	{
		dir.push_back('*');
	}
	else
	{
		dir.append(pattern);
	}

	WIN32_FIND_DATAW FindFileData;
	HANDLE hFind;

	hFind = FindFirstFileW(dir.c_str(), &FindFileData);
	if (hFind == INVALID_HANDLE_VALUE) 
	{
		return;
	}
	BOOL foundEntry = TRUE;
	do
	{
		if ((FindFileData.dwFileAttributes & fileAttrMask) == fileAttrComp)
		{
			if ((wcscmp(FindFileData.cFileName, L".") != 0) &&
				(wcscmp(FindFileData.cFileName, L"..") != 0))
			{
				results.push_back(FindFileData.cFileName);
			}
		}
		foundEntry = FindNextFileW(hFind, &FindFileData);
	}
	while (foundEntry);

	FindClose(hFind);
}

template<>
void scan_directory(std::string dir, const std::string& pattern, std::vector<std::string>& results, bool search_directories)
{
	results.clear();

	std::wstring wdir, wpattern;
	std::vector<std::wstring> wsubdirs;

	internal::to_wide_string(dir, wdir);
	internal::to_wide_string(pattern, wpattern);

	scan_directory(wdir, wpattern, wsubdirs, search_directories);

	to_narrow_string(wsubdirs, results);
}

#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
void scan_directory(std::basic_string<T> directory, const std::basic_string<T>& pattern, std::vector<std::basic_string<T> >& results, bool search_directories);

template<>
void scan_directory(std::string directory, const std::string& pattern, std::vector<std::string>& results, bool search_directories)
{
	DIR *dir;
	struct dirent *ent;
	struct stat st;

	const bool no_pattern = pattern.empty();

	dir = opendir(directory.c_str());
	while ((ent = readdir(dir)) != NULL) {
		std::string file_name = ent->d_name;
		std::string full_file_name = directory + "/" + file_name;

		if ((file_name == ".") || (file_name == ".."))
			continue;

		if (stat(full_file_name.c_str(), &st) == -1)
			continue;

		if (search_directories)
		{
			if (!S_ISDIR(st.st_mode)) // directory
				continue;
		}
		else
		{
			if (!S_ISREG(st.st_mode)) // file
				continue;
		}

		if (no_pattern || glob_match(file_name, pattern))
		{
			results.push_back(file_name);
		}
	}
	closedir(dir);
}

template<>
void scan_directory(std::wstring directory, const std::wstring& pattern, std::vector<std::wstring>& results, bool search_directories)
{
	results.clear();

	std::string ndir, npattern;
	std::vector<std::string> nsubdirs;

	internal::to_narrow_string(directory, ndir);
	internal::to_narrow_string(pattern, npattern);

	scan_directory(ndir, npattern, nsubdirs, search_directories);

	to_wide_string(nsubdirs, results);
}
#endif //#ifdef FS_POSIX_

template<class T>
void dir_get_subdirs(const T& dir, const T& pattern, std::vector<T>& results);

template<>
void dir_get_subdirs(const std::wstring& dir, const std::wstring& pattern, std::vector<std::wstring>& results)
{
	scan_directory(dir, pattern, results, true);
}

template<>
void dir_get_subdirs(const std::string& dir, const std::string& pattern, std::vector<std::string>& results)
{
	scan_directory(dir, pattern, results, true);
}

template<class T>
void dir_get_files(const T& dir, const T& pattern, std::vector<T>& results);

template<>
void dir_get_files(const std::wstring& dir, const std::wstring& pattern, std::vector<std::wstring>& results)
{
	scan_directory(dir, pattern, results, false);
}

template<>
void dir_get_files(const std::string& dir, const std::string& pattern, std::vector<std::string>& results)
{
	scan_directory(dir, pattern, results, false);
}
} //namespace internal
#endif // REGION: dir_get_subdirs, dir_get_files

#if 1 // REGION: create_directory, remove_directory
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
void create_directory(const T& dir);

template<>
void create_directory(const std::wstring& dir)
{
	std::wstring ext_dir = dir;
	to_win32_path(ext_dir);
	prepend_extended_fs_indicator(ext_dir);

	if (0 == CreateDirectoryW(ext_dir.c_str(), NULL))
	{
		std::string ndir;
		to_narrow_string(dir, ndir);
		GENERARE_FILESYSTEM_ERROR1(ndir);
	}
}

template<>
void create_directory(const std::string& dir)
{
	std::wstring wdir;
	to_wide_string(dir, wdir);
	create_directory(wdir);
}

template<class T>
void remove_directory(const T& dir);

template<>
void remove_directory(const std::wstring& dir)
{
	std::wstring ext_dir = dir;
	to_win32_path(ext_dir);
	prepend_extended_fs_indicator(ext_dir);

	if (0 == RemoveDirectoryW(ext_dir.c_str()))
	{
		std::string ndir;
		to_narrow_string(dir, ndir);
		GENERARE_FILESYSTEM_ERROR1(ndir);
	}
}

template<>
void remove_directory(const std::string& dir)
{
}
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
void create_directory(const T& dir);

template<>
void create_directory(const std::string& dir)
{
	if (mkdir(dir.c_str(), S_IRWXU | S_IRWXO | S_IRWXG) != 0)
	{
		GENERARE_FILESYSTEM_ERROR1(dir);
	}
}

template<>
void create_directory(const std::wstring& dir)
{
	std::string ndir;
	to_narrow_string(dir, ndir);
	create_directory(ndir);
}	

template<class T>
void remove_directory(const T& dir);

template<>
void remove_directory(const std::string& dir)
{
	if (rmdir(dir.c_str()) != 0)
	{
		GENERARE_FILESYSTEM_ERROR1(dir);
	}
}

template<>
void remove_directory(const std::wstring& dir)
{
	std::string ndir;
	to_narrow_string(dir, ndir);
	remove_directory(ndir);
}
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: create_directory, remove_directory

#if 1 // REGION: remove_file
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
void remove_file(const T&);

template<>
void remove_file(const std::wstring& path)
{
	std::wstring ext_path = path;
	to_win32_path(ext_path);
	prepend_extended_fs_indicator(ext_path);

	if (0 == DeleteFileW(ext_path.c_str()))
	{
		std::string npath;
		to_narrow_string(path, npath);
		GENERARE_FILESYSTEM_ERROR1(npath);
	}
}

template<>
void remove_file(const std::string& path)
{
	std::wstring wpath;
	to_wide_string(path, wpath);
	remove_file(wpath);
}
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
bool remove_file(const T& path);

template<>
bool remove_file(const std::string& path)
{
	if (unlink(path.c_str()) != 0)
	{
		GENERARE_FILESYSTEM_ERROR1(path);
	}
	return true;
}

template<>
bool remove_file(const std::wstring& path)
{
	std::string npath;
	to_narrow_string(path, npath);
	return remove_file(npath);
}	
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: remove_file

#if 1 // REGION: move
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
void move(const T& oldpath, const T& newpath);

template<>
void move(const std::wstring& oldpath, const std::wstring& newpath)
{
	std::wstring ext_oldpath = oldpath;
	to_win32_path(ext_oldpath);
	prepend_extended_fs_indicator(ext_oldpath);

	std::wstring ext_newpath = newpath;
	to_win32_path(ext_newpath);
	prepend_extended_fs_indicator(ext_newpath);

	if (0 == MoveFileW(ext_oldpath.c_str(), ext_newpath.c_str()))
	{
		std::string noldpath;
		to_narrow_string(oldpath, noldpath);
		std::string nnewpath;
		to_narrow_string(newpath, nnewpath);
		GENERARE_FILESYSTEM_ERROR2(noldpath, nnewpath);
	}
}

template<>
void move(const std::string& oldpath, const std::string& newpath)
{
	std::wstring woldpath, wnewpath;
	to_wide_string(oldpath, woldpath);
	to_wide_string(newpath, wnewpath);
	move(woldpath, wnewpath);
}
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
void move(const T& oldpath, const T& newpath);

template<>
void move(const std::string& oldpath, const std::string& newpath)
{
	if (rename(oldpath.c_str(), newpath.c_str()) != 0)
	{
		GENERARE_FILESYSTEM_ERROR2(oldpath, newpath);
	}
}

template<>
void move(const std::wstring& oldpath, const std::wstring& newpath)
{
	std::string noldpath, nnewpath;
	to_narrow_string(oldpath, noldpath);
	to_narrow_string(newpath, nnewpath);
	move(noldpath, nnewpath);
}
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: move

#if 1 // REGION: create_hard_link
namespace internal
{
#ifdef FS_WINDOWS_
template<class T>
void create_hard_link(const T& link, const T& source);

template<>
void create_hard_link(const std::wstring& link, const std::wstring& source)
{
	std::wstring ext_link = link;
	to_win32_path(ext_link);
	prepend_extended_fs_indicator(ext_link);

	std::wstring ext_source = source;
	to_win32_path(ext_source);
	prepend_extended_fs_indicator(ext_source);

	if (0 == CreateHardLinkW(ext_link.c_str(), ext_source.c_str(), NULL))
	{
		std::string nlink;
		to_narrow_string(link, nlink);
		std::string nsource;
		to_narrow_string(source, nsource);
		GENERARE_FILESYSTEM_ERROR2(nlink, nsource);
	}
}

template<>
void create_hard_link(const std::string& link, const std::string& source)
{
	std::wstring wlink, wsource;
	to_wide_string(link, wlink);
	to_wide_string(source, wsource);
	create_hard_link(wlink, wsource);
}
#endif //#ifdef FS_WINDOWS_
#ifdef FS_POSIX_
template<class T>
void create_hard_link(const T& oldpath, const T& newpath);

template<>
void create_hard_link(const std::string& oldpath, const std::string& newpath)
{
	if (link(oldpath.c_str(), newpath.c_str()) != 0)
	{
		GENERARE_FILESYSTEM_ERROR2(oldpath, newpath);
	}
}

template<>
void create_hard_link(const std::wstring& oldpath, const std::wstring& newpath)
{
	std::string noldpath, nnewpath;
	to_narrow_string(oldpath, noldpath);
	to_narrow_string(newpath, nnewpath);
	create_hard_link(noldpath, nnewpath);
}
#endif //#ifdef FS_POSIX_
} //namespace internal
#endif // REGION: create_hard_link

#if 1 // REGION: exists
namespace internal
{
template<class T>
bool exists(const T& path)
{   
	return is_directory(path) || is_file(path);
}
} //namespace internal
#endif // REGION: exists

#if 1 // REGION: Win32 Junction Points
#ifdef FS_WINDOWS_
namespace internal
{
template<class T>
bool create_junction_point(T link, T source);

template<>
bool create_junction_point(std::wstring link, std::wstring source)
{
	if (link.empty() || source.empty())
	{
		return false;
	}

	to_win32_path(link);
	prepend_extended_fs_indicator(link);

	remove_extended_fs_indicator(source);
	to_win32_path(source);
	std::wstring(L"\\??\\").append(source).swap(source);

	char raw_reparse_buffer[MAXIMUM_REPARSE_DATA_BUFFER_SIZE] = { 0 };
	TMN_REPARSE_DATA_BUFFER& reparse_buffer = *reinterpret_cast<TMN_REPARSE_DATA_BUFFER*>(raw_reparse_buffer);

	const WORD nDestMountPointBytes = static_cast<WORD>(source.size()) * 2;

	reparse_buffer.ReparseTag           = IO_REPARSE_TAG_MOUNT_POINT;
	reparse_buffer.ReparseDataLength    = nDestMountPointBytes + 12;
	reparse_buffer.Reserved             = 0;
	reparse_buffer.SubstituteNameOffset = 0;
	reparse_buffer.SubstituteNameLength = nDestMountPointBytes;
	reparse_buffer.PrintNameOffset      = nDestMountPointBytes + 2;
	reparse_buffer.PrintNameLength      = 0;
	lstrcpyW(reparse_buffer.PathBuffer, source.c_str());

	HANDLE dirHandle = 
			CreateFile(	link.c_str(),
						GENERIC_READ | GENERIC_WRITE,
						0,
						0,
						OPEN_EXISTING,
						FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
						0);
	if (dirHandle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD dummy;
	return (0 !=
		DeviceIoControl(dirHandle,
						FSCTL_SET_REPARSE_POINT,
						(LPVOID)&reparse_buffer,
						reparse_buffer.ReparseDataLength + TMN_REPARSE_DATA_BUFFER_HEADER_SIZE,
						NULL,
						0,
						&dummy,
						0));
}

template<>
bool create_junction_point(std::string link, std::string source)
{
	std::wstring wlink, wsource;
	to_wide_string(link, wlink);
	to_wide_string(source, wsource);
	return create_junction_point(wlink, wsource);
}

template<class T>
bool is_junction_point(const T&);

template<>
bool is_junction_point(const std::wstring& path)
{
	std::wstring ext_path = path;
	to_win32_path(ext_path);
	prepend_extended_fs_indicator(ext_path);

	DWORD result = GetFileAttributesW(ext_path.c_str());
	return ((result != INVALID_FILE_ATTRIBUTES) &&
			((result & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)));
}

template<>
bool is_junction_point(const std::string& path)
{
	std::wstring wpath;
	to_wide_string(path, wpath);
	return is_junction_point(wpath);
}
}
#endif //#ifdef FS_WINDOWS_
#endif // REGION: Win32 Junction Points

#if 1 // REGION: class basic_path

struct Initializer
{
	enum Enum
	{
		CurrentWorkingDirectory,
	};
};

template<class T>
class basic_path
{
public:
	typedef T						char_t;
	typedef std::basic_string<T>	string_t;
	typedef std::vector<string_t>	vecstr_t;

private:
	vecstr_t			path_elems;
	mutable	string_t	path_string;
	mutable bool		path_string_valid;
	bool				relative;
	bool				drive_specified;
	bool				unc_path;

	void initialize(string_t path_)
	{
		#ifdef FS_WINDOWS_
			internal::remove_extended_fs_indicator(path_);
		#endif //#ifdef FS_WINDOWS_

		if (path_.empty())
		{
			throw filesystem_error("Path cannot be initialized to empty value", __FILE__, __LINE__, "", "");
		}

		typename string_t::iterator it	 = path_.begin();
		typename string_t::iterator itEnd = path_.end();
		for (; it!=itEnd; ++it)
		{
			if (*it == '\\')
			{
				*it = '/';
			}
		}

		if (path_.find(string_t(2, '/').c_str(), 1) != string_t::npos) // find "//"
		{
			throw filesystem_error("Path encountered unexpected neighboring directory separators: '//'", __FILE__, __LINE__, "", "");
		}
		if ((path_.find(string_t(3, '.').c_str()) == 0) &&
			(path_.find_first_not_of('.') == string_t::npos)) // find repeats of elipses more than 2 in length
		{
			throw filesystem_error("Invalid path specified", __FILE__, __LINE__, "", "");
		}

		size_t searchIndex = 0;
		size_t nextDelimiter = 0;

		// Detect non-relative and/or drive-specified paths
		if (path_[0] == '/')
		{
			relative = false;
			if (path_.length() == 1)
			{
				// our path is "/".  We can short-cut and return.
				path_elems.push_back(path_);
				return;
			}
			if (path_[1] == '/')
			{
				// we have a UNC path
				unc_path = true;
				nextDelimiter = path_.find('/', 2);
				if (nextDelimiter == string_t::npos)
				{
					// we only have the root path.  We can short-cut and return.
					path_elems.push_back(path_);
					return;
				}
				path_elems.push_back(path_.substr(0, nextDelimiter));
				searchIndex = nextDelimiter + 1;
			}
			else
			{
				// we have a non-UNC root path.  Just set the first element to "/"
				path_elems.push_back(string_t(1, '/'));
				searchIndex = 1;
			}
		}
		else
		{
			if ((path_.length() >= 2) &&
				(path_[1] == ':') &&
				(
				 ((path_[0] >= 'A') && (path_[0] <= 'Z')) ||
				 ((path_[0] >= 'a') && (path_[0] <= 'z'))
				)
			   )
			{
				// we have a drive specification.
				drive_specified = true;

				if ((path_.length() >= 3) &&
					(path_[2] == '/'))
				{
					relative = false;
					path_elems.push_back(path_.substr(0, 3));
					searchIndex = 3;
				}
				else
				{
					path_elems.push_back(path_.substr(0, 2));
					searchIndex = 2;
				}
			}
		}

		// Now parse the remainding part of the path string.
		if (searchIndex != string_t::npos)
		{
			for (;;)
			{
				nextDelimiter = path_.find('/', searchIndex);
				if (nextDelimiter == string_t::npos)
				{
					string_t lastElem = path_.substr(searchIndex);
					if (!lastElem.empty())
					{
						path_elems.push_back(lastElem);
					}
					break;
				}
				path_elems.push_back(path_.substr(searchIndex, nextDelimiter-searchIndex));
				searchIndex = nextDelimiter+1;
			}
		}

		// Path cleanup & final validity checks

		// Validate environment variable members.  Ex: "$(PATH)"
		{
			typename vecstr_t::iterator	it		= path_elems.begin();
			typename vecstr_t::iterator	endIt	= path_elems.end();
			for (; it!=endIt; ++it)
			{
				size_t dollarSignLocation = it->find('$');
				if (dollarSignLocation != string_t::npos)
				{
					if ((dollarSignLocation != 0) ||			// '(' is not where we expect it.
						(it->length() < 4) ||					// element is too short.  It can't hold "$(A)".
						(it->find('$', 1) != string_t::npos) ||	// There's too many dollar signs.
						(it->find('(') != 1) ||					// '(' is not where we expect it.
						(it->find('(', 2) != string_t::npos) || // There's too many open parentheses.
						(it->find(')') != it->length()-1))		// ')' is not where we expect it.
					{
						throw filesystem_error("Path element contains '$', but has invalid environment variable format", __FILE__, __LINE__, "", "");
					}
					// Everything is OK if we reach here.
				}
				else if ((it->find('(') != string_t::npos) ||
						 (it->find('(') != string_t::npos))
				{
					throw filesystem_error("Path element contains parentheses, but has invalid environment variable format", __FILE__, __LINE__, "", "");
				}
			}
		}

		// Remove "."
		path_elems.erase(std::remove(path_elems.begin(), path_elems.end(), string_t(1, '.')), path_elems.end());
		if (path_elems.size() == 0)
		{
			path_elems.push_back(string_t(1, '.')); // The path was something like "." or "././." indicating the current path.  We need to keep this.
		}

		// Remove "..".
		remove_double_elipses();
	}

	void remove_double_elipses()
	{
		// This section is O(n^2) and could likely be optimized further.  But it works for now and is clear.
		string_t	doubleElipses(2, '.'); // ".."

		bool isDone = false;
		while (!isDone)
		{
			typename vecstr_t::iterator	endIt = path_elems.end();
			typename vecstr_t::iterator	startIt = path_elems.begin();
			if (drive_specified || unc_path || (!relative))
			{
				++startIt;
			}
			typename vecstr_t::iterator	prevIt	= startIt;
			typename vecstr_t::iterator	it		= startIt;
			if (it != endIt)
			{
				++it;
				isDone = true;
				while (it != endIt)
				{
					if ((*it == doubleElipses) &&
						(*prevIt != doubleElipses) &&
						((*prevIt)[0] != '$'))
					{
						path_elems.erase(prevIt, it+1);
						isDone = false;
						break;
					}
					prevIt = it;
					++it;
				}
			}
		}
	}

	const string_t& get_path_string() const
	{
		if (!path_string_valid)
		{
			size_t	elementCount = path_elems.size();
			size_t	element = 1;

			path_string = path_elems[0];

			if ((elementCount > 1) && (!unc_path))
			{
				if (!relative) // ex: "c:/" or "/"
				{
					path_string.append(path_elems[1]);
					element = 2;
				}
			}

			for (; element<elementCount; ++element)
			{
				path_string.append(string_t(1, '/'));
				path_string.append(path_elems[element]);
			}

			path_string_valid = true;
		}
		return path_string;
	}

	bool get_variable_id(const string_t& elem, string_t& varId) const
	{
		// assert(!elem.empty());
		if (elem[0] == '$')
		{
			size_t elemSize = elem.size();
			if ((elem[1] != '(') || (elem[elemSize-1] != ')'))
			{
				assert(false);  // we shouldn't get here.
				throw filesystem_error("Assertion failure", __FILE__, __LINE__, "", "");
			}
			varId = elem.substr(2, elemSize-3);
			return true;
		}
		return false;
	}

	void directory_scan_subdirs_for_files_helper(const string_t& pattern, std::vector<basic_path>& results)
	{
		std::vector<basic_path> dir_results;
		directory_get_files(pattern, dir_results);
		
		results.insert(results.end(), dir_results.begin(), dir_results.end());
		
		directory_get_subdirs(dir_results);

		std::vector<basic_path>::iterator it	= dir_results.begin();
		std::vector<basic_path>::iterator itEnd	= dir_results.end();
		for (; it!=itEnd; ++it)
		{
			it->directory_scan_subdirs_for_files_helper(pattern, results);
		}
	}

public:
	basic_path(const string_t& path_)
		: path_string_valid(false)
		, relative(true)
		, drive_specified(false)
		, unc_path(false)
	{
		initialize(path_);
	}

	basic_path(const char_t* path_)
		: path_string_valid(false)
		, relative(true)
		, drive_specified(false)
		, unc_path(false)
	{
		initialize(string_t(path_));
	}

	basic_path(Initializer::Enum initializer)
		: path_string_valid(false)
		, relative(true)
		, drive_specified(false)
		, unc_path(false)
	{
		switch (initializer)
		{
			case Initializer::CurrentWorkingDirectory:
			{
				string_t path;
				internal::current_working_dir(path);
				initialize(path);
				break;
			}
			default:
			{
				throw filesystem_error("Invalid path initializer specified", __FILE__, __LINE__, "", "");
			}
		}
	}

	basic_path& operator=(const string_t& path_string)
	{
		basic_path newPath(path_string);
		swap(newPath);
		return *this;
	}

	basic_path& operator=(Initializer::Enum initializer)
	{
		basic_path newPath(initializer);
		swap(newPath);
		return *this;
	}

	void swap(basic_path& other)
	{
		path_elems.swap(other.path_elems);
		path_string.swap(other.path_string);

		std::swap(path_string_valid,	other.path_string_valid);
		std::swap(relative,				other.relative);
		std::swap(drive_specified,		other.drive_specified);
		std::swap(unc_path,				other.unc_path);
	}

	basic_path operator/(const string_t& other)
	{
		string_t temp = to_portable_string();
		temp.append(string_t(1,'/')).append(other);
		return basic_path(temp);
	}

	basic_path operator/(const basic_path& other)
	{
		return operator/(other.to_portable_string());
	}

	basic_path& operator/=(const string_t& other)
	{
		basic_path newPath = operator/(other);
		swap(newPath);
		return *this;
	}

	basic_path& operator/=(const basic_path& other)
	{
		basic_path newPath = operator/(other);
		swap(newPath);
		return *this;
	}

	basic_path& append(const string_t& other)
	{
		return (*this)/= other;
	}

	basic_path& append(const basic_path& other)
	{
		return (*this)/= other;
	}

	bool is_relative() const
	{
		return relative;
	}

	bool is_drive_specified() const
	{
		return drive_specified;
	}

	bool is_unc_path() const
	{
		return unc_path;
	}

	bool exists() const
	{
		return internal::exists(get_path_string());
	}

	bool is_file() const
	{
		return internal::is_file(get_path_string());
	}

	bool is_directory() const
	{
		return internal::is_directory(get_path_string());
	}

	bool is_directory_empty() const
	{
		return internal::is_directory_empty(get_path_string());
	}

	const string_t to_portable_string() const
	{
		return get_path_string();
	}

	string_t to_win32_string() const
	{
		string_t					win23_str	= get_path_string();
		typename string_t::iterator it			= win23_str.begin();
		typename string_t::iterator itEnd		= win23_str.end();
		for (; it!=itEnd; ++it)
		{
			if (*it == '/')
			{
				*it = '\\';
			}
		}
		return win23_str;
	}

	basic_path full_path() const
	{
		string_t temp_path_string;

		#ifdef FS_WINDOWS_
			if (drive_specified & relative)
			{
				temp_path_string = internal::cur_drive_path(path_elems[0]);
				size_t elements = path_elems.size();
				if (elements > 0)
				{
					size_t lastIndex = temp_path_string.length() - 1;
					if ((temp_path_string[lastIndex] != '/') &&
						(temp_path_string[lastIndex] != '\\'))
					{
						temp_path_string.push_back('/');
					}

					temp_path_string.append(path_elems[1]);
					for (size_t elem=2; elem<elements; ++elem)
					{
						temp_path_string.push_back('/');
						temp_path_string.append(path_elems[elem]);
					}
				}
			}
			else
		#endif //#ifdef FS_WINDOWS_
			{
				temp_path_string = to_portable_string();
			}
		return basic_path(internal::full_pathname(temp_path_string));
	}

	basic_path from(const basic_path& other) const
	{
		basic_path thisFullPath = full_path();
		basic_path otherFullPath = other.full_path();

		if (!internal::compare_path_element(thisFullPath.path_elems[0], otherFullPath.path_elems[0]))
		{
			//we have paths with different roots.  Return the other full path.
			return thisFullPath;
		}
		// We have similar paths, we can reach one path from the other.
		size_t thisPathElems		= thisFullPath.path_elems.size();
		size_t otherPathElems		= otherFullPath.path_elems.size();
		size_t maxSimilarElems		= (thisPathElems < otherPathElems) ? thisPathElems : otherPathElems;
		size_t differentElemIndex	= maxSimilarElems+1;
		for (size_t elem=1; elem<maxSimilarElems; ++elem)
		{
			if (!internal::compare_path_element(thisFullPath.path_elems[elem], otherFullPath.path_elems[elem]))
			{
				differentElemIndex = elem;
				break;
			}
		}

		string_t newPathString;
		for (size_t elem=differentElemIndex; elem<otherPathElems; ++elem)
		{
			newPathString.push_back('.');
			newPathString.push_back('.');
			newPathString.push_back('/');
		}
		for (size_t elem=differentElemIndex; elem<thisPathElems; ++elem)
		{
			newPathString.append(thisFullPath.path_elems[elem]);
			newPathString.push_back('/');
		}
		//newPathString.erase(newPathString.end()-1);
		return basic_path(newPathString);
	}

	basic_path from(const string_t& other) const
	{
		return from(basic_path(other));
	}

	basic_path to(const basic_path& other) const
	{
		return other.from(*this);
	}

	basic_path to(const string_t& other) const
	{
		return basic_path(other).from(*this);
	}

	basic_path apply_variables(const std::map<string_t, string_t>& varmap) const
	{
		typename std::map<string_t, string_t>::const_iterator it;

		size_t		elemCount = path_elems.size();
		size_t		elem = 0;
		string_t	newPathString;
		string_t	varId;

		if (!relative)
		{
			newPathString = path_elems[0];
		}
		else
		{
			if (get_variable_id(path_elems[0], varId) &&
				((it = varmap.find(varId)) != varmap.end()))
			{
				newPathString = it->second;
				char_t lastChar = *(newPathString.end()-1);
				if ((lastChar != '/') && (lastChar != '\\'))
				{
					newPathString.push_back('/');
				}
			}
			else
			{
				newPathString = path_elems[0];
				newPathString.push_back('/');
			}
		}

		for (elem=1; elem<elemCount; ++elem)
		{
			if (get_variable_id(path_elems[elem], varId) &&
				((it = varmap.find(varId)) != varmap.end()))
			{
				newPathString.append(it->second);
				char_t lastChar = *(newPathString.end()-1);
				if ((lastChar != '/') && (lastChar != '\\'))
				{
					newPathString.push_back('/');
				}
			}
			else
			{
				newPathString.append(path_elems[elem]);
				newPathString.push_back('/');
			}
		}

		return basic_path(newPathString);
	}

	bool operator==(const basic_path& other) const
	{
		size_t elems = path_elems.size();
		if (elems != other.path_elems.size())
		{
			return false;
		}
		for (size_t elem=0; elem<elems; ++elem)
		{
			if (!internal::compare_path_element(path_elems[elem], other.path_elems[elem]))
			{
				return false;
			}
		}
		return true;
	}

	bool operator!=(const basic_path& other) const
	{
		return !operator==(other);
	}

	string_t filename() const
	{
		return *(path_elems.end()-1);
	}

	string_t stem() const
	{
		string_t temp = *(path_elems.end()-1);
		return temp.substr(0, temp.find_last_of('.'));
	}

	string_t extension() const
	{
		string_t temp = *(path_elems.end()-1);
		return temp.substr(temp.find_last_of('.')+1);
	}

	void directory_get_files(std::vector<string_t>& results)
	{
		if (!this->is_directory())
		{
			throw filesystem_error("Specified path is not a directory.", __FILE__, __LINE__, "", "");
		}
		internal::dir_get_files(to_portable_string(), string_t(), results);
	}

	void directory_get_files(const string_t& pattern, std::vector<string_t>& results)
	{
		if (!this->is_directory())
		{
			throw filesystem_error("Specified path is not a directory.", __FILE__, __LINE__, "", "");
		}
		internal::dir_get_files(to_portable_string(), pattern, results);
	}

	void directory_get_files(const string_t& pattern, std::vector<basic_path>& results)
	{
		basic_path				fullpath = full_path(); 
		std::vector<string_t>	str_results;

		fullpath.directory_get_files(pattern, str_results);
		results.clear();

		std::vector<string_t>::iterator it		= str_results.begin();
		std::vector<string_t>::iterator itEnd	= str_results.end();
		for (; it!=itEnd; ++it)
		{
			results.push_back(fullpath / *it);
		}
	}

	void directory_get_files(std::vector<basic_path>& results)
	{
		directory_get_files(string_t(), results);
	}

	void directory_get_subdirs(std::vector<string_t>& results)
	{
		if (!this->is_directory())
		{
			throw filesystem_error("Specified path is not a directory.", __FILE__, __LINE__, "", "");
		}
		internal::dir_get_subdirs(to_portable_string(), string_t(), results);
	}

	void directory_get_subdirs(const string_t& pattern, std::vector<string_t>& results)
	{
		if (!this->is_directory())
		{
			throw filesystem_error("Specified path is not a directory.", __FILE__, __LINE__, "", "");
		}
		internal::dir_get_subdirs(to_portable_string(), pattern, results);
	}

	void directory_get_subdirs(const string_t& pattern, std::vector<basic_path>& results)
	{
		basic_path				fullpath = full_path(); 
		std::vector<string_t>	str_results;

		fullpath.directory_get_subdirs(pattern, str_results);
		results.clear();

		std::vector<string_t>::iterator it		= str_results.begin();
		std::vector<string_t>::iterator itEnd	= str_results.end();
		for (; it!=itEnd; ++it)
		{
			results.push_back(fullpath / *it);
		}
	}

	void directory_get_subdirs(std::vector<basic_path>& results)
	{
		directory_get_subdirs(string_t(), results);
	}

	void directory_scan_subdirs_for_files(const string_t& pattern, std::vector<basic_path>& results)
	{
		results.clear();
		directory_scan_subdirs_for_files_helper(pattern, results);
	}

	void directory_scan_subdirs_for_files(std::vector<basic_path>& results)
	{
		results.clear();
		directory_scan_subdirs_for_files_helper(string_t(), results);
	}

#ifdef FS_WINDOWS_
	bool is_junction_point() const
	{
		return internal::is_junction_point(get_path_string());
	}
#endif //#ifdef FS_WINDOWS_
};
#endif // REGION: class basic_path

#if 1 // REGION: free filesystem functions

template <class T>
void create_directory(const basic_path<T>& dir)
{
	internal::create_directory(dir.to_portable_string());
}

template <class T>
void remove_directory(const basic_path<T>& dir)
{
	internal::remove_directory(dir.to_portable_string());
}

template <class T>
void remove_file(const basic_path<T>& dir)
{
	internal::remove_file(dir.to_portable_string());
}

template <class T>
void move(const basic_path<T>& oldpath, const basic_path<T>& newpath)
{
	internal::move(oldpath.to_portable_string(), newpath.to_portable_string());
}

template <class T>
void create_hard_link(const basic_path<T>& link, const basic_path<T>& source)
{
	internal::create_hard_link(link.to_portable_string(), source.to_portable_string());
}

template <class T>
std::fstream open_fstream(const basic_path<T>& filename, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
{
	return std::fstream(filename.to_portable_string().c_str(), mode);
}

template <class T>
std::fstream open_fstream(const basic_path<T>& filename, std::ios_base::openmode mode, int protection)
{
	return std::fstream(filename.to_portable_string().c_str(), mode, protection);
}

template <class T>
std::wfstream open_wfstream(const basic_path<T>& filename, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
{
	return std::wfstream(filename.to_portable_string().c_str(), mode);
}

template <class T>
std::wfstream open_wfstream(const basic_path<T>& filename, std::ios_base::openmode mode, int protection)
{
	return std::wfstream(filename.to_portable_string().c_str(), mode, protection);
}

template <class T>
std::ifstream open_ifstream(const basic_path<T>& filename, std::ios_base::openmode mode = std::ios_base::in)
{
	return std::ifstream(filename.to_portable_string().c_str(), mode);
}

template <class T>
std::ifstream open_ifstream(const basic_path<T>& filename, std::ios_base::openmode mode, int protection)
{
	return std::ifstream(filename.to_portable_string().c_str(), mode, protection);
}

template <class T>
std::wifstream open_wifstream(const basic_path<T>& filename, std::ios_base::openmode mode = std::ios_base::in)
{
	return std::wifstream(filename.to_portable_string().c_str(), mode);
}

template <class T>
std::wifstream open_wifstream(const basic_path<T>& filename, std::ios_base::openmode mode, int protection)
{
	return std::wifstream(filename.to_portable_string().c_str(), mode, protection);
}

template <class T>
std::ofstream open_ofstream(const basic_path<T>& filename, std::ios_base::openmode mode = std::ios_base::out)
{
	return std::ofstream(filename.to_portable_string().c_str(), mode);
}

template <class T>
std::ofstream open_ofstream(const basic_path<T>& filename, std::ios_base::openmode mode, int protection)
{
	return std::ofstream(filename.to_portable_string().c_str(), mode, protection);
}

template <class T>
std::wofstream open_wofstream(const basic_path<T>& filename, std::ios_base::openmode mode = std::ios_base::out)
{
	return std::wofstream(filename.to_portable_string().c_str(), mode);
}

template <class T>
std::wofstream open_wofstream(const basic_path<T>& filename, std::ios_base::openmode mode, int protection)
{
	return std::wofstream(filename.to_portable_string().c_str(), mode, protection);
}

#ifdef FS_WINDOWS_

template <class T>
void create_junction_point(const basic_path<T>& link, const basic_path<T>& source)
{
	if (source.is_directory())
	{
		if (!link.exists())
		{
			create_directory(link);
		}
		if (link.is_directory())
		{
			if (link.is_junction_point() || link.is_directory_empty())
			{
				if (!internal::create_junction_point(link.to_portable_string(), source.full_path().to_portable_string()))
				{
					throw filesystem_error("Failed to create junction point", __FILE__, __LINE__, "", "");
				}
				return;
			}
			else
			{
				throw filesystem_error("Directory exists, but is not empty", __FILE__, __LINE__, "", "");
			}
		}
		else
		{
			if (link.is_file())
			{
				throw filesystem_error("Link is a file", __FILE__, __LINE__, "", "");
			}
			throw filesystem_error("Link is not a directory", __FILE__, __LINE__, "", "");
		}
	}
	throw filesystem_error("Source is not a directory", __FILE__, __LINE__, "", "");
}
#endif //#ifdef FS_WINDOWS_

#endif // REGION: free filesystem functions

typedef basic_path<char>	path;
typedef basic_path<wchar_t>	wpath;

} //namespace filesystem

#endif //#ifndef _FILESYSTEM_H__
