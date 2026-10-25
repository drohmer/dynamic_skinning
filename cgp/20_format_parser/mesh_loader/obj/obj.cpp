#ifdef __linux__
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif 
#ifdef _WIN32
#pragma warning( disable : 4996 )
#endif

#include "obj.hpp"

#include "cgp/01_base/base.hpp"
#include "cgp/03_files/files.hpp"

#include <map>

#include <fstream>
#include <sstream>

namespace cgp
{

    void mesh_save_file_obj(std::string const& filename, mesh const& m)
    {
        std::ofstream stream(filename, std::ofstream::out);
        assert_cgp(stream.is_open(), "Cannot open file " + str(filename));

        for (int k = 0; k < m.position.size(); ++k)
            stream << "v " << m.position[k].x << " " << m.position[k].y <<" " << m.position[k].z << std::endl;
        for (int k = 0; k < m.uv.size(); ++k)
            stream << "vt " << m.uv[k].x <<" " << m.uv[k].y << std::endl;
        for (int k = 0; k < m.normal.size(); ++k)
            stream << "vn " << m.normal[k].x << " " << m.normal[k].y <<" " << m.normal[k].z << std::endl;

        bool const has_uv     = m.uv.size() == m.position.size();
        bool const has_normal = m.normal.size() == m.position.size();
        auto fmt_idx = [&](int idx) -> std::string {
            std::string s = str(idx + 1);
            if (has_uv && has_normal) return s + "/" + s + "/" + s;
            if (has_uv)               return s + "/" + s;
            if (has_normal)           return s + "//" + s;
            return s;
        };
        for (int k = 0; k < m.connectivity.size(); ++k) {
            stream << "f " << fmt_idx(m.connectivity[k][0]) << " "
                           << fmt_idx(m.connectivity[k][1]) << " "
                           << fmt_idx(m.connectivity[k][2]) << std::endl;
        }

        stream.close();
    }

    void mesh_save_file_obj(std::string const& filename, std::vector<vec3> const& position, std::vector<vec3> const& normal)
    {
        std::ofstream stream(filename, std::ofstream::out);
        assert_cgp(stream.is_open(), "Cannot open file " + str(filename));

        for (int k = 0; k < int(position.size()); ++k)
            stream << "v " << position[k].x << " " << position[k].y <<" " << position[k].z << std::endl;
        for (int k = 0; k < int(normal.size()); ++k)
            stream << "vn " << normal[k].x << " " << normal[k].y <<" " << normal[k].z << std::endl;

        assert_cgp(position.size() % 3 == 0, "mesh_save_file_obj: position count must be a multiple of 3 for triangle soup");
        for (int k = 0; k < int(position.size())/3; k++) {
            std::string const u0 = str(3*k+1);
            std::string const u1 = str(3*k+2);
            std::string const u2 = str(3*k+3);
            std::string const f0 = u0 + "//" + u0;
            std::string const f1 = u1 + "//" + u1;
            std::string const f2 = u2 + "//" + u2;

            stream << "f " << f0 << " " << f1 << " " << f2 << std::endl;
        }

        stream.close();
    }


// Comparator of triplet of integer for std::map
struct comparator_int3 {
    comparator_int3(int = 0) {}  // unused parameter kept for call-site compatibility

