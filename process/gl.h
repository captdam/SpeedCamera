/** Class - GL.class. 
 * Start, load, config, manage and destroy OpenGL engine. 
 * OpenGL 3.1 ES is used (to support embedded device). 
 * Only one GL class and one window is allowed. 
 * Use this class to access OpenGL engine. 
 */

#ifndef INCLUDE_GL_H
#define INCLUDE_GL_H

/* == Common ================================================================================ */

/** Usage frequency hint for driver */
typedef enum GL_Usage {
	gl_usage_stream = 0,	//Set once then use for a few times
	gl_usage_static = 1,	//Set once, use many times: More likely saved in GPU memory
	gl_usage_dynamic = 2,	//Set and use rapidly: More likely saved in CPU memory
gl_usage_placeholderEnd} gl_usage;

/* == Window and driver management ========================================================== */

typedef void* gl_synch; //Synch point
/** Synch status */
typedef enum GL_Synch_Status {
	gl_synch_error = -1,
	gl_synch_timeout = 0,	//Command cannot be finished before timeout
	gl_synch_done = 1,	//Command already finished before this call
	gl_synch_ok = 2		//Command finished before timeout
} gl_synch_status; //Synch status

/* == Shader and shader param data types, UBO =============================================== */

typedef unsigned int gl_program; //Shader program
#define GL_INIT_DEFAULT_PROGRAM (gl_program) 0

typedef int gl_param; //Shader program argument handler
/** Shader program argument data type*/
typedef enum GL_DataType {
	gl_datatype_float = 0,	//32-bit float
	gl_datatype_int = 1,	//32-bit int
	gl_datatype_uint = 2,	//32-bit unsigned int
gl_datatype_placeholderEnd } gl_datatype;
typedef unsigned int gl_ubo; //Uniform buffer object
#define GL_INIT_DEFAULT_UBO (gl_ubo)0

/** Shader types */
typedef enum GL_ProgramSourceCodeType {
	gl_programSrcType_vertex = 0,		//Vertex shader
	gl_programSrcType_fragment = 1,		//Fragment shader
	gl_programSrcType_geometry = 2,		//Geometry shader (optional)
gl_programSrcType_placeholderEnd } gl_programSrcType;
/** Shader code source */
typedef enum GL_ProgramSourceCodeLocation {
	gl_programSrcLoc_mem = 0,	//Source code content saved in memory
	gl_programSrcLoc_file = 1,	//Source code content saved in file
gl_programSrcLoc_placeholderEnd } gl_programSrcLoc;
/** Shader source code attributes */
typedef struct GL_ProgramSourceCode {
	const gl_programSrcType type;	//Shader type: gl_programSrcType_*
	const gl_programSrcLoc loc;	//Source code location: gl_programSrcLoc_*
	const char* str;		//Source code content or the source code file directory
} gl_programSrc;

/** Shader program argument types */
typedef enum GL_ProgramArgType {
	gl_programArgType_normal = 0,	//Uniform
	gl_programArgType_ubo = 1,	//UBO
gl_programArgType_placeholderEnd } gl_programArgType;
/** OpenGL shader program argument (to get the ID of uniform or UBO) */
typedef struct GL_ProgramArg {
	const gl_programArgType type;	//Argument type: gl_programArgType_*
	const char* name;		//Name of uniform/UBO in shader code
	gl_param id;			//Pass-by-reference, the id will be returned back here
} gl_programArg;

/* == Mesh (vertices) ======================================================================= */

/** Geometry objects (mesh) */
typedef struct GL_Mesh {
	unsigned int vao, vbo, ebo;
	unsigned int drawSize;
	unsigned int mode;
} gl_mesh;
#define GL_INIT_DEFAULT_MESH (gl_mesh){0, 0, 0, 0, 0}

typedef float gl_vertex_t; //Mesh vertices
typedef unsigned int gl_index_t; //Mesh index

