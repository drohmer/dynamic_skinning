#pragma once

namespace cgp_test
{
	// Tests on the simple OBJ loader/saver (module 20, GL-free part only):
	//  - save/load round-trip for position-only, and position+uv+normal meshes
	//  - adaptive face format on save: "v//vn" when normals but no uv (no empty uv slot)
	//  - obj_read_connectivity rejects a face line with fewer than 3 indices
	//  - the loader rejects an out-of-range (negative) vertex index instead of reading OOB
	void test_obj();
}
