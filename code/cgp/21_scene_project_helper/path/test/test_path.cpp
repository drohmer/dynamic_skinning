#include "test_path.hpp"

#include "cgp/01_base/base.hpp"

#include <string>

namespace cgp
{
	// split_executable_path has external linkage but is not declared in path.hpp.
	//  Forward-declare it here so the test can exercise the extraction logic directly.
	void split_executable_path(std::string const& executable_path, std::string& executable_dir_out, std::string& executable_name_out);
}

using namespace cgp;

namespace cgp_test
{
	void test_path()
	{
		// ---------- extension stripping at the FIRST dot, no separator ----------
		// These inputs contain neither '/' nor '\\', so the directory split is a no-op
		//  on every platform and only the extension-stripping branch is exercised.
		{
			std::string dir, name;

			// No extension, no separator: name kept verbatim, empty directory.
			split_executable_path("project", dir, name);
			assert_cgp_no_msg(dir == "");
			assert_cgp_no_msg(name == "project");

			// ".exe" stripped (name length > 4): cut at the first '.'.
			split_executable_path("renderer.exe", dir, name);
			assert_cgp_no_msg(dir == "");
			assert_cgp_no_msg(name == "renderer");

			// Several dots: must cut at the FIRST one (find_first_of), not the last.
			split_executable_path("my.app.exe", dir, name);
			assert_cgp_no_msg(dir == "");
			assert_cgp_no_msg(name == "my");

			// Short name (length <= 4): the dot is NOT stripped.
			split_executable_path("x.y", dir, name);
			assert_cgp_no_msg(dir == "");
			assert_cgp_no_msg(name == "x.y");
		}

#if !defined(_WIN32)
		// ---------- directory split with '/' separator (POSIX) ----------
		// Pins the executable-name substring length: it must be exactly the part after
		//  the last '/', not over- or under-counted.
		{
			std::string dir, name;

			split_executable_path("a/bc", dir, name);
			assert_cgp_no_msg(dir == "a/");
			assert_cgp_no_msg(name == "bc");

			split_executable_path("build/bin/project", dir, name);
			assert_cgp_no_msg(dir == "build/bin/");
			assert_cgp_no_msg(name == "project");

			// Directory split AND extension stripping together.
			split_executable_path("build/renderer.exe", dir, name);
			assert_cgp_no_msg(dir == "build/");
			assert_cgp_no_msg(name == "renderer");
		}
#endif
	}
}