/** Mesh geometry type */
typedef enum GL_MeshMode {
	gl_meshmode_points, gl_meshmode_triangles, gl_meshmode_triangleFan, gl_meshmode_triangleStrip,
gl_meshmode_placeholderEnd} gl_meshmode;

/* == Texture, PBO for texture transfer and FBO for off-screen rendering ==================== */

/** Texture data format */
typedef enum GL_TexFormat {
	gl_texformat_R8 = 0, gl_texformat_RG8, gl_texformat_RGB8, gl_texformat_RGBA8,
	gl_texformat_R8I, gl_texformat_RG8I, gl_texformat_RGB8I, gl_texformat_RGBA8I,
	gl_texformat_R8UI, gl_texformat_RG8UI, gl_texformat_RGB8UI, gl_texformat_RGBA8UI,
	gl_texformat_R16F, gl_texformat_RG16F, gl_texformat_RGB16F, gl_texformat_RGBA16F,
	gl_texformat_R16I, gl_texformat_RG16I, gl_texformat_RGB16I, gl_texformat_RGBA16I,
	gl_texformat_R16UI, gl_texformat_RG16UI, gl_texformat_RGB16UI, gl_texformat_RGBA16UI,
	gl_texformat_R32F, gl_texformat_RG32F, gl_texformat_RGB32F, gl_texformat_RGBA32F,
	gl_texformat_R32I, gl_texformat_RG32I, gl_texformat_RGB32I, gl_texformat_RGBA32I,
	gl_texformat_R32UI, gl_texformat_RG32UI, gl_texformat_RGB32UI, gl_texformat_RGBA32UI,
gl_texformat_placeholderEnd} gl_texformat;

/** Texture type */
typedef enum GL_TexType {
	gl_textype_2d, gl_textype_3d, gl_textype_2dArray,
gl_textype_placeholderEnd} gl_textype;

/* Texture object */
typedef struct GL_Texture {
	unsigned int texture;
	unsigned int width, height;
	union { unsigned int depth; unsigned int layer; };
	gl_texformat format;
	gl_textype type;
} gl_tex;
#define GL_INIT_DEFAULT_TEX (gl_tex){0, 0, 0, 0, 0, 0}

/** Pixel buffer object for texture data transfer */
typedef unsigned int gl_pbo;
#define GL_INIT_DEFAULT_PBO (gl_pbo)0

/** Framebuffer object for off-screen rendering */
typedef unsigned int gl_fbo;
#define GL_INIT_DEFAULT_FBO (gl_fbo)0

/* == Window and driver management ========================================================== */

/** Set log and error log stream. 
 * @param stream Log stream
 */
void gl_logStream(FILE* stream);

/** Write to log or error log stream. 
 * @param format A C printf-style format string
 * @param ... A list of arguments
 */
int gl_logWrite(const char* format, ...);

/** Print log to log stream. 
 * @param format A C printf-style format string
 * @param ... A list of arguments
 */
#define gl_log(format, ...) gl_logWrite("[GL] Log:\t"format"\n" __VA_OPT__(,) __VA_ARGS__)

/** Print error log to error log stream. 
 * @param format A C printf-style format string
 * @param ... A list of arguments
 */
#define gl_elog(format, ...) gl_logWrite("[GL] Err:\t"format"\n" __VA_OPT__(,) __VA_ARGS__)

/** Init the GL class. 
 * @return 1 if success, 0 if fail
 */
int gl_init() __attribute__((cold));

/** Call to start a render loop, this process all GLFW window events. 
 * @param cursorPos Pointer to 2 double, used to return cursor position relative to the window {x,y}
 * @param windowSize Pointer to 2 int, used to return size of the window (width,height)
 * @param cursorPos Pointer to 2 double, used to return size of the GPU framebuffer {width,height}
 */
void gl_drawStart(double cursorPos[static 2], int windowSize[static 2], int framebufferSize[static 2]);

/** Set the drawing viewport
 * @param offset Offset in px {x,y}
 * @param size Size in px {width,height}
 */
