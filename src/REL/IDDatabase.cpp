#include "REL/ID.h"

#include "REL/Module.h"

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace REL
{
	namespace
	{
		std::wstring s_dbDir;
		bool         s_dbDirSet = false;

		bool read_exact(HANDLE a_file, void* a_buffer, DWORD a_size)
		{
			auto* cursor = static_cast<char*>(a_buffer);
			while (a_size > 0) {
				DWORD read = 0;
				if (!::ReadFile(a_file, cursor, a_size, &read, nullptr) || read == 0) {
					return false;
				}
				cursor += read;
				a_size -= read;
			}
			return true;
		}

		[[noreturn]] void fail(const std::string& a_msg)
		{
			::MessageBoxA(nullptr, a_msg.c_str(), "kcd_re REL::IDDatabase", MB_OK | MB_ICONERROR);
			::TerminateProcess(::GetCurrentProcess(), 1);
		}

		std::string sha256(const std::vector<std::uint8_t>& a_data)
		{
			BCRYPT_ALG_HANDLE algorithm = nullptr;
			if (BCryptOpenAlgorithmProvider(
					&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
				fail("Could not initialize SHA-256 for the address library.");
			}
			DWORD objectSize = 0;
			DWORD copied = 0;
			if (BCryptGetProperty(
					algorithm, BCRYPT_OBJECT_LENGTH,
					reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize),
					&copied, 0) < 0) {
				BCryptCloseAlgorithmProvider(algorithm, 0);
				fail("Could not query SHA-256 state size.");
			}
			std::vector<std::uint8_t> object(objectSize);
			BCRYPT_HASH_HANDLE hash = nullptr;
			if (BCryptCreateHash(
					algorithm, &hash, object.data(), objectSize,
					nullptr, 0, 0) < 0
				|| BCryptHashData(
					hash, const_cast<PUCHAR>(a_data.data()),
					static_cast<ULONG>(a_data.size()), 0) < 0) {
				if (hash) BCryptDestroyHash(hash);
				BCryptCloseAlgorithmProvider(algorithm, 0);
				fail("Could not hash the loaded address library.");
			}
			std::array<std::uint8_t, 32> digest{};
			const auto status = BCryptFinishHash(
				hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
			BCryptDestroyHash(hash);
			BCryptCloseAlgorithmProvider(algorithm, 0);
			if (status < 0) {
				fail("Could not hash the loaded address library.");
			}
			constexpr char hex[] = "0123456789abcdef";
			std::string result(digest.size() * 2, '0');
			for (std::size_t index = 0; index < digest.size(); ++index) {
				result[index * 2] = hex[digest[index] >> 4];
				result[index * 2 + 1] = hex[digest[index] & 0x0F];
			}
			return result;
		}
	}

	void IDDatabase::SetDatabaseDirectory(std::wstring a_dir)
	{
		s_dbDir = std::move(a_dir);
		s_dbDirSet = true;
	}

	IDDatabase& IDDatabase::get()
	{
		static IDDatabase singleton;
		return singleton;
	}

	IDDatabase::IDDatabase()
	{
		load();
	}

	void IDDatabase::load()
	{
		auto&             mod = Module::get();
		const auto        dist = mod.distribution();
		const std::string build{ mod.build_code() };

		// Abstract ids are version-neutral: every distribution -- including Steam --
		// remaps through its own generated table (no privileged "identity" build).
		const char*   dn = nullptr;
		std::uint32_t expectDist = 0;
		switch (dist) {
		case Distribution::Steam: dn = "steam"; expectDist = 1; break;
		case Distribution::GOG:   dn = "gog";   expectDist = 2; break;
		case Distribution::Epic:  dn = "epic";  expectDist = 3; break;
		default:
			fail("Could not detect the game distribution (Steam / GOG / Epic) from "
				 "WHGame.dll.\nKCSE cannot select an address library.");
		}

		std::string key = build.empty() ? std::string{ mod.release() } : build;  // build_code, else release
		if (key.empty()) {
			fail("Could not determine the game build/version.\nKCSE cannot select an address library.");
		}

		const std::wstring dir = s_dbDirSet ? s_dbDir : (mod.game_root() + L"\\KCSE\\addresslib");
		std::string  nameA = std::string("kcd_addresslib_") + dn + "_" + key + ".bin";
		std::wstring full = dir + L"\\" + std::wstring(nameA.begin(), nameA.end());

		const auto file = ::CreateFileW(
			full.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE) {
			std::string p(full.size(), '\0');
			for (std::size_t i = 0; i < full.size(); ++i) {
				p[i] = static_cast<char>(full[i]);  // ASCII path display in the error box
			}
			fail("Address library not found:\n" + p +
				 "\n\nThis KCSE build has no address mapping for this game version/distribution.");
		}

		LARGE_INTEGER fileSize{};
		if (!::GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < 16
			|| fileSize.QuadPart > MAXDWORD) {
			::CloseHandle(file);
			fail("Address library file size is invalid.");
		}
		std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize.QuadPart));
		if (!read_exact(file, bytes.data(), static_cast<DWORD>(bytes.size()))) {
			::CloseHandle(file);
			fail("Address library is truncated.");
		}
		::CloseHandle(file);

		const auto* cursor = bytes.data();
		char magic[4]{};
		std::memcpy(magic, cursor, sizeof(magic));
		cursor += sizeof(magic);
		if (std::memcmp(magic, "KASL", 4) != 0) {
			fail("Address library has a bad magic header.");
		}

		std::uint32_t fmt = 0, fdist = 0, count = 0;
		std::memcpy(&fmt, cursor, sizeof(fmt)); cursor += sizeof(fmt);
		std::memcpy(&fdist, cursor, sizeof(fdist)); cursor += sizeof(fdist);
		std::memcpy(&count, cursor, sizeof(count)); cursor += sizeof(count);
		if (count == 0 || count > MAXDWORD / sizeof(mapping_t)
			|| bytes.size() != 16 + static_cast<std::size_t>(count) * sizeof(mapping_t)) {
			fail("Address library header is invalid.");
		}
		if (fdist != expectDist) {
			fail("Address library is for a different distribution than the running game.");
		}

		_id2offset.resize(count);
		std::memcpy(_id2offset.data(), cursor, count * sizeof(mapping_t));
		_metadata.distribution = dist;
		_metadata.format_version = fmt;
		_metadata.entry_count = count;
		_metadata.build_key = key;
		_metadata.sha256 = sha256(bytes);

		if (!std::is_sorted(_id2offset.begin(), _id2offset.end(),
				[](const mapping_t& a, const mapping_t& b) { return a.id < b.id; })) {
			std::sort(_id2offset.begin(), _id2offset.end(),
				[](const mapping_t& a, const mapping_t& b) { return a.id < b.id; });
		}
	}

	std::size_t IDDatabase::id2offset(std::uint64_t a_id) const
	{
		const mapping_t key{ static_cast<std::uint32_t>(a_id), 0 };
		const auto      it = std::lower_bound(
            _id2offset.begin(), _id2offset.end(), key,
            [](const mapping_t& a, const mapping_t& b) { return a.id < b.id; });
		if (it == _id2offset.end() || it->id != key.id) {
			char buf[160];
			std::snprintf(buf, sizeof(buf),
				"REL::ID %llu is not present in the address library for this version.\n"
				"This plugin is incompatible with the current game build.",
				static_cast<unsigned long long>(a_id));
			fail(buf);
		}
		return static_cast<std::size_t>(it->offset);
	}
}
