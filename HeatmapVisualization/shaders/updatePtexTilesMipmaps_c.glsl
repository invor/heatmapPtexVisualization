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

// ptex image handle buffer
layout(std430, binding = 0) readonly buffer ptexTexturesBuffer { uvec2[] ptex_textures; };
// ptex image handle buffer
//layout(std430, binding = 1) buffer ptexMipmapImagesBuffer { layout(rgba8) image2DArray[] ptex_mipmap_images; };
layout(std430, binding = 1) readonly buffer ptexMipmapImagesBuffer { uvec2[] ptex_mipmap_images; };
// ptex parameter buffer
layout(std430, binding = 2) readonly buffer ptexParametersBuffer { PtexParameters[] ptex_params; };
// update patches
layout(std430, binding = 3) readonly buffer updatePatchesBuffer { uint[] update_patches; };

uniform int update_patch_offset;

layout(local_size_x = 4, local_size_y = 4, local_size_z = 1) in;

void main()
{

    ivec3 gID = ivec3(gl_GlobalInvocationID);

    vec2 tile_res = vec2(float(gl_NumWorkGroups.x) * 4.0, float(gl_NumWorkGroups.y) * 4.0);

    uint tgt_primtive_idx = update_patches[update_patch_offset + gID.z];

    // get ptex index and slice from src primitive (i.e. the one that freed a texture tile slot)
    uint ptex_index = ptex_params[tgt_primtive_idx].texture_index;
    uint ptex_slice = ptex_params[tgt_primtive_idx].base_slice;

    sampler2DArray ptex_texture = sampler2DArray(ptex_textures[ptex_index]);
    // read texture value from mip level 0 and average
    // access texture between texel to make use of filtering for averaging 4 texels
    vec2 uv = gID.xy * (1.0/tile_res) + 0.5/tile_res;
    vec4 colour = texture(ptex_texture,vec3(uv,float(ptex_slice)));

    //write averaged values to mip level 1
    //layout(rgba8) writeonly image2DArray ptex_image = layout(rgba8) writeonly image2DArray(ptex_mipmap_images[ptex_index]); // NVIDIA
    writeonly image2DArray ptex_image = writeonly image2DArray(ptex_mipmap_images[ptex_index]); // AMD
    imageStore(ptex_image,ivec3(gID.x,gID.y,ptex_slice),colour);
}