void gl_setViewport(const unsigned int offset[static 2], const unsigned int size[static 2]);

/** Call at the end of a render loop. 
 * @param title New window title (pass NULL to use old title)
 */
void gl_drawEnd(const char* title);

/** Set or check the window close flag. 
 * A close flag request the GL to terminate the window, but it may not be done right away. 
 * @param close Pass positive value to set the close flag, pass 0 to unset, nagative value will have no effect (only get the flag)
 * @return Close flag: 0 = close flag unset, non-zero = close falg set
 */
int gl_close(int close);

/** Destroy the GL class, kill the viewer window and release all the GL resource. 
 */
void gl_destroy();

/** Switch to line mode
 * @param weight Set the line weight and turn on line mode; 0 to turn off line mode
 */
void gl_lineMode(unsigned int weight);

/** Force the GL driver to sync. 
 * Calling thread will be blocked until all previous GL calls executed completely. 
 */
void gl_fsync();

/** Request the GL driver to sync. 
 * Request to empty the command quene. Calling thread will not be blocked. 
 */
void gl_rsync();

/** Set a point in the GPU command queue for a later gl_synchWait() call. 
 * No effect on platform that does not support synch barrier. 
 * @return A synch point
 */
gl_synch (*gl_synchSet)();

/** Wait GPU command queue. 
 * Equivalent to gl_fsynch() on platform that does not support synch barrier. 
 * @param s A synch point previously returned by gl_synchSet()
 * @param timeout Timeout in nano seconds
 * @return Synch status
 */
gl_synch_status (*gl_synchWait)(gl_synch s, uint64_t timeout);

/** Delete a synch point if no longer need. 
 * No effect on platform that does not support synch barrier. 
 * @param s A synch point previously returned by gl_synchSet()
 */
void (*gl_synchDelete)(gl_synch s);

/* == Shader and shader param data types, UBO =============================================== */

/** Set the header of all shader program. 
 * All shaders should begin with "#version". This function is used to set this common header so it is not required exerytime a shader is created. 
 * This lib will not keep a copy of this string, so it is the application's resbonsibility to make sure this string is live when creating shader programs. 
 * @param header Pointer to the string
 */
void gl_program_setCommonHeader(const char* header);

/** Create a shader program. 
 * @param src A list of gl_programSrc indicates the source code. The last gl_programSrc should have a NULL .src to indicate the end of list
 * @param args A list of gl_programArg used to return the ID of each param. The last gl_programArg should have a NULL .name to indicate the end of list
 * @return Shader program
 */
gl_program gl_program_create(const gl_programSrc* srcs, gl_programArg* args);

/** Check a shader program.
 * @param program A shader program previously returned by gl_program_create()
 * @return 1 if good, 0 if not
 */
int gl_program_check(gl_program* program);

/** Use a shader program (bind a shader program to current)
 * @param program A shader program previously returned by gl_program_create()
 */
void gl_program_use(gl_program* program);

/** Set parameter of the current shader program. 
 * Call gl_program_use() to bind the shader program before set the parameter. 
 * @param paramId ID of parameter previously returned by gl_program_create()
 * @param length Number of vector in a parameter (vecX, can be 1, 2, 3 or 4, use 1 for non-verctor params, e.g. sampler)
 * @param type Type of the data: gl_datatype_*
 * @param data Pointer to the data to be pass
 */
void gl_program_setParam(const gl_param paramId, const unsigned int length, const gl_datatype type, const void* data);

/** Delete a shader program, the shader program will be reset to GL_INIT_DEFAULT_SHADER. 
 * @param program A shader program previously returned by gl_program_create()
 */
void gl_program_delete(gl_program* program);

/** Create a uniform buffer object (UBO) and bind it to a binding point. 
 * @param bindingPoint Binding point to bind
 * @param size Size of memory allocating to the buffer, in bytes
 * @param usage A hint to the driver about the frequency of usage, can be gl_usage_*
 * @return UBO
 */
