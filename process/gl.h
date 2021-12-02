/** Class - GL.class. 
 * Start, load, config, manage and destroy OpenGL engine. 
 * OpenGL 3.1 ES is used (to support embedded device). 
 * Only one GL class and one window is allowed. 
 * Use this class to access OpenGL engine. 
 */

#ifndef INCLUDE_GL_H
#define INCLUDE_GL_H

#include "common.h"

/** GL class object data structure
 */
typedef struct GL_ClassDataStructure* GL;

typedef enum gl_info {
	gl_info_i1_maxTextureSize, gl_info_i1_maxArrayTextureLayers, gl_info_i1_max3dTextureSize,
	gl_info_i1_maxTextureImageUnits, gl_info_i1_maxVertexTextureImageUnits,
	gl_info_s_vendor, gl_info_s_renderer, gl_info_s_version, gl_info_s_shadingLanguageVersion
} gl_info;

/** Shader and data types, UBO
 */
typedef unsigned int gl_shader;
#define GL_INIT_DEFAULT_SHADER (gl_shader)0
typedef int gl_param;
typedef enum gl_datatype {gl_type_float, gl_type_int, gl_type_uint} gl_datatype;

typedef unsigned int gl_ubo;
#define GL_INIT_DEFAULT_UBO (gl_ubo)0

/** Geometry objects (mesh)
 */
typedef unsigned int gl_vao, gl_vbo, gl_ebo;
typedef struct GL_Mesh {
	gl_vao vao;
	gl_vbo vbo;
	gl_ebo ebo;
	unsigned int drawSize;
	unsigned int mode;
} gl_mesh;
#define GL_INIT_DEFAULT_VAO (gl_vao)0
#define GL_INIT_DEFAULT_VBO (gl_vbo)0
#define GL_INIT_DEFAULT_EBO (gl_ebo)0
#define GL_INIT_DEFAULT_MESH (gl_mesh){GL_INIT_DEFAULT_VAO, GL_INIT_DEFAULT_VBO, GL_INIT_DEFAULT_EBO, 0, 0}
typedef float gl_vertex_t;
typedef unsigned int gl_index_t;
typedef enum gl_meshmode {
	gl_meshmode_points, gl_meshmode_triangles
} gl_meshmode;

/** Texture and Pixel buffer for texture transfer
 */
typedef unsigned int gl_tex;
#define GL_INIT_DEFAULT_TEX (gl_tex)0
typedef unsigned int gl_pbo;
#define GL_INIT_DEFAULT_PBO (gl_pbo)0
typedef enum gl_texformat {
	gl_texformat_R8, gl_texformat_RG8, gl_texformat_RGB8, gl_texformat_RGBA8,
	gl_texformat_R8I, gl_texformat_RG8I, gl_texformat_RGB8I, gl_texformat_RGBA8I,
	gl_texformat_R8UI, gl_texformat_RG8UI, gl_texformat_RGB8UI, gl_texformat_RGBA8UI,
	gl_texformat_R16F, gl_texformat_RG16F, gl_texformat_RGB16F, gl_texformat_RGBA16F,
	gl_texformat_R16I, gl_texformat_RG16I, gl_texformat_RGB16I, gl_texformat_RGBA16I,
	gl_texformat_R16UI, gl_texformat_RG16UI, gl_texformat_RGB16UI, gl_texformat_RGBA16UI,
	gl_texformat_R32F, gl_texformat_RG32F, gl_texformat_RGB32F, gl_texformat_RGBA32F,
	gl_texformat_R32I, gl_texformat_RG32I, gl_texformat_RGB32I, gl_texformat_RGBA32I,
	gl_texformat_R32UI, gl_texformat_RG32UI, gl_texformat_RGB32UI, gl_texformat_RGBA32UI,
gl_texformat_placeholderEnd} gl_texformat;

/** Frame buffer objects (multi-stage rendering)
 */
