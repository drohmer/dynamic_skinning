#pragma once

#include "cgp/cgp.hpp"
#include <fstream>
#include <sstream>
#include <string>

// Shared file reading helpers for skeleton/rig asset formats

inline cgp::numarray<std::string> read_joint_names(std::string const& filename)
{
    cgp::numarray<std::string> names;
    std::ifstream fid(filename);
    std::string line;
    while (std::getline(fid, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start != std::string::npos)
            names.push_back(line.substr(start, end - start + 1));
    }
    return names;
}

inline cgp::numarray<int> read_parent_indices(std::string const& filename)
{
    cgp::numarray<int> parents;
    std::ifstream fid(filename);
    int val;
    while (fid >> val)
        parents.push_back(val);
    return parents;
}

inline cgp::numarray<cgp::vec3> read_vec3_file(std::string const& filename)
{
    cgp::numarray<cgp::vec3> data;
    std::ifstream fid(filename);
    float x, y, z;
    while (fid >> x >> y >> z)
        data.push_back({x, y, z});
    return data;
}

inline cgp::numarray<cgp::quaternion> read_quaternion_file(std::string const& filename)
{
    cgp::numarray<cgp::quaternion> data;
    std::ifstream fid(filename);
    float qx, qy, qz, qw;
    while (fid >> qx >> qy >> qz >> qw)
        data.push_back({qx, qy, qz, qw});
    return data;
}

inline cgp::numarray<cgp::numarray<int>> read_rig_joint(std::string const& filename)
{
    cgp::numarray<cgp::numarray<int>> result;
    std::ifstream fid(filename);
    std::string line;
    while (std::getline(fid, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        cgp::numarray<int> joints;
        int val;
        while (ss >> val)
            joints.push_back(val);
        if (joints.size() > 0)
            result.push_back(joints);
    }
    return result;
}

inline cgp::numarray<cgp::numarray<float>> read_rig_weights(std::string const& filename)
{
    cgp::numarray<cgp::numarray<float>> result;
    std::ifstream fid(filename);
    std::string line;
    while (std::getline(fid, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        cgp::numarray<float> weights;
        float val;
        while (ss >> val)
            weights.push_back(val);
        if (weights.size() > 0)
            result.push_back(weights);
    }
    return result;
}

// Build mat4 from translation + quaternion
inline cgp::mat4 build_joint_matrix(cgp::vec3 const& t, cgp::quaternion const& q)
{
    cgp::mat3 R = cgp::rotation_transform(q).matrix();
    cgp::mat4 M;
    M(0,0) = R(0,0); M(0,1) = R(0,1); M(0,2) = R(0,2); M(0,3) = t.x;
    M(1,0) = R(1,0); M(1,1) = R(1,1); M(1,2) = R(1,2); M(1,3) = t.y;
    M(2,0) = R(2,0); M(2,1) = R(2,1); M(2,2) = R(2,2); M(2,3) = t.z;
    M(3,0) = 0; M(3,1) = 0; M(3,2) = 0; M(3,3) = 1;
    return M;
}
