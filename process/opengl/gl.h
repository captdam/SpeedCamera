/** Class - GL.class. 
 * Start, load, config, manage and destroy OpenGL engine. 
 * OpenGL 3.1 ES is used (to support embedded device). 
 * Only one GL class and one window is allowed. 
 * Use this class to access OpenGL engine. 
 */

#ifndef INCLUDE_GL_H
#define INCLUDE_GL_H

#include "common.h"

/* == Window management and driver init == [Object] ========================================= */

/** GL class object data structure
 */
typedef struct GL_ClassDataStructure* GL;

/** Init the GL class (only one allowed). 
 * The data is processed in the orginal (full-size) resolution, but the viewer size may be smaller, 
 * this should give some performance gain because of less display manager bandwidth. 
 * @param frameSize Resolution of the frame
 * @param windowRatio View size is frameSize/windowRatio
 * @return $this(Opaque) GL class object if success, null if fail
 */
GL gl_init(size2d_t frameSize, unsigned int windowRatio);

/** Call to start a render loop 
 * @param this This GL class object
 */
void gl_drawStart(GL this);

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

/** GL objects (programs, buffers...)
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

/** Load vertex and fragment shader form file. 
 * @param shaderVertexFile Direction to the vertex shader file
 * @param shaderFragmentFile Direction to the fragment shader file
 * @return Shader if success, 0 if fail
 */
gl_obj gl_loadShader(const char* shaderVertexFile, const char* shaderFragmentFile);

/** Use a shader program (bind a program to current)
 * @param shader Shader to bind, previously returned by gl_loadShader(...)
 */
void gl_useProgram(gl_obj shader);

/** Unload a shader 
 * @param shader Shader index returned by gl_loadShader(...)
 */
void gl_unloadShader(gl_obj shader);

/** Mesh VBO and EBO data types
 */
typedef float gl_vertex_t;
typedef unsigned int gl_index_t;

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
void gl_drawMesh(gl_mesh mesh);

/** Delete a mesh object (mesh) 
 * @param mesh A mesh object previously created by gl_createMesh(...)
 */
void gl_deleteMesh(gl_mesh mesh);

/** Create texture object 
 * @param info Video header, contains frame size and color scheme
 * @param data Pointer to the frame data
 * @return AGL texture object
 */
gl_obj gl_createTexture(vh_t info, void* data);

/** Update a texture object 
 * @param texture A texture object previously created by gl_createTexture(...)
 * @param info Video header, contains frame size and color scheme
 * @param data Pointer to the frame data
 */
void gl_updateTexture(gl_obj texture, vh_t info, void* data);

/** Bind a texture object to OpenGL engine texture unit 
 * @param texture A texture object previously created by gl_createTexture(...)
 * @param unit OpenGL texture unit, same as the order of sampler in GLSL
 */
void gl_bindTexture(gl_obj texture, unsigned int unit);

/** Delete a texture object 
 * @param texture  A texture object previously created by gl_createTexture(...)
 */
void gl_deleteTexture(gl_obj texture);

#endif /* #ifndef INCLUDE_GL_H */