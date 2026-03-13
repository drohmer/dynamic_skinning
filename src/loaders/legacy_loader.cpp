#include "legacy_loader.hpp"
#include "environment.hpp"

#include <fstream>
#include <sstream>

using namespace cgp;


// ---- File reading helpers ----

static numarray<std::string> read_joint_names(std::string const& filename)
{
    numarray<std::string> names;
    std::ifstream fid(filename);
    std::string line;
    while (std::getline(fid, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start != std::string::npos)
            names.push_back(line.substr(start, end - start + 1));
    }
    return names;
}

static numarray<int> read_parent_indices(std::string const& filename)
{
    numarray<int> parents;
    std::ifstream fid(filename);
    int val;
    while (fid >> val)
        parents.push_back(val);
    return parents;
}

static numarray<vec3> read_vec3_file(std::string const& filename)
{
    numarray<vec3> data;
    std::ifstream fid(filename);
    float x, y, z;
    while (fid >> x >> y >> z)
        data.push_back({x, y, z});
    return data;
}

static numarray<quaternion> read_quaternion_file(std::string const& filename)
{
    numarray<quaternion> data;
    std::ifstream fid(filename);
    float qx, qy, qz, qw;
    while (fid >> qx >> qy >> qz >> qw)
        data.push_back({qx, qy, qz, qw});
    return data;
}