    bool operator()(int3 const& a, int3 const& b) const
    {
        if (a[0] != b[0]) return a[0] < b[0];
        if (a[1] != b[1]) return a[1] < b[1];
        return a[2] < b[2];
    }
};


static numarray<numarray_stack<int3,3>> triangulate_faces(numarray<numarray<int3>> faces);


static std::pair<mesh, std::map<int3, int, comparator_int3>>
    make_unique_parameter_per_value(numarray<vec3> const& positions,
                                    numarray<vec2> const& texture_uv,
                                    numarray<vec3> const& normals,
                                    numarray<numarray_stack<int3,3>> const& faces,
                                    loader::obj_type const type);


mesh mesh_load_file_obj(const std::string& filename)
{
    numarray<numarray<int>> vertex_correspondance;
     mesh m = mesh_load_file_obj(filename, vertex_correspondance);
     m.fill_empty_field();
     return m;
}
mesh mesh_load_file_obj(const std::string& filename, numarray<numarray<int> >& vertex_correspondance)
{
    assert_file_exist(filename);

    // Load parameters
    numarray<vec3> positions = loader::obj_read_positions(filename);
    numarray<vec2> texture_uv = loader::obj_read_texture_uv(filename);
    numarray<vec3> normals = loader::obj_read_normals(filename);

    assert_cgp(positions.size()>0, str("File ")+filename+" has 0 vertices");

    // set obj type
    loader::obj_type type = loader::obj_type::vertex;
    if(texture_uv.size()>0 && normals.size()>0)
        type = loader::obj_type::vertex_texture_normal;
    else if( texture_uv.size()>0 )
        type = loader::obj_type::vertex_texture;
    else if( normals.size()>0 )
        type = loader::obj_type::vertex_normal;

    // Load connectivity and triangulate
    numarray<numarray_stack<int3,3>> faces = triangulate_faces( loader::obj_read_faces(filename, type) );

    // Set unique per-vertex value for texture and normals (duplicate vertices if necessary)
    mesh m;
    std::map<int3, int, comparator_int3> connectivity_map;
    std::tie(m,connectivity_map) = make_unique_parameter_per_value(positions, texture_uv, normals, faces, type);

    // Retrieve correspondance between initial vertices in files and new ones
    vertex_correspondance.resize(positions.size());
    for(auto const& it : connectivity_map)
    {
        int const vertex_in = it.first[0];
        int const vertex_out = it.second;

        vertex_correspondance[vertex_in].push_back(vertex_out);
    }

    return m;
}


numarray<numarray_stack<int3,3>> triangulate_faces(numarray<numarray<int3>> faces)
{
    numarray<numarray_stack<int3,3>> faces_triangulation;
    size_t const N_face = faces.size();
    for(size_t k_face=0; k_face<N_face; ++k_face)
    {
        numarray<int3> const& current_polygon = faces[k_face];
        int const N_polygon = int(current_polygon.size());

        for(int k=0; k<N_polygon-2; ++k) {
            // triangulation
            int3 const& f0 = current_polygon[0];
            int3 const& f1 = current_polygon[k+1];
            int3 const& f2 = current_polygon[k+2];

            faces_triangulation.push_back({f0,f1,f2});
        }
    }
    return faces_triangulation;
}

std::pair<mesh, std::map<int3, int, comparator_int3>>
    make_unique_parameter_per_value(numarray<vec3> const& positions,
                                    numarray<vec2> const& texture_uv,
                                    numarray<vec3> const& normals,
                                    numarray<numarray_stack<int3,3>> const& faces,
                                    loader::obj_type const type)
{
    mesh m;
    comparator_int3 comparator(int(positions.size()));
    std::map<int3, int, comparator_int3> connectivity_map(comparator); // stores map between original face index and final offset


    size_t const N_triangle = faces.size();
    for(size_t k_triangle=0; k_triangle<N_triangle; ++k_triangle)
    {
        numarray_stack<int3,3> const& tri = faces[k_triangle];
        uint3 new_triangle_index;
        for(int k=0; k<3; ++k)
        {
            int3 const& index = tri[k];
            auto const it = connectivity_map.find( index );
            if( it==connectivity_map.end() ) {

                size_t const offset = m.position.size();
                connectivity_map[index] = int(offset);
                new_triangle_index[k] = int(offset);

                int const idx_position = index[0];

                assert_cgp_no_msg( idx_position>=0 && idx_position<int(positions.size()));
                m.position.push_back( positions[idx_position] );

                if(type==loader::obj_type::vertex_texture_normal || type==loader::obj_type::vertex_texture) {
                    int const idx_uv = index[1];
                    assert_cgp_no_msg( idx_uv>=0 && idx_uv<int(texture_uv.size()) );
                    m.uv.push_back( texture_uv[ idx_uv ] );
                }
                if(type==loader::obj_type::vertex_texture_normal || type==loader::obj_type::vertex_normal) {
                    int const idx_normal = index[2];
                    assert_cgp_no_msg( idx_normal>=0 && idx_normal<int(normals.size()) );
                    m.normal.push_back( normals[idx_normal] );
                }

            }
            else
                new_triangle_index[k] = it->second;
        }
        m.connectivity.push_back(new_triangle_index);
    }

    return {m, connectivity_map};
}


namespace loader{
// Read every value introduced by a given keyword ("v", "vn", "vt", ...) in an .obj file.
//  read_one extracts a single element from the remainder of a matching line.
// Factors the otherwise-identical bodies of obj_read_positions/normals/texture_uv.
template <typename T, typename FUNC>
static std::vector<T> obj_read_values(std::string const& filename, std::string const& keyword, FUNC read_one)
{
    assert_file_exist(filename);
    std::vector<T> values;

    std::ifstream stream(filename);
    assert_cgp(stream.is_open(), "Cannot open file "+str(filename));

    std::string buffer;
    while( std::getline(stream, buffer) ) {
        if( buffer.empty() )
            continue;
        std::stringstream tokens_buffer(buffer);
        std::string first_word;
        tokens_buffer >> first_word;
        if( first_word==keyword )
            values.push_back( read_one(tokens_buffer) );
    }
    stream.close();

    return values;
}

std::vector<vec3> obj_read_positions(const std::string& filename)
{
    return obj_read_values<vec3>(filename, "v",
        [](std::stringstream& t){ vec3 p; t >> p.x >> p.y >> p.z; return p; });
}

std::vector<vec3> obj_read_normals(const std::string& filename)
{
    return obj_read_values<vec3>(filename, "vn",
        [](std::stringstream& t){ vec3 n; t >> n.x >> n.y >> n.z; return n; });
}

std::vector<vec2> obj_read_texture_uv(const std::string& filename)
{
    return obj_read_values<vec2>(filename, "vt",
        [](std::stringstream& t){ vec2 uv; t >> uv.x >> uv.y; return uv; });
}


std::vector<uint3> obj_read_connectivity(const std::string& filename)
{
    assert_file_exist(filename);

    std::vector<uint3> connectivity;

    // Open file
    std::ifstream stream(filename);
    assert_cgp(stream.is_open(), "Cannot open file "+str(filename));


    while(stream.good())
    {
        std::string buffer;
        std::getline(stream,buffer);

        if( buffer.size()>0 )
        {
            std::stringstream tokens_buffer(buffer);
            std::string first_word;
            tokens_buffer >> first_word;

            if( first_word.size()>0 && first_word[0]!='#' )
            {
                if(first_word=="f")
                {
                    std::array<std::string,3> s;
                    tokens_buffer >> s[0] >> s[1] >> s[2];
                    assert_cgp(!s[0].empty() && !s[1].empty() && !s[2].empty(),
                        "obj_read_connectivity: face line has fewer than 3 vertex indices in "+str(filename));

                    uint3 f;
                    for(int k=0; k<3; ++k)
                        f[k] = std::stoi(s[k])-1;

                    connectivity.push_back(f);
                }
            }
        }
    }



    stream.close();

    return connectivity;
}

int3 extract_face_index(std::string const& word, obj_type const type)
{
    int3 indices = {0,0,0};


    if(type == obj_type::vertex)
        sscanf(word.c_str(), "%d", &indices[0]);
    else if( type==obj_type::vertex_texture )
        sscanf(word.c_str(), "%d/%d", &indices[0], &indices[1]);
    else if( type==obj_type::vertex_normal )
        sscanf(word.c_str(), "%d//%d", &indices[0], &indices[2]);
    else if( type==obj_type::vertex_texture_normal )
        sscanf(word.c_str(), "%d/%d/%d", &indices[0], &indices[1], &indices[2]);

    for(int k=0; k<3; ++k)
        indices[k]--;    // obj indices starts at 1

    return indices;

}


numarray<numarray<int3>> obj_read_faces(const std::string& filename, obj_type const type)
{
    assert_file_exist(filename);
    numarray<numarray<int3>> faces;

    std::ifstream stream(filename);
    assert_cgp(stream.is_open(), "Cannot open file "+str(filename));
    while(stream.good()) {
        std::string buffer;
        std::getline(stream,buffer);
        if( buffer.size()>0 )
        {
            std::stringstream tokens_buffer(buffer);
            std::string first_word;
            tokens_buffer >> first_word;
            if( first_word.size()>0 && first_word[0]!='#' ) {
                if( first_word=="f" ) {

                    std::string word;
                    cgp::numarray<int3> current_face;
                    while(tokens_buffer) {
                        tokens_buffer >> word;

                        if(tokens_buffer){
                            int3 face_index = extract_face_index(word, type);
                            current_face.push_back(face_index);
                        }
                    }
                    faces.push_back(current_face);

                }
            }
        }
    }
    stream.close();

    return faces;
}


}

}
