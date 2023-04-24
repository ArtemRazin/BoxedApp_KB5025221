#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <Windows.h>
#include <iostream>
#include "BoxedAppSDK.h"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/archive/tmpdir.hpp>
#include "TestVirtualFiles.h"

#define DEF_BOXEDAPPSDK_OPTION__ALL_CHANGES_ARE_VIRTUAL                (1) // default: 0 (FALSE)
#define DEF_BOXEDAPPSDK_OPTION__EMBED_BOXEDAPP_IN_CHILD_PROCESSES      (2) // default: 0 (FALSE, don't enable BoxedApp to a new process by default)
#define DEF_BOXEDAPPSDK_OPTION__ENABLE_VIRTUAL_FILE_SYSTEM             (3) // default: 1 (TRUE)
#define DEF_BOXEDAPPSDK_OPTION__ENABLE_VIRTUAL_REGISTRY                (5) // default: 1 (TRUE)

BOOL g_virtInitialized = FALSE;

bool WriteTextToFile(const std::wstring &text, const std::wstring &path, bool overwrite);
bool CopyToVirtualFile(const std::wstring &src_file, const std::wstring &target_file);

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
	std::wstring exp_src_file;
	std::wstring exp_target_file;

	try
	{
		boost::filesystem::path dir(exp_target_file.c_str());
		if (!boost::filesystem::exists(dir.parent_path()))
		{
			boost::filesystem::create_directories(dir.parent_path());
		}
		boost::filesystem::copy_file(src_file, target_file, boost::filesystem::copy_option::overwrite_if_exists);
		success = true;
	}
	catch (const boost::filesystem::filesystem_error& e)
	{
		std::wcout << e.what() << std::endl;
	}

	std::wcout << L"Copy virtual file successful: " << target_file << std::endl;

	assert(IsFileExists(std::wstring(target_file)));

	return success;
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