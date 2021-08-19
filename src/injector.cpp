#include "injector.h"

#define ACCESS_RIGHTS (PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ)

// This is a really basic LoadLibrary injector:
// 	1. Open a handle to the process.
// 	2. Allocate memory in the process and write the full path to the DLL to it.
// 	3. Create a thread in the process; start address is LoadLibrary and set the
// 	   argument to be the region of memory we just wrote to.

DWORD find_process(const std::string &processName) {
	PROCESSENTRY32 processInfo;
	processInfo.dwSize = sizeof(PROCESSENTRY32);

	HANDLE processesSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (!processesSnapshot || processesSnapshot == INVALID_HANDLE_VALUE) {
		throw std::runtime_error("failed to get process list");
	}

	Process32First(processesSnapshot, &processInfo);
	if (processName == processInfo.szExeFile) {
		CloseHandle(processesSnapshot);
		return processInfo.th32ProcessID;
	}

	while (Process32Next(processesSnapshot, &processInfo)) {
		if (processName == processInfo.szExeFile) {
			CloseHandle(processesSnapshot);
			return processInfo.th32ProcessID;
		}
	}

	CloseHandle(processesSnapshot);
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("%s\n", "usage: injector some.dll process.exe");
	}

	LPVOID remote_mem;
	HANDLE remote_thread;
	HANDLE process_handle;

	auto cleanup = [&] {
		if (remote_mem)
			VirtualFreeEx(process_handle, remote_mem, 0, MEM_RELEASE);

		if (remote_thread)
			CloseHandle(remote_thread);

		if (process_handle)
			CloseHandle(process_handle);
	};

	try {
		auto file_path = std::filesystem::canonical(argv[1]).wstring();

		if (!std::filesystem::exists(file_path))
			throw std::runtime_error("failed to find DLL");

		printf("[*] %s '%ls' %s '%s'\n", "injecting DLL", file_path.c_str(), "into",
			argv[2]);

		auto process_id = find_process(argv[2]);
		if (!process_id)
			throw std::runtime_error("failed to find process");

		process_handle = OpenProcess(PROCESS_VM_OPERATION |
			PROCESS_VM_WRITE | PROCESS_CREATE_THREAD, FALSE, process_id);
		if (!process_handle)
			throw std::runtime_error("failed to open process handle");

		auto remote_mem_size = file_path.size() * 2;
		remote_mem = VirtualAllocEx(process_handle, nullptr, remote_mem_size,
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (!remote_mem)
			throw std::runtime_error("failed to allocate remote memory");

		SIZE_T bytes_written;
		WriteProcessMemory(process_handle, remote_mem, file_path.c_str(),
			remote_mem_size, &bytes_written);
		if (bytes_written != remote_mem_size)
			throw std::runtime_error("failed to write to remote memory");

		remote_thread = CreateRemoteThread(process_handle, nullptr, NULL,
			reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryW), remote_mem,
			NULL, nullptr);
		if (!remote_thread)
			throw std::runtime_error("failed to open remote thread");

		WaitForSingleObject(remote_thread, INFINITE);

		printf("%s\n", "[+] injected successfully");

		cleanup();
	} catch (const std::runtime_error &err) {
		cleanup();

		printf("[-] %s (%lu)\n", err.what(), GetLastError());

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
