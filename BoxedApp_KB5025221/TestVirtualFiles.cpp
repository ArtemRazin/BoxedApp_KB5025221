#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <Windows.h>
#include <iostream>
#include "BoxedAppSDK.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/archive/tmpdir.hpp>
#include "TestVirtualFiles.h"
#include <fileapi.h>

#define DEF_BOXEDAPPSDK_OPTION__ALL_CHANGES_ARE_VIRTUAL                (1) // default: 0 (FALSE)
#define DEF_BOXEDAPPSDK_OPTION__EMBED_BOXEDAPP_IN_CHILD_PROCESSES      (2) // default: 0 (FALSE, don't enable BoxedApp to a new process by default)
#define DEF_BOXEDAPPSDK_OPTION__ENABLE_VIRTUAL_FILE_SYSTEM             (3) // default: 1 (TRUE)
#define DEF_BOXEDAPPSDK_OPTION__ENABLE_VIRTUAL_REGISTRY                (5) // default: 1 (TRUE)

BOOL g_virtInitialized = FALSE;

bool WriteTextToFile(const std::wstring &text, const std::wstring &path, bool overwrite);
bool CopyToVirtualFile(const std::wstring &src_file, const std::wstring &target_file);
bool CreateDirectoryRecursively(const std::wstring &directory)

template<class T>
bool IsFileExists(const std::basic_string<T> &filePath);

std::wstring ReadTextFile(const std::wstring &path);

BOOL Initialize(bool virtFilesPresent, bool virtRegsPresent)
{
	g_virtInitialized = BoxedAppSDK_Init();

	if (g_virtInitialized)
	{
		BoxedAppSDK_EnableOption(DEF_BOXEDAPPSDK_OPTION__EMBED_BOXEDAPP_IN_CHILD_PROCESSES, TRUE);
		BoxedAppSDK_EnableOption(DEF_BOXEDAPPSDK_OPTION__ENABLE_VIRTUAL_FILE_SYSTEM, virtFilesPresent);
		BoxedAppSDK_EnableOption(DEF_BOXEDAPPSDK_OPTION__ENABLE_VIRTUAL_REGISTRY, virtRegsPresent);
	}

	return g_virtInitialized;
}

BOOL TestVirtualFiles(bool useBoxedApp)
{
	std::wstring text(L"TEST");

	std::wstring tmp_path = boost::lexical_cast<std::wstring>(boost::archive::tmpdir());

	std::wstring file_a = tmp_path + L"\\file_a.txt";
	assert(file_a != L"");

	std::wstring file_b = tmp_path + L"\\file_b.txt";
	assert(file_b != L"");

	std::wstring file_c = tmp_path + L"\\file_c.txt";
	assert(file_c != L"");

	if (!useBoxedApp || Initialize(true, true))
	{

		if (WriteTextToFile(text, file_a, true))
		{
			if (useBoxedApp)
				BoxedAppSDK_EnableOption(DEF_BOXEDAPPSDK_OPTION__ALL_CHANGES_ARE_VIRTUAL, TRUE);

			assert(CopyToVirtualFile(file_a, file_b));

			if (useBoxedApp)
				BoxedAppSDK_EnableOption(DEF_BOXEDAPPSDK_OPTION__ALL_CHANGES_ARE_VIRTUAL, FALSE);

			assert(IsFileExists(file_b));

			CopyFile(file_b.c_str(), file_c.c_str(), FALSE);
		}

		if (useBoxedApp)
			BoxedAppSDK_EnableOption(DEF_BOXEDAPPSDK_OPTION__ALL_CHANGES_ARE_VIRTUAL, FALSE);

		if (useBoxedApp)
			BoxedAppSDK_Exit();

		// confirm virtual file died with environment
		assert(!IsFileExists(file_b));

		// confirm c still exists
		assert(IsFileExists(file_c));

	}

	// confirm contents of C match A and return 0 on success
	return ReadTextFile(file_c) == text ? 0 : 1;
}

bool WriteTextToFile(const std::wstring &text, const std::wstring &path, bool overwrite)
{
	int open_mode = overwrite ?
		std::wofstream::trunc :
		std::wofstream::app;

	std::wofstream file;

	file.exceptions(std::wofstream::failbit | std::wofstream::badbit);

	try
	{
		file.open(path, std::wofstream::out | open_mode);
		file << text;
		file.close();
	}
	catch (std::wofstream::failure e)
	{
		std::wcout << e.what() << std::endl;
	}

	return !file.fail();
}

bool CopyToVirtualFile(const std::wstring &src_file, const std::wstring &target_file)
{
	bool success = false;

	try
	{
		std::wstring parentDir;
		wchar_t buffer[MAX_PATH];
		GetFullPathName(src_file.c_str(), MAX_PATH, buffer, nullptr);
		PathCchRemoveFileSpec(buffer, wcslen(buffer)); // Gets parent directory for filepath
		parentDir = std::wstring(buffer);

		if (!CreateDirectoryRecursively(parentDir))
		{
			std::wcout << "Directory already exists: " << parentDir << std::endl;
		}

		if (!CopyFileW(src_file.c_str(), target_file.c_str(), TRUE))
		{
			std::wcout << "File " << src_file << " was not copied to " << target_file << std::endl;
			success = false;
			goto Done;
		}
		success = true;
	}
	catch (const std::exception& e)
	{
		std::wcout << e.what() << std::endl;
	}

Done:
	std::wcout << L"Copy virtual file successful: " << target_file.c_str() << std::endl;

	return success;
}

bool CreateDirectoryRecursively(const std::wstring &directory) 
{
	static const std::wstring separators(L"\\/");

	// If the specified directory name doesn't exist, do our thing
	DWORD fileAttributes = ::GetFileAttributesW(directory.c_str());
	if (fileAttributes == INVALID_FILE_ATTRIBUTES) {

		// Recursively do it all again for the parent directory, if any
		std::size_t slashIndex = directory.find_last_of(separators);
		if (slashIndex != std::wstring::npos) {
			CreateDirectoryRecursively(directory.substr(0, slashIndex));
		}

		// Create the last directory on the path (the recursive calls will have taken
		// care of the parent directories by now)
		BOOL result = ::CreateDirectoryW(directory.c_str(), NULL);
		if (result == FALSE) {
			throw std::runtime_error("Could not create directory");
		}
		return TRUE;
	}
	else { // Specified directory name already exists as a file or directory

		bool isDirectoryOrJunction =
			((fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ||
			((fileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0);

		if (!isDirectoryOrJunction) {
			throw std::runtime_error(
				"Could not create directory because a file with the same name exists"
			);
		}
		return FALSE;
	}
}

template<class T>
bool IsFileExists(const std::basic_string<T> &filePath)
{
	boost::filesystem::path pathToFile(filePath);
	return boost::filesystem::is_regular_file(pathToFile);
}

std::wstring ReadTextFile(const std::wstring &path)
{
	std::wstringstream stream;
	std::wofstream file;

	file.exceptions(std::wofstream::failbit | std::wofstream::badbit);

	try
	{
		file.open(path, std::wofstream::in);

		if (file.is_open())
		{
			stream << file.rdbuf();

			file.close();
		}
	}
	catch (std::wofstream::failure e)
	{
		std::wcout << e.what() << std::endl;
	}

	return stream.str();
}