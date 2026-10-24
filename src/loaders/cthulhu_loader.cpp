#include "cthulhu_loader.hpp"
#include "file_readers.hpp"
#include "environment.hpp"

#include <fstream>
#include <sstream>
#include <cmath>
#include <iostream>

using namespace cgp;


// ---- Binary file readers ----

static void read_binary_vec3(std::string const& filename, numarray<vec3>& data, int N)
{
    data.resize(N);
    std::ifstream s(filename, std::ios::in | std::ios::binary);
    if (!s.is_open())
        throw std::runtime_error("Cannot open file: " + filename);
    s.read(reinterpret_cast<char*>(&data[0].x), 3 * N * sizeof(float));
}

static void read_binary_connectivity(std::string const& filename, numarray<uint3>& data, int N)
{
    data.resize(N);
    std::ifstream s(filename, std::ios::in | std::ios::binary);
    if (!s.is_open())
        throw std::runtime_error("Cannot open file: " + filename);
    s.read(reinterpret_cast<char*>(&data[0][0]), 3 * N * sizeof(unsigned int));
}

static void read_binary_floats(std::string const& filename, numarray<float>& data, int N)
{
    data.resize(N);
    std::ifstream s(filename, std::ios::in | std::ios::binary);
    if (!s.is_open())
        throw std::runtime_error("Cannot open file: " + filename);
    s.read(reinterpret_cast<char*>(&data[0]), N * sizeof(float));
}


// ---- Main loader ----

