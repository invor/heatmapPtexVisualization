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

struct TextureTile{
  uint tex_index;
  uint base_slice;
};

// vertex + index data of mesh with embedded material ID and noise parameters
layout(std430, binding = 0) readonly buffer VertexBuffer { float[] vertices; };
layout(std430, binding = 1) readonly buffer IndexBuffer { uint[] indices; };

// ptex image handle buffer
//layout(std430, binding = 2) buffer ptexImagesBuffer { layout(rgba8) image2DArray[] ptex_images; };
layout(std430, binding = 2) buffer ptexImagesBuffer { uvec2[] ptex_images; };
// ptex parameter buffer
layout(std430, binding = 3) writeonly buffer ptexParametersBuffer { PtexParameters[] ptex_params; };

// update patches
layout(std430, binding = 6) readonly buffer updatePatchesBuffer { uint[] update_patches; };
// free slots
layout(std430, binding = 7) readonly buffer textureTilesBuffer { TextureTile[] texture_tiles; };


uniform float texture_lod;
uniform int update_patch_offset;
uniform int texture_slot_offset;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    ivec3 gID = ivec3(gl_GlobalInvocationID);
    
    vec2 tile_res = vec2(float(gl_NumWorkGroups.x) * 8.0, float(gl_NumWorkGroups.y) * 8.0);
    
    uint tgt_primtive_idx = update_patches[update_patch_offset + gID.z];

//    uint quad_indices[4];
//    vec3 quad_vertices[4];
//    vec3 quad_normals[4];
//    vec2 noise_params[4];
//    float material_IDs[4];
//
//    for(int i=0; i<4; ++i)
//    {
//        quad_indices[i] = indices[tgt_primtive_idx*4+i];
//        quad_vertices[i].x = vertices[ quad_indices[i] * 9 + 0];
//        quad_vertices[i].y = vertices[ quad_indices[i] * 9 + 1];
//        quad_vertices[i].z = vertices[ quad_indices[i] * 9 + 2];
//    }
//    
//    // compute texel position in world space from gID.xy and vertices
//    vec2 uv = vec2(gID.xy) / (tile_res- vec2(1.0));
//    vec3 p_0 = mix(quad_vertices[0],
//                  quad_vertices[1],
//                  uv.x);
//    vec3 p_1 = mix(quad_vertices[3],
//                  quad_vertices[2],
//                  uv.x);
//    vec3 texel_position = mix(p_0, p_1, uv.y);
//
    // get ptex index and slice from src primitive (i.e. the one that freed a texture tile slot)
    uint ptex_index = texture_tiles[texture_slot_offset + gID.z].tex_index;
    uint ptex_slice = texture_tiles[texture_slot_offset + gID.z].base_slice;
//
//    // All texture (per tile) are kept within the same Texture2DArray
//    //layout(rgba8) writeonly image2DArray ptex_image = layout(rgba8) writeonly image2DArray(ptex_images[ptex_index]); // NVIDIA
//    //image2DArray ptex_image = image2DArray(ptex_images[ptex_index]); // AMD
//    //imageStore(ptex_image,ivec3(gID.x,gID.y,ptex_slice),vec4(albedoRGB,1.0));
//
    ptex_params[tgt_primtive_idx].texture_index = ptex_index;
    ptex_params[tgt_primtive_idx].base_slice = ptex_slice;
  
}