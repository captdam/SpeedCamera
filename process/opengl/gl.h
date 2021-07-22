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

/** Shader and data types
 */
typedef unsigned int gl_shader;
#define GL_INIT_DEFAULT_SHADER (gl_shader)0
typedef int gl_param;
typedef enum gl_datatype {gl_type_float, gl_type_int, gl_type_uint} gl_datatype;

/** Geometry objects (mesh)
 */
typedef unsigned int gl_vao, gl_xbo;
typedef struct GL_Mesh {
	gl_vao vao;
	gl_xbo vbo;
	gl_xbo ebo;
	unsigned int drawSize;
} gl_mesh;
#define GL_INIT_DEFAULT_VAO (gl_vao)0
#define GL_INIT_DEFAULT_XBO (gl_xbo)0
#define GL_INIT_DEFAULT_MESH (gl_mesh){GL_INIT_DEFAULT_VAO, GL_INIT_DEFAULT_XBO, GL_INIT_DEFAULT_XBO, 0}
typedef float gl_vertex_t;
typedef unsigned int gl_index_t;

/** Texture
 */
typedef unsigned int gl_tex;
#define GL_INIT_DEFAULT_TEX (gl_tex)0

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
 * @return $this(Opaque) GL class object if success, null if fail
 */
GL gl_init(size2d_t frameSize, unsigned int windowRatio);

/** Call to start a render loop, this process all GLFW window events 
 * @param this This GL class object
 */
void gl_drawStart(GL this);

/** Call this to draw a texture in viewer window 
 * To draw anything in the viewer window, first draw all the objects on a texture (framebuffer.texture); 
 * then, pass that texture to this function. 
 * @param this This GL class object
 * @param texture Texture to draw
 */
void gl_drawWindow(GL this, gl_tex* texture);

/** Call to end a render loop 
 * @param this This GL class object
 * @param title Window title (pass NULL to use old title)
 * @return Nano seconds used to render this frame
 */
uint64_t gl_drawEnd(GL this, const char* title);

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
 * @param paramName An array of string represents shader parameters' names (uniform)
 * @param paramId Pass-by-reference: IDs (location) of the parameters
 * @param paramCount Number of parameters, same as the length (number of elements) of the paramNames and paramId
 * @param isFilePath If 0, shaderVertex and shaderFragment are pointer to the source code. If 1, they are path to the shader file
 * @return gl_shader object upon success, GL_INIT_DEFAULT_SHADER if fail
 */
gl_shader gl_loadShader(char* shaderVertex, char* shaderFragment, char* paramName[], gl_param* paramId, const unsigned int paramCount, const int isFilePath);

/** Use a shader (bind a shader to current)
 * @param shader Shader to bind, previously returned by gl_loadShader()
 */
void gl_useShader(gl_shader* shader);

/** Pass data to shader parameters (uniform). 
 * Call gl_useShader() to bind the shader before set the parameters. 
 * @param paramId ID of parameter returned by gl_loadShader
 * @param length Number of parameters (vecX, can be 1, 2, 3 or 4, use 1 for non-verctor params, like float or sampler), must match with shader
 * @param type Type of the data (gl_type_float, gl_type_int or gl_type_uint), must match with shader
 * @param data Pointer to the data to be pass
 */
#define gl_setShaderParam(paramId, length, type, data) gl_setShaderParam_internal(paramId, length, type, (void*)data)
void gl_setShaderParam_internal(gl_param paramId, uint8_t length, gl_datatype type, void* data);

/** Unload a shader, the shader will be reset to GL_INIT_DEFAULT_SHADER. 
 * @param shader Shader index returned by gl_loadShader()
 */
void gl_unloadShader(gl_shader* shader);

/** Create and bind gl_mesh object (geometry). 
 * @param vertexSize Size of vertices array. Height = number of vertex; width = data per vertex
 * @param indexCount Number of index in the indices array
 * @param elementsSize Array, size of each elements (attribute) in a vertex
 * @param vertices The vertice array
 * @param indices The indices array
 * @return gl_mesh object
 */
gl_mesh gl_createMesh(size2d_t vertexSize, size_t indexCount, gl_index_t* elementsSize, gl_vertex_t* vertices, gl_index_t* indices);

/** Draw a gl_mesh object. 
 * @param mesh A gl_mesh object previously created by gl_createMesh()
 */
void gl_drawMesh(gl_mesh* mesh);

/** Delete a gl_mesh object (geometry), the gl_mesh ID will be set to GL_INIT_DEFAULT_MESH. 
 * @param mesh A gl_mesh object previously created by gl_createMesh()
 */
void gl_deleteMesh(gl_mesh* mesh);

/** Create gl_tex object whit empty content, this texture only has one 8-bit channel (R8). 
 * @param size Width and height of the gl_tex in unit of pixel
 * @return gl_tex object
 */
gl_tex gl_createTexture(size2d_t size);

/** Update a gl_tex object 
 * @param texture A gl_tex object previously created by gl_createTexture()
 * @param size Width and height of the texture in unit of pixel
 * @param data Pointer to the texture data
 */
void gl_updateTexture(gl_tex* texture, size2d_t size, void* data);

/** Bind a texture object to OpenGL engine texture unit 
 * @param texture A gl_tex object previously created by gl_createTexture()
 * @param unit OpenGL texture unit, same as the GL_TEXTUREX
 */
void gl_bindTexture(gl_tex* texture, unsigned int unit);

/** Delete a gl_tex object, the texture ID be set to GL_INIT_DEFAULT_TEX
 * @param texture A gl_tex object previously created by gl_createTexture()
 */
void gl_deleteTexture(gl_tex* texture);

/** Create a frame buffer used for multi-stage rendering 
 * @param size Width and height of the texture in unit of pixel
 * @return GL frame buffer object
 */
gl_fb gl_createFrameBuffer(size2d_t size);

/** Bind a frame buffer to current. 
 * To bind the default buffer (window), pass this->frame = 0. 
 * @param fb A frame buffer object previously created by gl_createFrameBuffer(...)
 * @param size Set the size of view port. Pass {0,0} to skip this step
 * @param clear Set to true (non-zero value) to clear the buffer, use 0 to skip
 */
void gl_bindFrameBuffer(gl_fb* fb, size2d_t size, int clear);

/** Delete a frame buffer object. 
 * @param fb A frame buffer object previously created by gl_createFrameBuffer(...)
 */
void gl_deleteFrameBuffer(gl_fb* fb);

/** Force the GL driver to sync, the frame buffer will be reset to GL_INIT_DEFAULT_FB. 
 * Calling thread will be blocked until all previous GL calls executed completely.
 */
void gl_fsync();

/** Request the GL driver to sync. 
 * Empty the command buffer. Calling thread will not be blocked.
 */
void gl_rsync();

#endif /* #ifndef INCLUDE_GL_H */