#include "REL/Module.h"

#include <Windows.h>

#include <cstring>
#include <string>
#include <string_view>

// libKCD2 port note (vs libKCD1):
//   * build_code(): KCD2 replaced whdlversions.txt ("Build = 404-504czj4") with a
//     structured whdlversions.json. See below.
//   * resolve_game_root(): UNCHANGED. On KCD2 Steam the exe lives at
//     ...\KingdomComeDeliverance2\Bin\Win64MasterMasterSteamPGO\KingdomCome.exe,
//     so stripping 3 components (exe, <cfg dir>, Bin) still lands on the game root.
//     (The KCSE dinput8 proxy must be dropped in that exe/<cfg> dir, NOT Bin\Win64Shared,
//     which is only a runtime search path the game builds for NGX/FSR2 DLLs.)
//   * release(): UNCHANGED. KCD2 system.cfg still carries wh_sys_version = "1.5.6".
//   * detect_distribution(): UNCHANGED. steam_api64.dll / Galaxy64.dll /
//     EOSSDK-Win64-Shipping.dll markers are still present in .rdata.

namespace REL
{
	namespace
	{
		bool read_file(const std::wstring& a_path, std::string& a_contents)
		{
			const auto file = ::CreateFileW(
				a_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE) {
				return false;
			}

			LARGE_INTEGER size{};
			if (!::GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > MAXDWORD) {
				::CloseHandle(file);
				return false;
			}

			a_contents.resize(static_cast<std::size_t>(size.QuadPart));
			auto* cursor = a_contents.data();
			auto remaining = static_cast<DWORD>(a_contents.size());
			bool ok = true;
			while (remaining > 0) {
				DWORD read = 0;
				if (!::ReadFile(file, cursor, remaining, &read, nullptr) || read == 0) {
					ok = false;
					break;
				}
				cursor += read;
				remaining -= read;
			}
			::CloseHandle(file);
			return ok;
		}

		// Minimal, dependency-free scraping for the fixed Warhorse-generated
		// whdlversions.json layout. Not a general JSON parser: find an object
		// anchor (e.g. "\"Assembly\""), then the first `key` after it, then that
		// key's value.
		std::string json_string_after(const std::string& j, const char* a_anchor, const char* a_key)
		{
			const auto a = j.find(a_anchor);
			if (a == std::string::npos) return {};
			const auto k = j.find(a_key, a);
			if (k == std::string::npos) return {};
			const auto colon = j.find(':', k + std::strlen(a_key));
			if (colon == std::string::npos) return {};
			const auto q1 = j.find('"', colon);
			if (q1 == std::string::npos) return {};
			const auto q2 = j.find('"', q1 + 1);
			if (q2 == std::string::npos) return {};
			return j.substr(q1 + 1, q2 - q1 - 1);
		}