gl_ubo gl_uniformBuffer_create(unsigned int bindingPoint, unsigned int size, gl_usage usage);

/** Check an UBO
 * @param ubo An UBO previously returned by gl_uniformBuffer_create()
 * @return 1 if good, 0 if not
 */
int gl_uniformBuffer_check(gl_ubo* ubo);

/** Bind a shader program's block param to a uniform buffer in the binding point. 
 * @param bindingPoint Binding point to bind, same as the one used with gl_uniformBuffer_create()
 * @param program A shader program previously returned by gl_program_create()
 * @param paramId ID of parameter previously returned by gl_program_create()
 */
void gl_uniformBuffer_bindShader(unsigned int bindingPoint, gl_program* program, gl_param paramId);

/** Update a portion of uniform buffer. 
 * @param ubo An UBO previously returned by gl_uniformBuffer_create()
 * @param start Starting offset of the update in bytes
 * @param len Length of the update in bytes
 * @param data Pointer to the update data
 */
void gl_uniformBuffer_update(gl_ubo* ubo, unsigned int start, unsigned int len, void* data);

/** Delete an UBO. 
 * @param ubo An UBO previously created by gl_uniformBuffer_create()
 */
void gl_unifromBuffer_delete(gl_ubo* ubo);

/* == Mesh (vertices) ======================================================================= */

/** Create and bind gl_mesh object (geometry). 
 * @param count {number of points in one vertex, number of vertices, number of indices for indexed draw or 0 for direct draw}
 * @param elementsSize Array, size of each elements (attribute, GLSL location) in a vertex. Add all attributes should equal to points in vertex
 * @param vertices The vertice array
 * @param indices For indexed draw: the indices array; for direct draw, use NULL
 * @param mode The mode of the mesh, can be gl_meshmode_*, this affects how the GPU draw the mesh
 * @param usage A hint to the driver about the frequency of usage, can be gl_usage_*
 * @return Mesh
 */
gl_mesh gl_mesh_create(const unsigned int count[static 3], gl_index_t* elementsSize, gl_vertex_t* vertices, gl_index_t* indices, gl_meshmode mode, gl_usage usage);

/** Check a mesh
 * @param mesh A mesh previously returned by gl_mesh_create()
 * @return 1 if good, 0 if not
 */
int gl_mesh_check(gl_mesh* mesh);

/** Update portion of a mesh. 
 * This function update the VBO (if not NULL, and offCnt[1] is not 0) and EBO (if not NULL, and offCnt[3] is not 0, and used when created) of the mesh. 
 * @param mesh  A mesh previously returned by gl_mesh_create()
 * @param vertice A pointer to the new vertices
 * @param indices A pointer to the new indices
 * @param offCnt {Offset of the update in vertices array, Number of vertices in the array, Offset of the update in indices array, Number of indices in the array}
 */
void gl_mesh_update(gl_mesh* mesh, gl_vertex_t* vertices, gl_index_t* indices, const unsigned int offCnt[static 4]);

/** Draw a mesh. 
 * @param mesh A mesh previously returned by gl_mesh_create()
 */
void gl_mesh_draw(gl_mesh* mesh);

/** Delete a mesh. 
 * @param mesh A mesh previously returned by gl_mesh_create()
 */
void gl_mesh_delete(gl_mesh* mesh);

/* == Texture, PBO for texture transfer and FBO for off-screen rendering ==================== */

/** Create a texture whit empty content. 
 * @param format Format of the texture, can be gl_texformat_*
 * @param type Texture type, can be gl_textype_*
 * @param size Width, height and depth of texture, depth is ignored for 2D texture
 * @return Texture
 */
gl_tex gl_texture_create(gl_texformat format, gl_textype type, const unsigned int size[static 3]);

/** Check a texture. 
 * @param tex A texture previously returned by gl_texture_create()
 * @return 1 if good, 0 if not
 */
