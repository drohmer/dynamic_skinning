#include "test_obj.hpp"

#include "cgp/01_base/base.hpp"
#include "../obj.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace cgp;

namespace
{
	// Write raw text to a file (used to forge OBJ fixtures on disk).
	void write_text_file(std::string const& filename, std::string const& content)
	{
		std::ofstream stream(filename, std::ofstream::out);
		assert_cgp_no_msg(stream.is_open());
		stream << content;
		stream.close();
	}

	// Read a whole file back into a string (used to inspect what the saver wrote).
	std::string read_text_file(std::string const& filename)
	{
		std::ifstream stream(filename);
		assert_cgp_no_msg(stream.is_open());
		std::stringstream buffer;
		buffer << stream.rdbuf();
		return buffer.str();
	}

	// A small tetrahedron whose faces reference vertices in increasing first-seen
	//  order (0,1,2,3) so the loader rebuilds the vertices in the original order.
	mesh make_tetrahedron()
	{
		mesh m;
		m.position = { {0,0,0}, {1,0,0}, {0,1,0}, {0,0,1} };
		m.connectivity = { {0,1,2}, {0,2,3}, {0,3,1}, {1,3,2} };
		return m;
	}
}

namespace cgp_test
{
	void test_obj()
	{
		// ---------- round-trip: position only ----------
		// No uv and no normal -> type "vertex", faces written as plain "f i j k".
		{
			mesh const m = make_tetrahedron();
			std::string const file = "cgp_test_obj_position.obj";
			mesh_save_file_obj(file, m);

			mesh const loaded = mesh_load_file_obj(file);
			std::remove(file.c_str());

			assert_cgp_no_msg(loaded.position.size() == m.position.size());
			for (int k = 0; k < m.position.size(); ++k)
				assert_cgp_no_msg(is_equal(loaded.position[k], m.position[k]));

			assert_cgp_no_msg(loaded.connectivity.size() == m.connectivity.size());
			for (int k = 0; k < m.connectivity.size(); ++k)
				assert_cgp_no_msg(is_equal(loaded.connectivity[k], m.connectivity[k]));
		}

		// ---------- round-trip: position + uv + normal ----------
		// uv.size()==normal.size()==position.size() -> faces written as "f a/a/a".
		// Each vertex i carries a distinct (position, uv, normal) so nothing is welded away.
		{
			mesh m = make_tetrahedron();
			m.uv = { {0.0f,0.0f}, {1.0f,0.0f}, {0.0f,1.0f}, {1.0f,1.0f} };
			m.normal = { {1,0,0}, {0,1,0}, {0,0,1}, {0.577f,0.577f,0.577f} };

			std::string const file = "cgp_test_obj_full.obj";
			mesh_save_file_obj(file, m);

			mesh const loaded = mesh_load_file_obj(file);
			std::remove(file.c_str());

			assert_cgp_no_msg(loaded.position.size() == m.position.size());
			assert_cgp_no_msg(loaded.uv.size() == m.uv.size());
			assert_cgp_no_msg(loaded.normal.size() == m.normal.size());
			for (int k = 0; k < m.position.size(); ++k) {
				assert_cgp_no_msg(is_equal(loaded.position[k], m.position[k]));
				assert_cgp_no_msg(is_equal(loaded.uv[k], m.uv[k]));
				assert_cgp_no_msg(is_equal(loaded.normal[k], m.normal[k]));
			}
		}

		// ---------- adaptive face format: normals but no uv ----------
		// has_normal && !has_uv must produce "f a//a" (empty uv slot), never "f a/a/a".
		// The former bug always wrote the v/vt/vn format and corrupted such meshes.
		{
			mesh m = make_tetrahedron();
			m.normal = { {1,0,0}, {0,1,0}, {0,0,1}, {0.577f,0.577f,0.577f} };
			// no uv on purpose

			std::string const file = "cgp_test_obj_normal_only.obj";
			mesh_save_file_obj(file, m);

			std::string const content = read_text_file(file);
			// A face line must use the "//" form and must not contain a single "/x/" uv slot.
			assert_cgp(content.find("//") != std::string::npos,
				"mesh_save_file_obj: normal-only mesh must use the v//vn face format");

			mesh const loaded = mesh_load_file_obj(file);
			std::remove(file.c_str());

			// Note: mesh_load_file_obj calls fill_empty_field(), so loaded.uv is
			//  populated with defaults; we only assert positions and normals here.
			assert_cgp_no_msg(loaded.position.size() == m.position.size());
			assert_cgp_no_msg(loaded.normal.size() == m.normal.size());
			for (int k = 0; k < m.normal.size(); ++k)
				assert_cgp_no_msg(is_equal(loaded.normal[k], m.normal[k]));
		}

		// ---------- obj_read_connectivity rejects a short face line ----------
		// "f 1 2" has only two indices: the empty-string guard must trigger.
		{
			std::string const file = "cgp_test_obj_shortface.obj";
			write_text_file(file, "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2\n");

			bool threw = false;
			try {
				loader::obj_read_connectivity(file);
			}
			catch (std::logic_error const&) {
				threw = true;
			}
			std::remove(file.c_str());
			assert_cgp(threw, "obj_read_connectivity must reject a face with fewer than 3 indices");
		}

		// ---------- loader rejects an out-of-range vertex index ----------
		// "f 0 0 0" decrements to index -1: the lower-bound assert must fire instead of
		//  reading position[-1] out of bounds.
		{
			std::string const file = "cgp_test_obj_badindex.obj";
			write_text_file(file, "v 0 0 0\nf 0 0 0\n");

			bool threw = false;
			try {
				mesh const loaded = mesh_load_file_obj(file);
				(void)loaded;
			}
			catch (std::logic_error const&) {
				threw = true;
			}
			std::remove(file.c_str());
			assert_cgp(threw, "mesh_load_file_obj must reject an out-of-range (negative) vertex index");
		}
	}
}