typedef unsigned int gl_fbo;
#define GL_INIT_DEFAULT_FBO (gl_fbo)0
typedef struct GL_FrameBuffer {
	gl_fbo frame;
	gl_tex texture;
} gl_fb;
#define GL_INIT_DEFAULT_FB (gl_fb){GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX}

/** Synch
 */
typedef void* gl_synch;
typedef enum gl_synch_statue {gl_synch_error = -1, gl_synch_timeout, gl_synch_done, gl_synch_ok} gl_synch_statue;

/* == Window management and driver init ===================================================== */

/** Init the GL class (static, only one allowed). 
 * Note: The data is processed in the orginal (full-size) resolution, but the viewer size may be smaller if using windowRation higher than 1, 
 * this should give some performance gain because of less display manager bandwidth [to-be-verified]. 
 * @param frameSize Resolution of the frame
 * @param windowRatio View size is frameSize/windowRatio
 * @param mix Intensity of orginal and processed data
 * @return 1 if success, 0 if fail
 */
int gl_init(size2d_t frameSize, unsigned int windowRatio, float mix);

/** Get driver and hardware info. 
 * The type of data can be int array, float array or string, see param enum 'name' for the type and array length. 
 * - If data type is int array or float array, the client should pass a pointer with enough memory space to param 'data'. 
 * This program will write the requested info to that location. This function also return the pointer with same address which passed into param 'data'. 
 * - If type is str, this function will ignor the param 'data'. A pointer to a static string will be returned. 
 * @param name What info you want? Can be gl_info_*_*. This also indicates the info data type (int array, float array or string) and length (of array)
 * @param data The returned info, pass-by-reference
 * @return Pointer to the returned data
 */
void* gl_getInfo(gl_info name, void* data);

/** Call to start a render loop, this process all GLFW window events. 
 * @param cursorPos Current cursor position relative to the window, pass-by-reference
 */
void gl_drawStart(size2d_t* cursorPos);

/** Call this to draw a texture in viewer window. 
 * To draw anything in the viewer window, first draw all the objects on a framebuffer; 
 * then, pass that framebuffer's texture to this function. 
 * @param orginalTexture Orginal texture from camera
 * @param processedTexture Processed data
 */
void gl_drawWindow(gl_tex* orginalTexture, gl_tex* processedTexture);

/** Call to end a render loop. 
 * @param this This GL class object
 * @param title Window title (pass NULL to use old title)
 */
void gl_drawEnd(const char* title);

/** Set and check the close flag. 
 * @param close Pass positive value to set the close flag, pass 0 to unset, nagative value will have no effect (only get the flag)
 * @return Close flag: 0 = close flag unset, non-zero = close falg set
 */
int gl_close(int close);

/** Destroy the GL class, kill the viewer window. 
 */
void gl_destroy();

/* == OpenGL routines ======================================================================= */

/** Load vertex and fragment shader, get IDs (location) of all parameters (uniform). 
 * @param shaderVertex Vertex shader or path to vertex shader file
 * @param shaderFragment Fragment shader or path to fragment shader file
 * @param shaderGeometry Optional geometry shader or path to geometry shader file, leave NULL to skip
 * @param isFilePath If 0, shaderVertex and shaderFragment are pointer to the source code. If 1, they are path to the shader file
 * @param paramName An array of string represents shader parameters' names (uniform)
 * @param paramId Pass-by-reference: IDs (location) of the parameters
 * @param paramCount Number of parameters, same as the length (number of elements) of the paramName and paramId
 * @param blockName An array of string represents shader blocks' names (uniform interface)
 * @param blockId Pass-by-reference: IDs (index) of the blocks
 * @param blockCount Number of blocks, same as the length (number of elements) of the blockName and blockId
 * @return gl_shader object upon success, GL_INIT_DEFAULT_SHADER if fail
 */
gl_shader gl_shader_load(
	const char* shaderVertex, const char* shaderFragment, const char* shaderGeometry, const int isFilePath,
	const char* paramName[], gl_param* paramId, const unsigned int paramCount,
	const char* blockName[], gl_param* blockId, const unsigned int blockCount
);