int gl_texture_check(gl_tex* tex);

/** Update a portion of a texture
 * @param tex A texture previously returned by gl_texture_create()
 * @param data Pointer to the texture data
 * @param offset Start point of texture to be update, z-coord is ignored if texture is 2D
 * @param size Size of update, z-coord is ignored if texture is 2D
 */
void gl_texture_update(gl_tex* tex, void* data, const unsigned int offset[static 3], const unsigned int size[static 3]);

/** Bind a texture to OpenGL engine texture unit 
 * @param tex A texture previously returned by gl_texture_create()
 * @param paramId ID of parameter previously returned by gl_program_create()
 * @param unit A OpenGL texture unit, a texture unit can only hold one textre at a time
 */
void gl_texture_bind(gl_tex* tex, gl_param paramId, unsigned int unit);

/** Delete a texture
 * @param tex A texture previously returned by gl_texture_create()
 */
void gl_texture_delete(gl_tex* tex);

/** Create a pixel buffer object (PBO) that is used to manually transfer texture data. 
 * @param size Size of the PBO/texture data in bytes (number _of_pixel * bytes_per_pixel)
 * @param usage A hint to the driver about the frequency of usage, can be gl_usage_*
 */
gl_pbo gl_pixelBuffer_create(unsigned int size, gl_usage usage);

/** Check a PBO. 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 * @return 1 if good, 0 if not
 */
int gl_pixelBuffer_check(gl_pbo* pbo);

/** Start a CPU-to-GPU transfer by obtain the pointer to the GPU memory. 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 * @param size Size of the PBO/texture data in bytes
 */
void* gl_pixelBuffer_updateStart(gl_pbo* pbo, unsigned int size);

/** Finish the data transfer started by gl_pixelBuffer_updateStart(). 
 */
void gl_pixelBuffer_updateFinish();

/** Transfer data from PBO to actual texture 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 * @param tex Dest gl_tex object previously created by gl_texture_create()
 */
void gl_pixelBuffer_updateToTexture(gl_pbo* pbo, gl_tex* tex);

/** Delete a PBO. 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 */
void gl_pixelBuffer_delete(gl_pbo* pbo);

/** Create a framebuffe object (FBO) for off-screen rendering. 
 * Attach a list of texture to the frame buffer. 
 * @param internalBuffer A list of texture to be attached, texture type must be 2D texture, at least 1 is required
 * @param count Number of texture to attach
 * @return FBO
 */
gl_fbo gl_frameBuffer_create(const gl_tex internalBuffer[static 1], unsigned int count);

/** Check a FBO
 * @param fb A FBO previously returned by gl_frameBuffer_create()
 * @return 1 if good, 0 if not
 */
int gl_frameBuffer_check(gl_fbo* fbo);

/** Bind a FBO to current for drawing. 
 * To bind the default buffer (display window), pass NULL. 
 * @param fbo A FBO previously returned by gl_frameBuffer_create(), or NULL for display window
 * @param clear Use non-zero value to erase the content before drawing, use 0 to draw on exist content
 */
void gl_frameBuffer_bind(gl_fbo* fbo, int clear);

/** Download a portion of fram buffer from GPU. 
 * @param fbo A FBO previously created by gl_frameBuffer_create()
 * @param dest Where to save the data, the memory space should be enough to hold the download content
 * @param format Format of data downloading
 * @param id Which attached texture to download
 * @param offset Start point of framebuffer to be update {x,y}
 * @param size Size of framebuffer to be download {width,height}
 */
void gl_frameBuffer_download(gl_fbo* fbo, void* dest, gl_texformat format, const unsigned int attachment, const unsigned int offset[static 2], const unsigned int size[static 2]);

/** Delete a FBO. 
 * @param fb A FBO previously returned by gl_frameBuffer_create()
 */
void gl_frameBuffer_delete(gl_fbo* fbo);

#endif /* #ifndef INCLUDE_GL_H */