character_structure load_cthulhu_character(std::string const& base_dir,
                                          std::string const& resolution,
                                          bool use_bend_weights)
{
    // Build paths based on resolution
    std::string sub_dir = base_dir;
    if (resolution != "default")
        sub_dir = base_dir + resolution + "/";
    std::string bin_dir = sub_dir + "binary/";
    std::string rig_dir = sub_dir + "rig/";

    // ---- 1. Read mesh size ----
    int N_vertex = 0;
    int N_triangles = 0;
    {
        std::ifstream s(bin_dir + "size.txt");
        s >> N_vertex >> N_triangles;
    }
    std::cout << "Cthulhu: loading mesh (N_vertex=" << N_vertex << ", N_triangles=" << N_triangles << ") ..." << std::endl;

    // ---- 2. Read binary mesh data ----
    numarray<vec3> positions;
    numarray<vec3> normals;
    numarray<uint3> connectivity;
    numarray<float> bend_weights_raw;  // γ^bend (per-vertex bend weight)

    read_binary_vec3(bin_dir + "point_set.dat", positions, N_vertex);
    read_binary_vec3(bin_dir + "normal.dat", normals, N_vertex);
    read_binary_connectivity(bin_dir + "connectivity.dat", connectivity, N_triangles);
    if (use_bend_weights) {
        read_binary_floats(bin_dir + "floppy_weights", bend_weights_raw, N_vertex);

        // Transform floppy_weights → weight_bend (γ^bend):
        //   w = 1 - min(|2w|, 1)
        // Values near 0 in raw → high bend weight (1.0), values near ±0.5 → low bend weight (0.0)
        for (int i = 0; i < N_vertex; ++i) {
            float w = std::abs(2.0f * bend_weights_raw[i]);
            w = std::min(w, 1.0f);
            bend_weights_raw[i] = 1.0f - w;
        }
    }

    // Recompute normals from geometry
    normals = normal_per_vertex(positions, connectivity);
    std::cout << "Cthulhu: mesh loaded OK" << std::endl;

    // ---- 3. Build cgp mesh ----
    mesh m;
    m.position = positions;
    m.normal = normals;
    m.connectivity = connectivity;
    m.fill_empty_field();

    // ---- 4. Read skeleton ----
    std::cout << "Cthulhu: loading skeleton ..." << std::endl;
    numarray<std::string> joint_names = read_joint_names(rig_dir + "skeleton_joint_name");
    numarray<int> parent_indices = read_parent_indices(rig_dir + "skeleton_connectivity_parent");
    numarray<vec3> rest_translations = read_vec3_file(rig_dir + "skeleton_rest_pose_translation");
    numarray<quaternion> rest_rotations = read_quaternion_file(rig_dir + "skeleton_rest_pose_rotation");

    int N_joint = joint_names.size();

    skeleton_structure skeleton;
    skeleton.joint_name.resize(N_joint);
    skeleton.parent_index.resize(N_joint);
    skeleton.joint_matrix_local.resize(N_joint);
    skeleton.joint_matrix_global.resize(N_joint);

    for (int k = 0; k < N_joint; ++k) {
        skeleton.joint_name[k] = joint_names[k];
        skeleton.parent_index[k] = parent_indices[k];
        skeleton.joint_matrix_local[k] = build_joint_matrix(rest_translations[k], rest_rotations[k]);
    }
    skeleton.update_joint_matrix_local_to_global();
    std::cout << "Cthulhu: skeleton loaded (" << N_joint << " joints) OK" << std::endl;

    // ---- 5. Read rig weights ----
    std::cout << "Cthulhu: loading rig weights ..." << std::endl;

    // Build skinning weight dependencies (direct mapping, no vertex correspondance)
    // Note: rig files may have empty lines for vertices with no dependencies.
    // read_rig_joint/read_rig_weights skip empty lines, so we need to read
    // the files directly to preserve line-to-vertex correspondence.
    numarray<numarray<skinning_weight_info>> vertex_deps;
    vertex_deps.resize(N_vertex);
    {
        std::ifstream fj(rig_dir + "rig_joint");
        std::ifstream fw(rig_dir + "rig_weights");
        std::string line_j, line_w;
        int vertex_idx = 0;
        while (std::getline(fj, line_j) && std::getline(fw, line_w) && vertex_idx < N_vertex) {
            std::istringstream ssj(line_j);
            std::istringstream ssw(line_w);
            int joint_val;
            float weight_val;
            while (ssj >> joint_val && ssw >> weight_val) {
                vertex_deps[vertex_idx].push_back({joint_val, weight_val});
            }
            ++vertex_idx;
        }
    }
    std::cout << "Cthulhu: rig weights loaded OK" << std::endl;

    // ---- 6. Build controller_skinning ----
    controller_skinning_structure controller;
    controller.vertex_to_joint_dependence = vertex_deps;

    // Identity mapping: rig joint index = skeleton joint index
    controller.rig_index_to_skeleton_index.resize(N_joint);
    for (int k = 0; k < N_joint; ++k)
        controller.rig_index_to_skeleton_index[k] = k;

    // Inverse bind matrices from rest pose
    controller.inverse_bind_matrices.resize(N_joint);
    for (int k = 0; k < N_joint; ++k)
        controller.inverse_bind_matrices[k] = inverse(skeleton.joint_matrix_global[k]);

    controller.global_bind_matrix = mat4::build_identity();

    // ---- 7. Build static animation (3 identical keyframes at rest pose) ----
    skeleton_animation_structure anim;
    anim.joint_index.resize(N_joint);
    anim.times.resize(N_joint);
    anim.matrix.resize(N_joint);

    for (int k = 0; k < N_joint; ++k) {
        anim.joint_index[k] = k;
        anim.times[k].resize(3);
        anim.matrix[k].resize(3);
        for (int t = 0; t < 3; ++t) {
            anim.times[k][t] = static_cast<float>(t);
            anim.matrix[k][t] = skeleton.joint_matrix_local[k];
        }
    }
    anim.time_max = 2.0f;

    // ---- 8. Assemble character ----
    character_structure character;
    animated_model_structure& model = character.animated_model;
    model.skeleton = skeleton;

    rigged_mesh_structure rigged;
    rigged.mesh_bind_pose = m;
    rigged.mesh_deformed = m;
    rigged.controller_skinning = controller;
    if (use_bend_weights)
        rigged.weight_bend = bend_weights_raw;  // γ^bend per-vertex bend weight
    model.rigged_mesh["body"] = rigged;
    model.animation["Default"] = anim;

    character.current_animation_name = "Default";
    character.timer.event_period = 2.0f;

    // Initialize drawable
    mesh_drawable drawable;
    drawable.initialize_data_on_gpu(m);
    character.drawable["body"] = drawable;
    character.sk_drawable = skeleton_drawable(model.skeleton);

    // ---- 9. Store joint names for FBone filtering ----
    // FBone joints (names starting with 'F') should be excluded from
    // dynamic skinning hierarchical weights. Store the exclusion set.
    for (int k = 0; k < N_joint; ++k) {
        if (!joint_names[k].empty() && joint_names[k][0] == 'F') {
            character.animated_model.excluded_ds_joints.insert(k);
        }
    }

    std::cout << "Cthulhu: character loaded successfully" << std::endl;
    return character;
}
