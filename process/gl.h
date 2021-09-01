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
} gl_mesh;
#define GL_INIT_DEFAULT_VAO (gl_vao)0
#define GL_INIT_DEFAULT_VBO (gl_vbo)0
#define GL_INIT_DEFAULT_EBO (gl_ebo)0
#define GL_INIT_DEFAULT_MESH (gl_mesh){GL_INIT_DEFAULT_VAO, GL_INIT_DEFAULT_VBO, GL_INIT_DEFAULT_EBO, 0}
typedef float gl_vertex_t;
typedef unsigned int gl_index_t;

/** Texture and Pixel buffer for texture transfer
 */
typedef unsigned int gl_tex;
#define GL_INIT_DEFAULT_TEX (gl_tex)0
typedef unsigned int gl_pbo;
#define GL_INIT_DEFAULT_PBO (gl_pbo)0
typedef enum gl_texformat {gl_texformat_R8, gl_texformat_RG8, gl_texformat_RGB8, gl_texformat_RGBA8} gl_texformat;

/** Frame buffer objects (multi-stage rendering)
 */
typedef unsigned int gl_fbo;
#define GL_INIT_DEFAULT_FBO (gl_fbo)0
typedef struct GL_FrameBuffer {
	gl_fbo frame;
	gl_tex texture;
} gl_fb;
#define GL_INIT_DEFAULT_FB (gl_fb){GL_INIT_DEFAULT_FBO, GL_INIT_DEFAULT_TEX}

/* == Window management and driver init ===================================================== */

/** Init the GL class (only one allowed). 
 * The data is processed in the orginal (full-size) resolution, but the viewer size may be smaller, 
 * this should give some performance gain because of less display manager bandwidth [to-be-verified]. 
 * @param frameSize Resolution of the frame
 * @param windowRatio View size is frameSize/windowRatio
 * @param mix Intensity of orginal and processed data
 * @return $this(Opaque) GL class object if success, null if fail
 */
GL gl_init(size2d_t frameSize, unsigned int windowRatio, float mix);

/** Call to start a render loop, this process all GLFW window events 
 * @param this This GL class object
 */
void gl_drawStart(GL this);

/** Call this to draw a texture in viewer window 
 * To draw anything in the viewer window, first draw all the objects on a texture (framebuffer.texture); 
 * then, pass that texture to this function. 
 * @param this This GL class object
 * @param orginalTexture Orginal texture from camera
 * @param processedTexture Processed data
 */
void gl_drawWindow(GL this, gl_tex* orginalTexture, gl_tex* processedTexture);

/** Call to end a render loop 
 * @param this This GL class object
 * @param title Window title (pass NULL to use old title)
 */
void gl_drawEnd(GL this, const char* title);

/** Set and check the close flag
 * @param this This GL class object
 * @param close Pass positive value to set the close flag, pass 0 to unset, nagative value will have no effect (only get the flag)
 * @return Close flag: 0 = close flag unset, non-zero = close falg set
 */
int gl_close(GL this, int close);

/** Destroy the GL class, kill the viewer window
 * @param this This GL class object
 */
void gl_destroy(GL this);

/* == OpenGL routines ======================================================================= */

/** Load vertex and fragment shader, get IDs (location) of all parameters (uniform). 
 * @param shaderVertex Vertex shader or path to vertex shader file
 * @param shaderFragment Fragment shader or path to fragment shader file
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
	const char* shaderVertex, const char* shaderFragment, const int isFilePath,
	const char* paramName[], gl_param* paramId, const unsigned int paramCount,
	const char* blockName[], gl_param* blockId, const unsigned int blockCount
);

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
 * @return gl_mesh object
 */
gl_mesh gl_mesh_create(size2d_t vertexSize, size_t indexCount, gl_index_t* elementsSize, gl_vertex_t* vertices, gl_index_t* indices);

/** Draw a gl_mesh object. 
 * @param mesh A gl_mesh object previously created by gl_mesh_create()
 */
void gl_mesh_draw(gl_mesh* mesh);

/** Delete a gl_mesh object (geometry), the gl_mesh ID will be set to GL_INIT_DEFAULT_MESH. 
 * @param mesh A gl_mesh object previously created by gl_mesh_create()
 */
void gl_mesh_delete(gl_mesh* mesh);