		std::string json_number_after(const std::string& j, const char* a_anchor, const char* a_key)
		{
			const auto a = j.find(a_anchor);
			if (a == std::string::npos) return {};
			const auto k = j.find(a_key, a);
			if (k == std::string::npos) return {};
			const auto colon = j.find(':', k + std::strlen(a_key));
			if (colon == std::string::npos) return {};
			auto p = colon + 1;
			while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) ++p;
			const auto s = p;
			while (p < j.size() && j[p] >= '0' && j[p] <= '9') ++p;
			return j.substr(s, p - s);
		}
	}

	std::string_view to_string(Distribution a_dist) noexcept
	{
		switch (a_dist) {
		case Distribution::Steam: return "Steam";
		case Distribution::GOG:   return "GOG";
		case Distribution::Epic:  return "Epic";
		default:                  return "Unknown";
		}
	}

	Module& Module::get()
	{
		static Module singleton;
		return singleton;
	}

	Module::Module()
	{
		_base = reinterpret_cast<std::uintptr_t>(::GetModuleHandleW(FILENAME.data()));
		if (!_base) {
			::MessageBoxW(nullptr,
				L"REL::Module: WHGame.dll is not loaded into the process.",
				L"kcd2_re", MB_OK | MB_ICONERROR);
			::TerminateProcess(::GetCurrentProcess(), 1);
		}
		resolve_game_root();
		load_pe();
		detect_distribution();
	}

	void Module::resolve_game_root()
	{
		wchar_t buf[MAX_PATH]{};
		const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
		std::wstring path(buf, n ? n : 0);
		// KCD2 Steam: ...\KingdomComeDeliverance2\Bin\Win64MasterMasterSteamPGO\<exe>
		//   -> strip exe, <cfg dir>, Bin  ->  ...\KingdomComeDeliverance2
		for (int i = 0; i < 3; ++i) {
			const auto pos = path.find_last_of(L"\\/");
			if (pos == std::wstring::npos) {
				break;
			}
			path.resize(pos);
		}
		_gameRoot = path;
	}

	void Module::load_pe()
	{
		const auto b = reinterpret_cast<const std::uint8_t*>(_base);
		const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(b);
		const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(b + dos->e_lfanew);
		_timestamp = nt->FileHeader.TimeDateStamp;

		const auto sections = IMAGE_FIRST_SECTION(nt);
		const auto map = [&](const char* a_name, Segment::Name a_idx) {
			for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
				if (std::strncmp(reinterpret_cast<const char*>(sections[i].Name), a_name, 8) == 0) {
					_segments[a_idx] = Segment(
						_base,
						_base + sections[i].VirtualAddress,
						sections[i].Misc.VirtualSize);
					return;
				}
			}
		};
		map(".text", Segment::textx);
		map(".idata", Segment::idata);
		map(".rdata", Segment::rdata);
		map(".data", Segment::data);
		map(".pdata", Segment::pdata);
	}

	void Module::detect_distribution()
	{
		const auto seg = _segments[Segment::rdata];
		if (!seg.address() || !seg.size()) {
			_distribution = Distribution::Unknown;
			return;
		}
		const std::string_view hay(reinterpret_cast<const char*>(seg.address()), seg.size());
		if (hay.find("steam_api64.dll") != std::string_view::npos) {
			_distribution = Distribution::Steam;
		} else if (hay.find("Galaxy64.dll") != std::string_view::npos) {
			_distribution = Distribution::GOG;
		} else if (hay.find("EOSSDK-Win64-Shipping.dll") != std::string_view::npos) {
			_distribution = Distribution::Epic;
		} else {
			_distribution = Distribution::Unknown;
		}
	}

	std::string_view Module::release()
	{
		if (!_releaseQueried) {
			_releaseQueried = true;
			std::string config;
			if (read_file(_gameRoot + L"\\system.cfg", config)) {
				const auto key = config.find("wh_sys_version");  // KCD2: wh_sys_version = "1.5.6"
				const auto q1 = key == std::string::npos ? std::string::npos : config.find('"', key);
				const auto q2 = q1 == std::string::npos ? std::string::npos : config.find('"', q1 + 1);
				if (q1 != std::string::npos && q2 != std::string::npos) {
					_release = config.substr(q1 + 1, q2 - q1 - 1);
				}
			}
		}
		return _release;
	}

	std::string_view Module::build_code()
	{
		if (!_buildQueried) {
			_buildQueried = true;
			// KCD2 replaced KCD1's whdlversions.txt ("Build = 404-504czj4") with
			// whdlversions.json:
			//   {"Assembly":{"Id":15693,"DateTestedData":...},
			//    "Preset":{...,"Branch":{"Id":9,"Name":"release_1_5",...}}, ...}
			// We key the address library on <Branch.Name>-<Assembly.Id>
			// (e.g. "release_1_5-15693") -- Assembly.Id uniquely identifies one
			// shipped binary build; Branch.Name makes the .bin filename readable.
			std::string j;
			if (read_file(_gameRoot + L"\\whdlversions.json", j) && !j.empty()) {
				const std::string branch = json_string_after(j, "\"Branch\"", "\"Name\"");
				const std::string id     = json_number_after(j, "\"Assembly\"", "\"Id\"");
				if (!branch.empty() && !id.empty()) {
					_buildCode = branch + "-" + id;
				} else if (!id.empty()) {
					_buildCode = id;
				} else if (!branch.empty()) {
					_buildCode = branch;
				}
			}
		}
		return _buildCode;
	}
}