static numarray<numarray<int>> read_rig_joint(std::string const& filename)
{
    numarray<numarray<int>> result;
    std::ifstream fid(filename);
    std::string line;
    while (std::getline(fid, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        numarray<int> joints;
        int val;
        while (ss >> val)
            joints.push_back(val);
        if (joints.size() > 0)
            result.push_back(joints);
    }
    return result;
}

static numarray<numarray<float>> read_rig_weights(std::string const& filename)
{
    numarray<numarray<float>> result;
    std::ifstream fid(filename);
    std::string line;
    while (std::getline(fid, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        numarray<float> weights;
        float val;
        while (ss >> val)
            weights.push_back(val);
        if (weights.size() > 0)
            result.push_back(weights);
    }
    return result;
}


// ---- Animation reading ----

struct anim_keyframe {
    float time;
    vec3 translation;
    quaternion rotation;
};

static numarray<numarray<anim_keyframe>> read_skeleton_animation(std::string const& filename, float scaling)
{
    numarray<numarray<anim_keyframe>> result;
    std::ifstream fid(filename);

    while (fid.good()) {
        int N_key = 0;
        fid >> N_key;
        if (!fid.good() || N_key <= 0) break;

        numarray<anim_keyframe> joint_anim;
        joint_anim.resize(N_key);

        for (int k = 0; k < N_key; ++k) {
            float t;
            vec3 p;
            float qx, qy, qz, qw;

            fid >> t >> p.x >> p.y >> p.z >> qx >> qy >> qz >> qw;

            // Normalize quaternion
            float qn = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
            if (qn > 1e-6f) { qx /= qn; qy /= qn; qz /= qn; qw /= qn; }

            joint_anim[k] = {t, p * scaling, {qx, qy, qz, qw}};
        }
        result.push_back(joint_anim);
    }
    return result;
}

static float find_animation_length(numarray<numarray<anim_keyframe>> const& anim)
{
    float end_time = 0;
    float prev_end_time = 0;
    for (int j = 0; j < anim.size(); ++j) {
        for (int k = 0; k < anim[j].size(); ++k) {
            if (anim[j][k].time > end_time) {
                prev_end_time = end_time;
                end_time = anim[j][k].time;
            }
        }
    }
    return end_time - (end_time - prev_end_time) / 10.0f;
}


// ---- Build mat4 from translation + quaternion ----

static mat4 build_joint_matrix(vec3 const& t, quaternion const& q)
{
    mat3 R = rotation_transform(q).matrix();
    mat4 M;
    M(0,0) = R(0,0); M(0,1) = R(0,1); M(0,2) = R(0,2); M(0,3) = t.x;
    M(1,0) = R(1,0); M(1,1) = R(1,1); M(1,2) = R(1,2); M(1,3) = t.y;
    M(2,0) = R(2,0); M(2,1) = R(2,1); M(2,2) = R(2,2); M(2,3) = t.z;
    M(3,0) = 0; M(3,1) = 0; M(3,2) = 0; M(3,3) = 1;
    return M;
}


// ---- Main loader ----

character_structure load_legacy_character(std::string const& directory, float scaling, bool invert_normals)
{
    std::string dir = project::path + directory;

    // Load mesh with vertex correspondance (OBJ may duplicate vertices for UV)
    numarray<numarray<int>> vertex_correspondance;
    mesh m = mesh_load_file_obj(dir + "mesh.obj", vertex_correspondance);
    m.position *= scaling;
    if (invert_normals) {
        // Flip triangle winding
        for (int k = 0; k < m.connectivity.size(); ++k)
            std::swap(m.connectivity[k].y, m.connectivity[k].z);
    }
    m.normal = normal_per_vertex(m.position, m.connectivity);
    m.fill_empty_field();

    // Load skeleton data
    numarray<std::string> joint_names = read_joint_names(dir + "skeleton_joint_name");
    numarray<int> parent_indices = read_parent_indices(dir + "skeleton_connectivity_parent");
    numarray<vec3> rest_translations = read_vec3_file(dir + "skeleton_rest_pose_translation");
    numarray<quaternion> rest_rotations = read_quaternion_file(dir + "skeleton_rest_pose_rotation");

    int N_joint = joint_names.size();

    // Load rig data (per original vertex, before correspondance)
    numarray<numarray<int>> rig_joints_raw = read_rig_joint(dir + "rig_joint");
    numarray<numarray<float>> rig_weights_raw = read_rig_weights(dir + "rig_weights");

    // Apply vertex correspondance mapping
    // (original vertex k maps to multiple mesh vertices via vertex_correspondance[k])
    int N_vertex = m.position.size();

    // Build mapped skinning weights
    numarray<numarray<skinning_weight_info>> vertex_deps;
    vertex_deps.resize(N_vertex);

    // Map correspondance: for each original vertex k, copy its skinning to all duplicated vertices
    for (int k = 0; k < vertex_correspondance.size(); ++k) {
        numarray<skinning_weight_info> deps;
        if (k < rig_joints_raw.size()) {
            for (int j = 0; j < rig_joints_raw[k].size(); ++j) {
                deps.push_back({rig_joints_raw[k][j], rig_weights_raw[k][j]});
            }
        }
        for (int d = 0; d < vertex_correspondance[k].size(); ++d) {
            int mapped_vertex = vertex_correspondance[k][d];
            if (mapped_vertex < N_vertex)
                vertex_deps[mapped_vertex] = deps;
        }
    }

    // ---- Build skeleton ----
    skeleton_structure skeleton;
    skeleton.joint_name.resize(N_joint);
    skeleton.parent_index.resize(N_joint);
    skeleton.joint_matrix_local.resize(N_joint);
    skeleton.joint_matrix_global.resize(N_joint);

    for (int k = 0; k < N_joint; ++k) {
        skeleton.joint_name[k] = joint_names[k];
        skeleton.parent_index[k] = parent_indices[k];
        skeleton.joint_matrix_local[k] = build_joint_matrix(rest_translations[k] * scaling, rest_rotations[k]);
    }
    skeleton.update_joint_matrix_local_to_global();

    // ---- Build controller_skinning ----
    // In old format, rig joints directly index the skeleton (no separate rig-to-skeleton mapping)
    controller_skinning_structure controller;
    controller.vertex_to_joint_dependence = vertex_deps;

    // rig_index_to_skeleton_index is identity: joint i in rig = joint i in skeleton
    controller.rig_index_to_skeleton_index.resize(N_joint);
    for (int k = 0; k < N_joint; ++k)
        controller.rig_index_to_skeleton_index[k] = k;

    // Compute inverse bind matrices from rest pose global matrices
    controller.inverse_bind_matrices.resize(N_joint);
    for (int k = 0; k < N_joint; ++k)
        controller.inverse_bind_matrices[k] = inverse(skeleton.joint_matrix_global[k]);

    controller.global_bind_matrix = mat4::build_identity();

    // ---- Build animation ----
    // Check if animation file exists
    std::string anim_file = dir + "skeleton_animation";
    std::ifstream test_anim(anim_file);
    bool has_animation = test_anim.good();
    test_anim.close();

    float anim_time_max = 2.0f;
    skeleton_animation_structure anim;
    anim.joint_index.resize(N_joint);
    anim.times.resize(N_joint);
    anim.matrix.resize(N_joint);

    for (int k = 0; k < N_joint; ++k)
        anim.joint_index[k] = k;

    if (has_animation) {
        auto raw_anim = read_skeleton_animation(anim_file, scaling);
        anim_time_max = find_animation_length(raw_anim);

        for (int k = 0; k < N_joint && k < raw_anim.size(); ++k) {
            int N_key = raw_anim[k].size();
            anim.times[k].resize(N_key);
            anim.matrix[k].resize(N_key);
            for (int t = 0; t < N_key; ++t) {
                anim.times[k][t] = raw_anim[k][t].time;
                anim.matrix[k][t] = build_joint_matrix(raw_anim[k][t].translation, raw_anim[k][t].rotation);
            }
        }
    }
    else {
        // Static pose: 3 identical keyframes
        for (int k = 0; k < N_joint; ++k) {
            anim.times[k].resize(3);
            anim.matrix[k].resize(3);
            for (int t = 0; t < 3; ++t) {
                anim.times[k][t] = static_cast<float>(t);
                anim.matrix[k][t] = skeleton.joint_matrix_local[k];
            }
        }
    }
    anim.time_max = anim_time_max;

    // ---- Assemble character ----
    character_structure character;
    animated_model_structure& model = character.animated_model;
    model.skeleton = skeleton;

    rigged_mesh_structure rigged;
    rigged.mesh_bind_pose = m;
    rigged.mesh_deformed = m;
    rigged.controller_skinning = controller;
    model.rigged_mesh["body"] = rigged;
    model.animation["Default"] = anim;

    character.current_animation_name = "Default";
    character.timer.event_period = anim_time_max;

    // Initialize drawable
    mesh_drawable drawable;
    drawable.initialize_data_on_gpu(m);

    // Try to load texture
    std::string tex_png = dir + "texture.png";
    std::string tex_jpg = dir + "texture.jpg";
    std::ifstream test_png(tex_png);
    std::ifstream test_jpg(tex_jpg);
    if (test_png.good()) {
        drawable.texture.load_and_initialize_texture_2d_on_gpu(tex_png);
    } else if (test_jpg.good()) {
        drawable.texture.load_and_initialize_texture_2d_on_gpu(tex_jpg);
    }
    test_png.close();
    test_jpg.close();

    character.drawable["body"] = drawable;
    character.sk_drawable = skeleton_drawable(model.skeleton);

    return character;
}


std::vector<legacy_model_entry> get_legacy_models()
{
    return {
        {"Clown",       "assets/clown/",        1.0f,  false},
        {"Dragon",      "assets/dragon/",       0.09f, false},
        {"Girafe",      "assets/girafe/",       2.0f,  true},
        {"Snail",       "assets/snail/",        1.0f,  false},
        {"Spot",        "assets/spot/",         1.0f,  false},
    };
}