/** Check a shader
 * @param shader Shader to check, previously returned by gl_shader_load()
 * @return 1 if good, 0 if not
 */
int gl_shader_check(gl_shader* shader);

/** Use a shader (bind a shader to current)
 * @param shader Shader to bind, previously returned by gl_shader_load()
 */
void gl_shader_use(gl_shader* shader);

/** Pass data to shader parameters (uniform). 
 * Call gl_useShader() to bind the shader before set the parameters. 
 * @param paramId ID of parameter returned by gl_shader_load()
 * @param length Number of parameters (vecX, can be 1, 2, 3 or 4, use 1 for non-verctor params, like float or sampler), must match with shader
 * @param type Type of the data (gl_type_float, gl_type_int or gl_type_uint), must match with shader
 * @param data Pointer to the data to be pass
 */
#define gl_shader_setParam(paramId, length, type, data) gl_shader_setParam_internal(paramId, length, type, (void*)data)
void gl_shader_setParam_internal(gl_param paramId, uint8_t length, gl_datatype type, void* data);

/** Unload a shader, the shader will be reset to GL_INIT_DEFAULT_SHADER. 
 * @param shader Shader name returned by gl_shader_load()
 */
void gl_shader_unload(gl_shader* shader);

/** Create a uniform buffer and bind it to a binding point. 
 * @param bindingPoint Binding point to bind
 * @param size Size of memory allocating to the buffer, in bytes
 * @return GL uniform buffer object
 */
gl_ubo gl_uniformBuffer_create(unsigned int bindingPoint, size_t size);

/** Check an uniform buffer
 * @param ubo A gl_ubo previously created by gl_uniformBuffer_create()
 * @return 1 if good, 0 if not
 */
int gl_uniformBuffer_check(gl_ubo* ubo);

/** Bind a shader's block to a uniform buffer in the binding point. 
 * @param bindingPoint Binding point to bind
 * @param shader Shader name returned by gl_shader_load()
 * @param blockId ID of the block in the shader  returned by gl_shader_load()
 */
void gl_uniformBuffer_bindShader(unsigned int bindingPoint, gl_shader* shader, gl_param blockId);

/** Update data in a uniform buffer. 
 * @param ubo A gl_ubo previously created by gl_uniformBuffer_create()
 * @param start Starting offset of the update in bytes
 * @param len Length of the update in bytes
 * @param data Pointer to the data to write into UBO
 */
#define gl_uniformBuffer_update(ubo, start, len, data) gl_uniformBuffer_update_internal(ubo, start, len, (void*)data)
void gl_uniformBuffer_update_internal(gl_ubo* ubo, size_t start, size_t len, void* data);

/** Delete a uniform buffer (UBO). 
 * @param ubo A gl_ubo previously created by gl_uniformBuffer_create()
 */
void gl_unifromBuffer_delete(gl_ubo* ubo);

/** Create and bind gl_mesh object (geometry). 
 * @param vertexSize Size of vertices array. Height = number of vertex; width = data per vertex
 * @param indexCount Number of index in the indices array
 * @param elementsSize Array, size of each elements (attribute) in a vertex
 * @param vertices The vertice array
 * @param indices The indices array
 * @param mode The mode of the mesh, can be gl_meshmode_*, this affects how the GPU draw the mesh
 * @return gl_mesh object
 */
gl_mesh gl_mesh_create(size2d_t vertexSize, size_t indexCount, gl_index_t* elementsSize, gl_vertex_t* vertices, gl_index_t* indices, gl_meshmode mode);

/** Check an uniform buffer
 * @param mesh A gl_mesh object previously created by gl_mesh_create()
 * @return 1 if good, 0 if not
 */
int gl_mesh_check(gl_mesh* mesh);

