#version 330 core
// Full precision: in the web version (OpenGL ES 3.0), the shader would otherwise use mediump floats
// (16 bits on phones and tablets) and lowp samplers in the vertex shader, which destroys the skinning.
// Without effect on desktop OpenGL.
precision highp float;
precision highp sampler2D;

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
layout (location = 6) in vec2 delay_distance;          // distance d_i along the skeleton (delayed LBS)

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
uniform sampler2D hier_weights_tex;       // tiled 2D: (hier_max * hier_tile_w) x ceil(N_vertex / hier_tile_w)
uniform sampler2D history_tex;             // (4 N_history) x N_joints: past local joint matrices (delayed LBS)
uniform sampler2D dlbs_joint_tex;          // 5 x N_joints: B_j^-1, then (parent, d_j) (delayed LBS)

// Dynamic skinning uniforms
uniform int ds_active;
uniform int hier_max;
uniform int hier_tile_w;    // tiling: vertices per row in hier_weights_tex
uniform float bend_factor;
uniform int display_bend_weights;  // if 1, encode γ^bend as white→soft red color

// Per-joint data of the bend deformer, read by every vertex for each of its joints: uniform arrays rather
// than textures, to avoid two dependent texture reads per joint and per vertex (twice as fast on mobile GPUs)
const int MAX_JOINTS = 128;  // = gpu_skinning_state::MAX_JOINTS
uniform vec4 joint_velocity[MAX_JOINTS];  // pre-filtered angular velocity: unit axis (xyz) and norm (w)
uniform vec4 joint_center[MAX_JOINTS];    // position of the joint in the current pose (xyz)

// Delayed LBS uniforms
uniform int dlbs_active;
uniform int dlbs_head;                  // index of the most recent pose in the ring buffer
uniform int dlbs_N;                     // number of stored poses
uniform float dlbs_steps_per_distance;  // delay in time steps per unit of distance: 1 / (c dt)


// ============================================================================
// Helper functions
// ============================================================================

mat4 get_mat4(sampler2D tex, int row) {
    return mat4(texelFetch(tex, ivec2(0, row), 0),
                texelFetch(tex, ivec2(1, row), 0),
                texelFetch(tex, ivec2(2, row), 0),
                texelFetch(tex, ivec2(3, row), 0));
}

// Local matrix of joint j for the stored pose k (ring buffer index)
mat4 get_history_mat4(int k, int j) {
    return mat4(texelFetch(history_tex, ivec2(4 * k + 0, j), 0),
                texelFetch(history_tex, ivec2(4 * k + 1, j), 0),
                texelFetch(history_tex, ivec2(4 * k + 2, j), 0),
                texelFetch(history_tex, ivec2(4 * k + 3, j), 0));
}

// Delayed skinning matrix of joint j for a vertex at the distance d along the skeleton:
//   T_j^d = Π_{k ∈ root..j} L_k(t - (d - d_k) / c)  B_j^-1
mat4 get_delayed_skinning_mat4(int j, float d) {
    mat4 G = mat4(1.0);
    int k = j;
    for (int depth = 0; depth < 256 && k >= 0; ++depth) {
        vec4 info = texelFetch(dlbs_joint_tex, ivec2(4, k), 0);  // (parent, d_k)
        // Delay of L_k in time steps, interpolated between two stored poses
        float s = clamp((d - info.y) * dlbs_steps_per_distance, 0.0, float(dlbs_N - 2));
        int k0 = int(s);
        float a = s - float(k0);
        int frame0 = (dlbs_head - k0 + dlbs_N) % dlbs_N;
        int frame1 = (dlbs_head - k0 - 1 + dlbs_N) % dlbs_N;
        G = ((1.0 - a) * get_history_mat4(frame0, k) + a * get_history_mat4(frame1, k)) * G;
        k = int(info.x);
    }
    return G * get_mat4(dlbs_joint_tex, j);
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
        mat4 T = (dlbs_active != 0)
            ? get_delayed_skinning_mat4(joint_idx, delay_distance.x)
            : get_mat4(skinning_matrices_tex, joint_idx);
        p_skinned += w * (T * vec4(vertex_position, 1.0)).xyz;
        n_skinned += w * (T * vec4(vertex_normal, 0.0)).xyz;
    }

    // ========================================================================
    // 2. Dynamic Skinning (pre-filtered velocity + bend deformer)
    // ========================================================================
    vec3 deformation = vec3(0.0);
    vec3 normal_deformation = vec3(0.0);  // normals follow the same per-joint rotations

    if (ds_active != 0) {
        // Tiled addressing for hier_weights_tex
        int tile_row = gl_VertexID / hier_tile_w;
        int tile_col_base = (gl_VertexID % hier_tile_w) * hier_max;

        // Read per-vertex bend weight γ^bend from first entry's .z channel
        float weight_bend = texelFetch(hier_weights_tex, ivec2(tile_col_base, tile_row), 0).z;

        for (int k = 0; k < hier_max; ++k) {
            // Read hierarchical weight entry: (joint_idx, beta, weight_bend, center_joint_idx)
            vec4 hw = texelFetch(hier_weights_tex, ivec2(tile_col_base + k, tile_row), 0);
            int j = int(hw.x);
            float beta = hw.y;
            if (beta < 1e-6) break;  // sentinel: end of list

            // Pre-filtered angular velocity (computed on CPU): unit axis and norm
            vec4 velocity = joint_velocity[j];
            float on = velocity.w;
            if (on < 0.01) continue;
            vec3 od = velocity.xyz;

            // --- Bend deformer (Rodrigues formula) ---
            vec3 pj = joint_center[int(hw.w)].xyz;  // rotation center: q_j, or a flexible origin
            vec3 u = p_skinned - pj;

            // v = component of u perpendicular to rotation axis
            vec3 v = u - dot(u, od) * od;

            // theta = -bend_factor * weight_bend * ||omega x u||
            float cross_norm = on * length(cross(od, u));
            float theta = -bend_factor * weight_bend * cross_norm;

            // (R - I) * v = sin(theta) * (od x v) + (cos(theta) - 1) * v
            deformation += beta * (sin(theta) * cross(od, v) + (cos(theta) - 1.0) * v);

            // (R - I) * n with Rodrigues (n is not orthogonal to the axis)
            vec3 od_x_n = cross(od, n_skinned);
            normal_deformation += beta * (sin(theta) * od_x_n + (1.0 - cos(theta)) * cross(od, od_x_n));
        }
    }

    // ========================================================================
    // 3. Output
    // ========================================================================
    vec3 final_position = p_skinned + deformation;
    vec4 world_position = model * vec4(final_position, 1.0);

    fragment.position = world_position.xyz;
    fragment.normal   = mat3(transpose(inverse(model))) * normalize(n_skinned + normal_deformation);
    fragment.uv       = vertex_uv;

    // Encode bend weight γ^bend as color: white (w=0) → red (w=1)
    if (display_bend_weights != 0 && ds_active != 0) {
        int tr = gl_VertexID / hier_tile_w;
        int tc = (gl_VertexID % hier_tile_w) * hier_max;
        float wb = texelFetch(hier_weights_tex, ivec2(tc, tr), 0).z;
        fragment.color = mix(vec3(1.0), vec3(0.85, 0.45, 0.42), wb);  // white -> soft red
    } else {
        fragment.color = vertex_color;
    }

    gl_Position = projection * view * world_position;
}
