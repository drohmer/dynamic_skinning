#pragma once

namespace cgp_test
{
	// Tests on the path helper (module 21, GL-free part only).
	// Exercises split_executable_path, which extracts the directory and executable
	//  name from argv[0] and strips an optional extension:
	//  - correct substring length for the executable name
	//  - extension stripping at the FIRST dot (find_first_of, not a sign comparison)
	//  - the no-slash (local directory) fall-back case
	void test_path();
}