/** Draw a gl_mesh object. 
 * If mesh is NULL, will use default mesh, which covers the entire window ({0,0} to {1,1})
 * @param mesh A gl_mesh object previously created by gl_mesh_create() or NULL
 */
void gl_mesh_draw(gl_mesh* mesh);

/** Delete a gl_mesh object (geometry), the gl_mesh ID will be set to GL_INIT_DEFAULT_MESH. 
 * @param mesh A gl_mesh object previously created by gl_mesh_create()
 */
void gl_mesh_delete(gl_mesh* mesh);

/** Create 2D gl_tex object whit empty content 
 * @param format Format of the texture, can be gl_texformat_*
 * @param size Width and height of the gl_tex in unit of pixel
 * @return gl_tex object
 */
gl_tex gl_texture_create(gl_texformat format, size2d_t size);

/** Create 2D array gl_tex object with empty content
 * @param format Format of the texture, can be gl_texformat_*
 * @param size Width and height of the gl_tex in unit of pixel, number of texture as depth/z
 * @return gl_tex object
 */
gl_tex gl_textureArray_create(gl_texformat format, size3d_t size);

/** Check a texture (2D or 2D array) 
 * @param texture A gl_mesh object previously created by gl_mesh_create()
 * @return 1 if good, 0 if not
 */
int gl_texture_check(gl_tex* texture);

/** Update a 2D gl_tex object 
 * @param format Format of the texture, can be gl_texformat_*, need to be same as it when create
 * @param texture A 2D gl_tex object previously created by gl_texture_create()
 * @param size Width and height of the texture in unit of pixel
 * @param data Pointer to the texture data
 */
void gl_texture_update(gl_texformat format, gl_tex* texture, size2d_t size, void* data);

/** Update a 2D array gl_tex object, one texture at a time 
 * @param format Format of the texture, can be gl_texformat_*, need to be same as it when create
 * @param texture A 2D array gl_tex object previously created by gl_textureArray_create()
 * @param size Width and height are the size of texture in unit of pixel, depth is which texture in the array to be update
 * @param data Pointer to the texture data
 */
void gl_textureArray_update(gl_texformat format, gl_tex* texture, size3d_t size, void* data);

/** Bind a 2D gl_tex object to OpenGL engine texture unit 
 * @param texture A gl_tex object previously created by gl_texture_create()
 * @param paramId ID of texture parameter returned by gl_shader_load()
 * @param unit OpenGL texture unit, same as the GL_TEXTUREX
 */
void gl_texture_bind(gl_tex* texture, gl_param paramId, unsigned int unit);

/** Bind a 2D gl_tex object to OpenGL engine texture unit 
 * @param texture A gl_tex object previously created by gl_texture_create()
 * @param paramId ID of texture parameter returned by gl_shader_load()
 * @param unit OpenGL texture unit, same as the GL_TEXTUREX
 */
void gl_textureArray_bind(gl_tex* texture, gl_param paramId, unsigned int unit);

/** Delete a gl_tex object (2D or 2D array), the texture ID will be set to GL_INIT_DEFAULT_TEX
 * @param texture A gl_tex object previously created by gl_texture_create()
 */
void gl_texture_delete(gl_tex* texture);

/** Create a gl_pbo that used to manually transfer data to openGL texture 
 * @param size Size of the PBO/texture data in bytes (pixel * bytes_of_one_pixel)
 */
gl_pbo gl_pixelBuffer_create(size_t size);

/** Check a pixel buffer
 * @param pbo A gl_pbo previously created by gl_pixelBuffer_create()
 * @return 1 if good, 0 if not
 */
int gl_pixelBuffer_check(gl_pbo* pbo);

/** Start a transfer by obtain the pointer to the GPU memory
 * @param pbo A gl_pbo previously created by gl_pixelBuffer_create()
 * @param size Size of the PBO/texture data in bytes (pixel * bytes_of_one_pixel)
 */
void* gl_pixelBuffer_updateStart(gl_pbo* pbo, size_t size);

/** Finish the data transfer started by gl_pixelBuffer_updateStart()
 */
