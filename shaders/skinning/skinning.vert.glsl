#version 330 core

// GPU Skinning vertex shader — LBS + Dynamic Skinning (pre-filtered velocity + bend deformer)
// The oscillation filter convolution is computed on CPU per joint;
// this shader only reads the pre-filtered velocity and applies the bend deformer per vertex.

// Inputs from VBOs
layout (location = 0) in vec3 vertex_position;   // bind pose position
layout (location = 1) in vec3 vertex_normal;      // bind pose normal
layout (location = 2) in vec3 vertex_color;
layout (location = 3) in vec2 vertex_uv;
layout (location = 4) in vec4 skinning_joint_indices;  // skeleton-space joint indices (as float)
layout (location = 5) in vec4 skinning_weights;        // LBS weights

// Output to fragment shader
out struct fragment_data
{
    vec3 position; // position in world space
    vec3 normal;   // normal in world space
    vec3 color;
    vec2 uv;
} fragment;

// Standard uniforms
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Skinning data textures
uniform sampler2D skinning_matrices_tex;   // 4 x N_joints, each row = one mat4 (T_j)
uniform sampler2D joint_global_tex;        // 4 x N_joints, each row = one mat4 (joint_matrix_global[j])
uniform sampler2D filtered_velocity_tex;   // 1 x N_joints, pre-filtered angular velocity per joint
uniform sampler2D hier_weights_tex;        // tiled 2D: (hier_max * hier_tile_w) x ceil(N_vertex / hier_tile_w)

// Dynamic skinning uniforms
uniform int ds_active;
uniform int hier_max;
uniform int hier_tile_w;    // tiling: vertices per row in hier_weights_tex
uniform float bend_factor;
uniform int display_bend_weights;  // if 1, encode γ^bend as white→red color


// ============================================================================
// Helper functions
// ============================================================================

mat4 get_mat4(sampler2D tex, int row) {
    return mat4(texelFetch(tex, ivec2(0, row), 0),
                texelFetch(tex, ivec2(1, row), 0),
                texelFetch(tex, ivec2(2, row), 0),
                texelFetch(tex, ivec2(3, row), 0));
}

vec3 get_translation(sampler2D tex, int row) {
    return texelFetch(tex, ivec2(3, row), 0).xyz;
}


void main()
{
    // ========================================================================
    // 1. Linear Blend Skinning (LBS)
    // ========================================================================
    vec3 p_skinned = vec3(0.0);
    vec3 n_skinned = vec3(0.0);

    for (int i = 0; i < 4; ++i) {
        float w = skinning_weights[i];
        if (w < 1e-6) continue;

        int joint_idx = int(skinning_joint_indices[i]);
        mat4 T = get_mat4(skinning_matrices_tex, joint_idx);
        p_skinned += w * (T * vec4(vertex_position, 1.0)).xyz;
        n_skinned += w * (T * vec4(vertex_normal, 0.0)).xyz;
    }

    // ========================================================================
    // 2. Dynamic Skinning (pre-filtered velocity + bend deformer)
    // ========================================================================
    vec3 deformation = vec3(0.0);

    if (ds_active != 0) {
        // Tiled addressing for hier_weights_tex
        int tile_row = gl_VertexID / hier_tile_w;
        int tile_col_base = (gl_VertexID % hier_tile_w) * hier_max;

        // Read per-vertex bend weight γ^bend from first entry's .z channel
        float weight_bend = texelFetch(hier_weights_tex, ivec2(tile_col_base, tile_row), 0).z;

        for (int k = 0; k < hier_max; ++k) {
            // Read hierarchical weight entry: (joint_idx, beta, weight_bend, 0)
            vec4 hw = texelFetch(hier_weights_tex, ivec2(tile_col_base + k, tile_row), 0);
            int j = int(hw.x);
            float beta = hw.y;
            if (beta < 1e-6) break;  // sentinel: end of list

            // Read pre-filtered angular velocity (computed on CPU)
            vec3 filtered = texelFetch(filtered_velocity_tex, ivec2(0, j), 0).xyz;

            float on = length(filtered);
            if (on < 0.01) continue;
            vec3 od = filtered / on;

            // --- Bend deformer (Rodrigues formula) ---
            vec3 pj = get_translation(joint_global_tex, j);
            vec3 u = p_skinned - pj;

            // v = component of u perpendicular to rotation axis
            vec3 v = u - dot(u, od) * od;

            // theta = -bend_factor * weight_bend * ||omega x u||
            float cross_norm = on * length(cross(od, u));
            float theta = -bend_factor * weight_bend * cross_norm;

            // (R - I) * v = sin(theta) * (od x v) + (cos(theta) - 1) * v
            deformation += beta * (sin(theta) * cross(od, v) + (cos(theta) - 1.0) * v);
        }
    }

    // ========================================================================
    // 3. Output
    // ========================================================================
    vec3 final_position = p_skinned + deformation;
    vec4 world_position = model * vec4(final_position, 1.0);

    fragment.position = world_position.xyz;
    fragment.normal   = mat3(transpose(inverse(model))) * normalize(n_skinned);
    fragment.uv       = vertex_uv;

    // Encode bend weight γ^bend as color: white (w=0) → red (w=1)
    if (display_bend_weights != 0 && ds_active != 0) {
        int tr = gl_VertexID / hier_tile_w;
        int tc = (gl_VertexID % hier_tile_w) * hier_max;
        float wb = texelFetch(hier_weights_tex, ivec2(tc, tr), 0).z;
        fragment.color = vec3(1.0, 1.0 - wb, 1.0 - wb);
    } else {
        fragment.color = vertex_color;
    }

    gl_Position = projection * view * world_position;
}
