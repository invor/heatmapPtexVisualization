#version 450
#extension GL_ARB_bindless_texture : require

struct PtexParameters
{
  int ngbr_ptex_param_indices[4]; // index into PtexParameters buffers
  int ngbr_uv_transform_cases[4]; // uv transform cases for the 4 direc neighbours

  // Save space by storing index into buffer with all texture handles instead of storing 64bit handles
  // (for texture arrays handle is shared by many faces)
  uint texture_index; // index into texture indirection buffer (where all bindless texture handles are stored)
  uint base_slice; // slice in texture array
};

// ptex parameter buffer
layout(std430, binding = 3) buffer ptexParametersBuffer { PtexParameters[] ptex_params; };

// update patches
layout(std430, binding = 6) buffer updatePatchesBuffer { uint[] update_patches; };

uniform int update_patch_offset;
uniform int texture_base_idx;
uniform uint vista_patch_cnt;

layout(local_size_x = 1, local_size_y = 1, local_size_z = 32) in;

void main()
{
  uint gID = uint(gl_GlobalInvocationID.z);

  if(gID >= vista_patch_cnt)
    return;

  // get src and tgt primtive ids
  uint tgt_primtive_idx = update_patches[update_patch_offset + gID];

  // get ptex index and slice from src primitive (i.e. the one that freed a texture tile slot)
  int layers = 2048;

  uint ptex_index = (tgt_primtive_idx) / layers;
  uint ptex_slice = (tgt_primtive_idx) - (ptex_index * layers);
  ptex_index += texture_base_idx;

  ptex_params[tgt_primtive_idx].texture_index = ptex_index;
  ptex_params[tgt_primtive_idx].base_slice = ptex_slice;
}