/** Create gl_tex object whit empty content 
 * @param format Format of the texture, can be gl_texformat_R8, gl_texformat_RG8, gl_texformat_RGB8 or gl_texformat_RGBA8
 * @param size Width and height of the gl_tex in unit of pixel
 * @return gl_tex object
 */
gl_tex gl_texture_create(gl_texformat format, size2d_t size);

/** Update a gl_tex object 
 * @param format Format of the texture, can be gl_texformat_R8, gl_texformat_RG8, gl_texformat_RGB8 or gl_texformat_RGBA8, need to be same as it when create
 * @param texture A gl_tex object previously created by gl_texture_create()
 * @param size Width and height of the texture in unit of pixel
 * @param data Pointer to the texture data
 */
void gl_texture_update(gl_texformat format, gl_tex* texture, size2d_t size, void* data);

/** Bind a texture object to OpenGL engine texture unit 
 * @param texture A gl_tex object previously created by gl_texture_create()
 * @param paramId ID of texture parameter returned by gl_shader_load()
 * @param unit OpenGL texture unit, same as the GL_TEXTUREX
 */
void gl_texture_bind(gl_tex* texture, gl_param paramId, unsigned int unit);

/** Delete a gl_tex object, the texture ID be set to GL_INIT_DEFAULT_TEX
 * @param texture A gl_tex object previously created by gl_texture_create()
 */
void gl_texture_delete(gl_tex* texture);

/** Create a gl_pbo that used to manually transfer data to openGL texture 
 * @param size Size of the PBO/texture data in bytes (pixel * bytes_of_one_pixel)
 */
gl_pbo gl_pixelBuffer_create(size_t size);

/** Start a transfer by obtain the pointer to the GPU memory
 * @param pbo A gl_pbo previously created by gl_pixelBuffer_create()
 * @param size Size of the PBO/texture data in bytes (pixel * bytes_of_one_pixel)
 */
void* gl_pixelBuffer_updateStart(gl_pbo* pbo, size_t size);

/** Finish the data transfer started by gl_pixelBuffer_updateStart()
 */
void gl_pixelBuffer_updateFinish();

/** Delete a gl_pbo
 * @param pbo A gl_pbo previously created by gl_pixelBuffer_create()
 */
void gl_pixelBuffer_delete(gl_pbo* pbo);

/** Transfer data from PBO to actual texture 
 * @param format Format of the texture, can be gl_texformat_R8, gl_texformat_RG8, gl_texformat_RGB8 or gl_texformat_RGBA8, need to be same as it when create
 * @param pbo A gl_pbo previously created by gl_pixelBuffer_create()
 * @param texture Dest gl_tex object previously created by gl_texture_createRGB()
 * @param size Width and height of the texture in unit of pixel
 */
void gl_pixelBuffer_updateToTexture(gl_texformat format, gl_pbo* pbo, gl_tex* texture, size2d_t size);

/** Create a frame buffer used for multi-stage rendering 
 * @param format Format of the texture, can be gl_texformat_R8, gl_texformat_RG8, gl_texformat_RGB8 or gl_texformat_RGBA8
 * @param size Width and height of the texture in unit of pixel
 * @return GL frame buffer object
 */
gl_fb gl_frameBuffer_create(gl_texformat format, size2d_t size);

/** Bind a frame buffer to current. 
 * To bind the default buffer (window), pass this with child frame = 0. 
 * @param fb A frame buffer object previously created by gl_frameBuffer_create()
 * @param size Set the size of view port. Pass {0,0} to skip this step
 * @param clear Set to true (non-zero value) to clear the buffer, use 0 to skip
 */
void gl_frameBuffer_bind(gl_fb* fb, size2d_t size, int clear);

/** Delete a frame buffer object. 
 * @param fb A frame buffer object previously created by gl_frameBuffer_create()
 */
void gl_frameBuffer_delete(gl_fb* fb);

/** Force the GL driver to sync, the frame buffer will be reset to GL_INIT_DEFAULT_FB. 
 * Calling thread will be blocked until all previous GL calls executed completely.
 */
void gl_fsync();

/** Request the GL driver to sync. 
 * Empty the command buffer. Calling thread will not be blocked.
 */
void gl_rsync();

#endif /* #ifndef INCLUDE_GL_H */