void gl_pixelBuffer_updateFinish();

/** Delete a gl_pbo. The PBO ID will be set the GL_INIT_DEFAULT_PBO. 
 * @param pbo A gl_pbo previously created by gl_pixelBuffer_create()
 */
void gl_pixelBuffer_delete(gl_pbo* pbo);

/** Transfer data from PBO to actual texture 
 * @param format Format of the texture, can be gl_texformat_*, need to be same as it when create
 * @param pbo A gl_pbo previously created by gl_pixelBuffer_create()
 * @param texture Dest gl_tex object previously created by gl_texture_createRGB()
 * @param size Width and height of the texture in unit of pixel
 */
void gl_pixelBuffer_updateToTexture(gl_texformat format, gl_pbo* pbo, gl_tex* texture, size2d_t size);

/** Create a frame buffer used for multi-stage rendering 
 * @param format Format of the texture, can be gl_texformat_*
 * @param size Width and height of the texture in unit of pixel
 * @return GL frame buffer object
 */
gl_fb gl_frameBuffer_create(gl_texformat format, size2d_t size);

/** Check a frame buffer
 * @param fb A frame buffer previously created by gl_frameBuffer_create()
 * @return 1 if good, 0 if not
 */
int gl_frameBuffer_check(gl_fb* fb);

/** Bind a frame buffer to current. 
 * To bind the default buffer (window), pass this with child frame = 0. 
 * @param fb A frame buffer previously created by gl_frameBuffer_create()
 * @param size Set the size of view port. Pass {0,0} to skip this step
 * @param clear Set to true (non-zero value) to clear the buffer, use 0 to skip
 */
void gl_frameBuffer_bind(gl_fb* fb, size2d_t size, int clear);

/** Download texture of frambuffer from GPU, formet is always RGBA-vec4 (using float32, 16 bytes/pixel). 
 * @param fb A frame buffer previously created by gl_frameBuffer_create()
 * @param size The size of the texture, must be the same as when it created
 * @param dest Where to save the texture, must be atleast size.width * size.height * 4 * sizeof(float)
 */
void gl_frameBuffer_download(gl_fb* fb, size2d_t size, void* dest);

/** Get the pixel of the framebuffer's texture at location where
 * @param fb A frame buffer previously created by gl_frameBuffer_create()
 * @param where X- and y- coord of the pixel
 * @return The pixel value
 */
vec4 gl_frameBuffer_getPixel(gl_fb* fb, size2d_t where);

/** Delete a frame buffer. The framebuffer ID will be set to GL_INIT_DEFAULT_FB. 
 * @param fb A frame buffer previously created by gl_frameBuffer_create()
 */
void gl_frameBuffer_delete(gl_fb* fb);

/** Force the GL driver to sync. 
 * Calling thread will be blocked until all previous GL calls executed completely. 
 */
void gl_fsync();

/** Request the GL driver to sync. 
 * Empty the command buffer. Calling thread will not be blocked. 
 */
void gl_rsync();

/** Set a point in the GPU command queue for a later gl_synchWait() call. 
 * @return A gl_synch object
 */
gl_synch gl_synchSet();

/** Wait GPU command queue. 
 * @param s A gl_synch object previous returned by gl_synchSet()
 * @param timeout Timeout in nano seconds. This functionn will return gl_synch_timeout if the GPU commands before the synch point cannot be preformed before the timeout. This value can be 0
 * @return gl_synch_ok or gl_synch_done if the all commands before the synch point in the GPU queue are done before this call or before the timeout; gl_synch_timeout if timeout passed bu command has not done; gl_synch_error if any error
 */
gl_synch_statue gl_synchWait(gl_synch s, uint64_t timeout);

/** Delete a gl_synch object if no longer need
 * @param s A gl_synch object previous returned by gl_synchSet()
 */
void gl_synchDelete(gl_synch s);

#endif /* #ifndef INCLUDE_GL_H */