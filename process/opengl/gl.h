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

/** GL objects (programs, buffers, ...)
 */
typedef unsigned int gl_obj;

/** Geometry objects (mesh)
 */
typedef struct GL_Mesh {
	gl_obj vao;
	gl_obj vbo;
	gl_obj ebo;
	unsigned int drawSize;
} gl_mesh;

/** Frame buffer objects (multi-stage rendering)
 */
typedef struct GL_FrameBuffer {
	gl_obj frame;
	gl_obj texture;
} gl_fb;

/** Mesh VBO and EBO data types
 */
typedef float gl_vertex_t;
typedef unsigned int gl_index_t;

/** Shader uniform location
 */
typedef int gl_param;

/** Shader uniform data type
 */
typedef enum gl_datatype {gl_type_float, gl_type_int, gl_type_uint} gl_datatype;

/* == Window management and driver init == [Object] ========================================= */

/** Init the GL class (only one allowed). 
 * The data is processed in the orginal (full-size) resolution, but the viewer size may be smaller, 
 * this should give some performance gain because of less display manager bandwidth. 
 * @param frameSize Resolution of the frame
 * @param windowRatio View size is frameSize/windowRatio
 * @return $this(Opaque) GL class object if success, null if fail
 */
GL gl_init(size2d_t frameSize, unsigned int windowRatio);

/** Call to start a render loop, this process all GLFW window events 
 * @param this This GL class object
 * @return Width and height of the window
 */
size2d_t gl_drawStart(GL this);

/** Call this to draw a texture in viewer window 
 * To draw anything in the viewer window, first draw all the objects on a texture; 
 * then, pass that texture to this function. 
 * @param this This GL class object
 * @param texture Texture to draw
 */
void gl_drawWindow(GL this, gl_obj* texture);

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

/* == OpenGL routines == [Static] =========================================================== */

/** Load vertex and fragment shader form file, get IDs (location) of all parameters (uniform). 
 * @param shaderVertexFile Direction to the vertex shader file
 * @param shaderFragmentFile Direction to the fragment shader file
 * @param paramName An array of string represents parameters' names (uniform)
 * @param paramId Pass-by-reference: IDs (location) of the parameters
 * @param paramCount Number of parameters, same as the length (number of elements) of the paramNames and paramId
 * @return Shader if success, 0 if fail
 */
gl_obj gl_loadShader(const char* shaderVertexFile, const char* shaderFragmentFile, const char* paramName[], gl_param* paramId, const size_t paramCount);

/** Use a shader program (bind a program to current)
 * @param shader Shader to bind, previously returned by gl_loadShader(...)
 */
void gl_useShader(gl_obj* shader);

/** Pass data to shader parameters (uniform).
 * Call gl_useShader to bind the shader before set the parameters 
 * @param paramId ID of parameter returned by gl_loadShader
 * @param length Number of parameters (vecX, can be 1, 2, 3 or 4, use 1 for non-verctor params, like float or sampler), must match with shader
 * @param type Type of the data (float, int or uint), must match with shader
 * @param data Pointer to the data to be pass
 */
#define gl_setShaderParam(paramId, length, type, data) gl_setShaderParam_internal(paramId, length, type, (void*)data)
void gl_setShaderParam_internal(gl_param paramId, uint8_t length, gl_datatype type, void* data);

/** Unload a shader, the shader ID will be set to 0 
 * @param shader Shader index returned by gl_loadShader(...)
 */
void gl_unloadShader(gl_obj* shader);

/** Create and bind mesh object (geometry) 
 * @param vertexSize Size of vertices array. Height = number of vertex; width = data per vertex
 * @param indexCount Number of index in the indices array
 * @param elementsSize Size of each elements (attribute) in a vertex
 * @param vertices The vertice array
 * @param indices The indices array
 * @return GL mesh object
 */
gl_mesh gl_createMesh(size2d_t vertexSize, size_t indexCount, gl_index_t* elementsSize, gl_vertex_t* vertices, gl_index_t* indices);

/** Draw a mesh object 
 * @param mesh A mesh previously created by gl_createMesh(...)
 */
void gl_drawMesh(gl_mesh* mesh);

/** Delete a mesh object (mesh), the mesh ID will be set to 0 
 * @param mesh A mesh object previously created by gl_createMesh(...)
 */
void gl_deleteMesh(gl_mesh* mesh);

/** Create texture object 
 * @param info Video header, contains frame size and color scheme
 * @param data Pointer to the frame data
 * @return GL texture object
 */
gl_obj gl_createTexture(vh_t info, void* data);

/** Update a texture object 
 * @param texture A texture object previously created by gl_createTexture(...)
 * @param info Video header, contains frame size and color scheme
 * @param data Pointer to the frame data
 */
void gl_updateTexture(gl_obj* texture, vh_t info, void* data);

/** Bind a texture object to OpenGL engine texture unit 
 * @param texture A texture object previously created by gl_createTexture(...)
 * @param unit OpenGL texture unit, same as the GL_TEXTUREX
 */
void gl_bindTexture(gl_obj* texture, unsigned int unit);

/** Delete a texture object , the texture ID be set to 0
 * @param texture A texture object previously created by gl_createTexture(...)
 */
void gl_deleteTexture(gl_obj* texture);

/** Create a frame buffer used for multi-stage rendering 
 * @param info Video header, contains frame size and color scheme
 * @return GL frame buffer object
 */
gl_fb gl_createFrameBuffer(vh_t info);

/** Bind a frame buffer to current. 
 * To bind the default buffer (window), pass this->frame = 0. 
 * @param this A frame buffer object previously created by gl_createFrameBuffer(...)
 * @param size Set the size of view port. Leval one of width or height to 0 to skip this step
 * @param clear Set to true (non-zero value) to clear the buffer
 */
void gl_bindFrameBuffer(gl_fb* this, size2d_t size, int clear);

/** Delete a frame buffer object
 * @param this A frame buffer object previously created by gl_createFrameBuffer(...)
 */
void gl_deleteFrameBuffer(gl_fb* this);

#endif /* #ifndef INCLUDE_GL_H */