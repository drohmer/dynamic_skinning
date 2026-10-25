#include "normalize_character.hpp"

using namespace cgp;


// Normalize the displayed mesh to fit within [-1, 1] on the largest axis.
// Requires one skinning pass to have been run first (mesh_deformed in display coordinates).
// Only translations are scaled — rotation blocks stay pure (no scaling embedded).
//
// Math: after normalization, skinning produces p' = s*(p_deformed - center).
// - Skeleton: offset root by -center, scale all translations by s
// - Animations: same treatment
// - Inverse bind: scale by s (derived from inv_bind' = s * inv_bind)
// - Mesh bind pose: UNCHANGED (it may live in a different scale, as with asset_loader)
normalization_transform normalize_character(character_structure& character)
{
    auto& model = character.animated_model;

    // 1. Run one skinning pass to get mesh_deformed in display coordinates
    model.set_skeleton_from_animation(
        character.current_animation_name, 0.0f);
    for (auto& entry : model.rigged_mesh)
        model.skinning_lbs(entry.first);

    // 2. Compute bounding box from deformed mesh (display coordinates)
    vec3 p_min = vec3(1e10f, 1e10f, 1e10f);
    vec3 p_max = vec3(-1e10f, -1e10f, -1e10f);
    for (auto const& entry : model.rigged_mesh) {
        for (auto const& p : entry.second.mesh_deformed.position) {
            p_min = vec3(std::min(p_min.x, p.x), std::min(p_min.y, p.y), std::min(p_min.z, p.z));
            p_max = vec3(std::max(p_max.x, p.x), std::max(p_max.y, p.y), std::max(p_max.z, p.z));
        }
    }

    vec3 center = (p_min + p_max) / 2.0f;
    vec3 extent = p_max - p_min;
    float max_extent = std::max(std::max(extent.x, extent.y), extent.z);
    if (max_extent < 1e-5f) return {};
    float s = 2.0f / max_extent;

    // 3. Transform skeleton: offset root by -center, then scale ALL translations by s.
    //    Rotation blocks are untouched — joint_matrix_global will have pure rotations.
    if (model.skeleton.joint_matrix_local.size() > 0) {
        vec3 t0 = model.skeleton.joint_matrix_local[0].get_block_translation();
        model.skeleton.joint_matrix_local[0].set_block_translation(t0 - center);

        int N_joints = model.skeleton.joint_matrix_local.size();
        for (int k = 0; k < N_joints; ++k)
            model.skeleton.joint_matrix_local[k].apply_scaling_to_block_translation(s);

        model.skeleton.update_joint_matrix_local_to_global();
    }

    // 4. Transform animation matrices (same pattern: offset root, scale all translations)
    for (auto& anim_entry : model.animation) {
        auto& anim = anim_entry.second;
        for (int k = 0; k < static_cast<int>(anim.joint_index.size()); ++k) {
            bool is_root = (anim.joint_index[k] == 0);

            int N_frames = anim.matrix[k].size();
            for (int t = 0; t < N_frames; ++t) {
                if (is_root) {
                    vec3 tr = anim.matrix[k][t].get_block_translation();
                    anim.matrix[k][t].set_block_translation(tr - center);
                }
                anim.matrix[k][t].apply_scaling_to_block_translation(s);
            }
        }
    }

    // 5. Transform inverse bind matrices: inv_bind' = s * inv_bind
    //    This is derived from the constraint: M_global' * inv_bind' = N * M_global * inv_bind
    //    where N = [sI | -s*center] is the display-space normalization.
    //    Since M_global'^{-1} * N * M_global = s*I for all joints, inv_bind' = s * inv_bind.
    for (auto& entry : model.rigged_mesh) {
        auto& inv_bind = entry.second.controller_skinning.inverse_bind_matrices;
        for (int k = 0; k < static_cast<int>(inv_bind.size()); ++k)
            inv_bind[k].apply_scaling(s);
    }

    // 6. Rebuild skeleton drawable with normalized joint positions
    character.sk_drawable = skeleton_drawable(model.skeleton);

    return {s, center};
}
