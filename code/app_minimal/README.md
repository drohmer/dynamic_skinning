# Minimal application — Dynamic Skinning on the CPU

The simplest possible version of dynamic skinning, on three models: the cylinder, the giraffe and the clown. There is no GPU path, no joint picking and no delay. You can only rotate the camera, choose the model, and tune the parameters of the oscillation kernel and the drag.

The algorithm is not duplicated: like the complete application, it uses `dynamic_skinning_source/` (the method) and `lib_helper/` (animated character, loaders and animations).

## Build and run

```bash
cd code/app_minimal
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/minimal        # the shaders and assets are found in code/
```

It is also built by `code/CMakeLists.txt`, together with the complete application (`code/build/minimal`).

## Files

- **`scene.cpp/.hpp`**: all the dynamic skinning of the application.
- **`environment.cpp/.hpp`**: camera and light sent to the shaders, path of the project.
- **`main.cpp`**: the generic window, callbacks and display loop of cgp.

At each frame (`scene_structure::display_frame`), in order:

1. The pose of the skeleton at the animation time gives the joint frames and their positions q_j.
2. LBS computes the positions p_i^LBS.
3. Dynamic skinning computes p_i^DS = p_i^LBS + Σ_j β_ij ψ^bend(F^osc(Ω_j), p_i^LBS − q_j).
4. The deformed mesh is displayed.

## Parameters (notations of the paper)

The oscillation kernel is κ(t) = A exp(−t/τ) sin(2π f t), with:

| Variable | Role | Default value |
|---|---|---|
| `f`   | oscillation frequency (Hz) | 1.5 |
| `tau` | damping τ (s) | 0.67 |
| `A`   | amplitude | 0.88 |

And the weight of the drag (lag proportional to the angular velocity, independent of the oscillations):

| Variable | Role | Default value |
|---|---|---|
| `D`   | drag | 0.5 |

Mouse: left drag = rotation, right drag or wheel = zoom, Ctrl + left drag = translation.
