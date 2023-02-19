/** Class - GL.class. Captdam's OpenGL helper class. 
 * Start, load, config, manage and destroy OpenGL engine. 
 * GLFW and GLEW are used as backend of this class. 
 * Only one GL class and one window is allowed. 
 * Use this class to access OpenGL engine. 
 */

#ifndef INCLUDE_GL_H
#define INCLUDE_GL_H

/* == Common ================================================================================ */

/** Init config */
typedef struct GL_Config {
	uint8_t vMajor;			//OpenGl version major
	uint8_t vMinor;			//OpenGL version minor
	uint8_t gles;			//OpenGL-ES (1) or OpenGL (0)
	uint8_t vsynch;			//Limit FPS to v-synch points
	uint16_t winWidth, winHeight;	//Window size
	char* winName;			//Window name
} gl_config;

/** Usage frequency hint for driver */
typedef enum GL_Usage {
	gl_usage_stream = 0,	//Set once then use for a few times
	gl_usage_static = 1,	//Set once, use many times: More likely saved in GPU memory
	gl_usage_dynamic = 2,	//Set and use rapidly: More likely saved in CPU memory
gl_usage_placeholderEnd} gl_usage;

/* == Window and driver management ========================================================== */

/** Cursor and window & framebuffer size */
typedef struct GL_WinsizeCursor {
	int winsize[2];		//Window size
	int framesize[2];	//Framebuffer size
	double curPos[2];	//Cursor pos in NTC
	int curPosWin[2];	//Cursor pos in px respect to window
	int curPosFrame[2];	//Cursor pos in px respect to framebuffer
} gl_winsizeNcursor;

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
#define GL_INIT_DEFAULT_PROGRAM (gl_program)0

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

/** Mesh geometry type */
typedef unsigned int gl_meshmode;
#define gl_meshmode_points		((gl_meshmode)0x0000)
#define gl_meshmode_lines		((gl_meshmode)0x0001)
#define gl_meshmode_lineLoop		((gl_meshmode)0x0002)
#define gl_meshmode_lineStrip		((gl_meshmode)0x0003)
#define gl_meshmode_triangles		((gl_meshmode)0x0004)
#define gl_meshmode_triangleStrip	((gl_meshmode)0x0005)
#define gl_meshmode_triangleFan		((gl_meshmode)0x0006)
//#define gl_meshmode_quads		((gl_meshmode)0x0007)
//#define gl_meshmode_quadStrip		((gl_meshmode)0x0008)
//#define gl_meshmode_polygon		((gl_meshmode)0x0009)

/** Geometry objects (mesh) */
typedef struct GL_Mesh {
	unsigned int vao, vbo; //Vertex array object (the mesh), vertex buffer object (vertices form the mesh)
	unsigned int ebo, ibo; //Indices buffer object (for indexed draw); Instance object bound to this mesh for instanced draw
	unsigned int drawSize; //Number of vertices or indices in the mesh
	gl_meshmode mode;
} gl_mesh;
#define GL_INIT_DEFAULT_MESH (gl_mesh){0, 0, 0, 0, 0, 0}

typedef float gl_vertex_t; //Mesh vertices
typedef unsigned int gl_index_t; //Mesh index

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
	gl_texformat_d16, gl_texformat_d24, gl_texformat_d32f,
	gl_texformat_d24s8, gl_texformat_d32fs8, gl_texformat_s8,
gl_texformat_placeholderEnd} gl_texformat;

/** Texture type */
typedef unsigned int gl_textype;
#define gl_textype_2d		((gl_textype)0)
#define gl_textype_3d		((gl_textype)1)
#define gl_textype_2dArray	((gl_textype)2)
#define gl_textype_renderBuffer	((gl_textype)10)

/** Texture object */
typedef struct GL_Texture {
	unsigned int texture;
	unsigned int width, height;
	union { unsigned int depth; unsigned int layer; };
	gl_texformat format;
	gl_textype type : 8;
	unsigned int mipmap : 1;
} gl_tex;
#define GL_INIT_DEFAULT_TEX (gl_tex){0, 0, 0, 0, 0, 0, 0}

/** Texture constructor argument */
typedef unsigned int gl_tex_dimWrapping;
#define gl_tex_dimWrapping_repeat	((gl_tex_dimWrapping)0x2901)
#define gl_tex_dimWrapping_edge		((gl_tex_dimWrapping)0x812F)
#define gl_tex_dimWrapping_mirrorRepeat	((gl_tex_dimWrapping)0x8370)

typedef unsigned int gl_tex_dimFilter;
#define gl_tex_dimFilter_nearest		((gl_tex_dimFilter)0x2600)
#define gl_tex_dimFilter_linear			((gl_tex_dimFilter)0x2601)
#define gl_tex_dimFilter_nearestMipmapNearest	((gl_tex_dimFilter)0x2700)
#define gl_tex_dimFilter_linearMipmapNearest	((gl_tex_dimFilter)0x2701)
#define gl_tex_dimFilter_nearestMipmapLinear	((gl_tex_dimFilter)0x2702)
#define gl_tex_dimFilter_linearMipmapLinear	((gl_tex_dimFilter)0x2703)

typedef struct GL_Texture_Dim {
	unsigned int size; //Size in pixel
	gl_tex_dimWrapping wrapping;
} gl_tex_dim;


/** Pixel buffer object for texture data transfer */
typedef unsigned int gl_pbo;
#define GL_INIT_DEFAULT_PBO (gl_pbo)0

/** Framebuffer object for off-screen rendering */
typedef unsigned int gl_fbo;
#define GL_INIT_DEFAULT_FBO (gl_fbo)0

/** Framebuffer attachment types */
typedef enum GL_FramebufferAttachment {
	gl_fboattach_depth = -3, gl_fboattach_stencil = -2, gl_fboattach_depth_stencil = -1,
	gl_fboattach_color0 = 0
} gl_fboattach;

/** Framebuffer clear mask */
#define gl_frameBuffer_clearColor 0x4000
#define gl_frameBuffer_clearDepth 0x0100
#define gl_frameBuffer_clearStencil 0x0400
#define gl_frameBuffer_clearAll 0x4500

/* == Window and driver management ========================================================== */

/** Init the GL class. 
 * @return 1 if success, 0 if fail
 */
int gl_init(gl_config config) __attribute__((cold));

/** Get the GLFW window object so user program can use GLFW lib to access the window. 
 * @return GLFW window object
 */
void* gl_getWindow();

/** Call to start a render loop.
 * Process all GLFW window events. 
 */
void gl_drawStart();

/** Get window and framebuffer size, get cursor position respect to window and framebuffer size respectively. 
 * @return Window and framebuffer size, cursor position
 */
gl_winsizeNcursor gl_getWinsizeCursor();

/** Set the drawing viewport. 
 * @param offset Offset in px {x,y}
 * @param size Size in px {width,height}
 */
void gl_setViewport(const unsigned int offset[static 2], const unsigned int size[static 2]);

/** Call at the end of a render loop. 
 * @param title New window title (pass NULL to use old title)
 */
void gl_drawEnd(const char* const title);

/** Set or check the window close flag. 
 * A close flag request the GL to terminate the window, but it may not be done right away. 
 * @param close Pass positive value to set the close flag, pass 0 to unset, nagative value will have no effect (only get the flag)
 * @return Close flag: 0 = close flag unset, non-zero = close falg set
 */
int gl_close(const int close);

/** Destroy the GL class, kill the viewer window and release all the GL resource. 
 */
void gl_destroy();

/** Switch to line mode
 * @param weight Set the line weight and turn on line mode; 0 to turn off line mode
 */
void gl_lineMode(const unsigned int weight);

/** Force the GL driver to sync. 
 * Calling thread will be blocked until all previous GL calls executed completely. 
 */
void gl_fsync();

/** Request the GL driver to sync. 
 * Request to empty the command quene. Calling thread will not be blocked. 
 */
void gl_rsync();

/** Set a point in the GPU command queue for a later gl_synchWait() call. 
 * @return A synch point
 */
gl_synch gl_synchSet();

/** Wait GPU command queue. 
 * @param s A synch point previously returned by gl_synchSet()
 * @param timeout Timeout in nano seconds
 * @return Synch status
 */
gl_synch_status gl_synchWait(const gl_synch s, const uint64_t timeout);

/** Delete a synch point if no longer need. 
 * @param s A synch point previously returned by gl_synchSet()
 */
void gl_synchDelete(const gl_synch s);

/* == Shader and shader param data types, UBO =============================================== */

/** Set the header of all shader program. 
 * All shaders should begin with "#version". This function is used to set this common header so it is not required exerytime a shader is created. 
 * This lib will not keep a copy of this string, so it is the application's resbonsibility to make sure this string is live when creating shader programs. 
 * @param header Pointer to the string
 */
void gl_program_setCommonHeader(const char* const header);

/** Create a shader program from file or memory. 
 * @param src A string representing the program, use "fDIR" (e.g. "f./myshader.glsl") for file, use "mSRC" (e.g. "m@VS\nlayout...") for in-memory code
 * @param args A list of gl_programArg used to return the ID of each param. The last gl_programArg should have a NULL .name to indicate the end of list
 * @return Shader program
 */
gl_program gl_program_load(char* src, gl_programArg* const args);

/** Check a shader program.
 * @param program A shader program previously returned by gl_program_load()
 * @return 1 if good, 0 if not
 */
int gl_program_check(const gl_program* const program);

/** Use a shader program (bind a shader program to current)
 * @param program A shader program previously returned by gl_program_load()
 */
void gl_program_use(const gl_program* const program);

/** Set parameter of the current shader program. 
 * Call gl_program_use() to bind the shader program before set the parameter. 
 * @param paramId ID of parameter previously returned by gl_program_load()
 * @param length Number of vector in a parameter (vecX, can be 1, 2, 3 or 4, use 1 for non-verctor params, e.g. sampler)
 * @param type Type of the data: gl_datatype_*
 * @param data Pointer to the data to be pass
 */
void gl_program_setParam(const gl_param paramId, const unsigned int length, const gl_datatype type, const void* data);

/** Delete a shader program, the shader program will be reset to GL_INIT_DEFAULT_SHADER. 
 * @param program A shader program previously returned by gl_program_load()
 */
void gl_program_delete(gl_program* const program);

/** Create a uniform buffer object (UBO) and bind it to a binding point. 
 * @param bindingPoint Binding point to bind
 * @param size Size of memory allocating to the buffer, in bytes
 * @param usage A hint to the driver about the frequency of usage, can be gl_usage_*
 * @return UBO
 */
gl_ubo gl_uniformBuffer_create(const unsigned int bindingPoint, const unsigned int size, const gl_usage usage);

/** Check an UBO
 * @param ubo An UBO previously returned by gl_uniformBuffer_create()
 * @return 1 if good, 0 if not
 */
int gl_uniformBuffer_check(const gl_ubo* const ubo);

/** Bind a shader program's block param to a uniform buffer in the binding point. 
 * @param bindingPoint Binding point to bind, same as the one used with gl_uniformBuffer_create()
 * @param program A shader program previously returned by gl_program_load()
 * @param paramId ID of parameter previously returned by gl_program_load()
 */
void gl_uniformBuffer_bindShader(const unsigned int bindingPoint, const gl_program* const program, const gl_param paramId);

/** Update a portion of uniform buffer. 
 * @param ubo An UBO previously returned by gl_uniformBuffer_create()
 * @param start Starting offset of the update in bytes
 * @param len Length of the update in bytes
 * @param data Pointer to the update data
 */
void gl_uniformBuffer_update(const gl_ubo* const ubo, const unsigned int start, const unsigned int len, const void* const data);

/** Delete an UBO. 
 * @param ubo An UBO previously created by gl_uniformBuffer_create()
 */
void gl_unifromBuffer_delete(gl_ubo* const ubo);

/* == Mesh (vertices) ======================================================================= */

/** Create and bind gl_mesh object. 
 * Each mesh has vCnt vertices, each vertex has Sigma(vStride) of numbers. 
 * The order of layout location in shader program must be vertices first, then instances, no gap between. 
 * @param vCnt Number of vertices, each vertices contains sum(vSize) gl_vertex
 * @param eCnt Number of indices (NOT face, indices/face for strip is undefined) for indexed draw or 0 for non-indexed draw, an index is an gl_index
 * @param iCnt Max number of instances or 0 for non-instanced draw, each instance contains sum(iSize) gl_vertex
 * @param mode The mode of the mesh, can be gl_meshmode_*, this affects how the GPU draw the mesh
 * @param vSize Zero terminated array, size of each attribute (number of gl_vertex_t) in a vertex
 * @param iSize Zero terminated array, size of each attribute (number of gl_vertex_t) in a instance, ignored if iBufSize is 0
 * @param vertices The vertice array, leave NULL if no actual data is ready
 * @param indices The indices array, leave NULL if no actual data is ready, ignored if eCnt is 0, use index -1 for restart in strip
 * @param shareInstance Share instance buffer with another gl_mesh object. Useful for model with multiple meshes, instead of creating new buffer, the same buffer data can be reused
 * @return Mesh
 */
gl_mesh gl_mesh_create(
	const unsigned int vCnt, const unsigned int eCnt, const unsigned int iCnt, const gl_meshmode mode,
	const gl_index_t* const vSize, const gl_index_t* const iSize,
	const gl_vertex_t* const vertices, const gl_index_t* const indices, const gl_mesh* const shareInstance
);

/** Check a mesh
 * @param mesh A mesh previously returned by gl_mesh_create()
 * @return 1 if good, 0 if not
 */
int gl_mesh_check(const gl_mesh* const mesh);

/** Update portion of a mesh's vertices array (VBO inside mesh). 
 * @param mesh  A mesh previously returned by gl_mesh_create()
 * @param vertice A pointer to the new vertices
 * @param start Offset of the update in vertices array (in unit of gl_vertex_t)
 * @param offCnt Number of vertices in the array (in unit of gl_vertex_t)
 */
void gl_mesh_updateVertices(const gl_mesh* const mesh, const gl_vertex_t* const vertices, const unsigned int start, const unsigned int len) __attribute__ ((deprecated));

/** Update portion of a mesh's indices array (EBO bind to VAO inside mesh). 
 * @param mesh  A mesh previously returned by gl_mesh_create()
 * @param indices A pointer to the new indices
 * @param start Offset of the update in indices array (in unit of gl_index_t)
 * @param offCnt Number of indices in the array (in unit of gl_index_t)
 */
void gl_mesh_updateIndices(const gl_mesh* const mesh, const gl_index_t* const indices, const unsigned int start, const unsigned int len) __attribute__ ((deprecated));

/** Update portion of a mesh's instances array (IBO inside mesh). 
 * @param mesh  A mesh previously returned by gl_mesh_create()
 * @param instances A pointer to the new instances
 * @param start Offset of the update in instances array (in unit of gl_vertex_t)
 * @param offCnt Number of instances in the array (in unit of gl_vertex_t)
 */
void gl_mesh_updateInstances(const gl_mesh* const mesh, const gl_vertex_t* const instances, const unsigned int start, const unsigned int len);

/** Draw a mesh once or for a few times. 
 * If the mesh is created with instance, that instance will be used; if without instance, only draw the mesh once.
 * @param mesh A mesh previously returned by gl_mesh_create()
 * @param vSize Number of vertices/indices used to draw, pass 0 to draw all (size when create)
 * @param iSize Number of instance to draw
 */
void gl_mesh_draw(const gl_mesh* const mesh, const unsigned int vSize, const unsigned int iSize);

/** Delete a mesh. 
 * @param mesh A mesh previously returned by gl_mesh_create()
 */
void gl_mesh_delete(gl_mesh* const mesh);

/* == Texture, PBO for texture transfer and FBO for off-screen rendering ==================== */

/** Create a texture or renderbuffer whit empty content. 
 * A render buffer is like texture, but with no content and on accessible for framebuffer. 
 * Render buffer has no sampler, so using renderbuffer in FBO is favored if texture is not required. Renderbuffer has limited format support. 
 * In favor of GLES, no 1D texture. 
 * Mipmap will be generated if minFilter request mipmap; otherwise, no mipmap is used. (magFilter cannot use mipmap). 
 * @param format Format of the texture, can be gl_texformat_*
 * @param type Texture type, can be gl_textype_*
 * @param minFilter Minify filter, can be gl_tex_dimFilter_*
 * @param magFilter Magnify filter, can be gl_tex_dimFilter_* (no mipmap)
 * @param dim Array of 3 dimensions properties, one member for each dimension (always pass 3 members! higher dimension will be ignored); For array texture, only size is used for highest dimension
 * @return Texture
 */
gl_tex gl_texture_create(const gl_texformat format, const gl_textype type, const gl_tex_dimFilter minFilter, const gl_tex_dimFilter magFilter, const gl_tex_dim dim[static 3]);

/** Check a texture. 
 * @param tex A texture previously returned by gl_texture_create()
 * @return 1 if good, 0 if not
 */
int gl_texture_check(const gl_tex* const tex);

/** Update a portion of a texture. 
 * Do NOT use on renderbuffer. 
 * Use uint8_t for X8/X8UI/S8; int8_t for X8I; uint6_t for X16UI; int16_t for X16I; uint32_t for X32UI; int32_t for X32I; f32_t for X16/X32. 
 * @param tex A texture previously returned by gl_texture_create()
 * @param data Pointer to the texture data
 * @param offset Start point of texture to be update, z-coord is ignored if texture is 2D
 * @param size Size of update, z-coord is ignored if texture is 2D
 */
void gl_texture_update(const gl_tex* const tex, const void* const data, const unsigned int offset[static 3], const unsigned int size[static 3]);

/** Bind a texture to OpenGL engine texture unit. 
 * Do NOT use on renderbuffer. 
 * @param tex A texture previously returned by gl_texture_create()
 * @param paramId ID of parameter previously returned by gl_program_load()
 * @param unit A OpenGL texture unit, a texture unit can only hold one textre at a time
 */
void gl_texture_bind(const gl_tex* const tex, const gl_param paramId, const unsigned int unit);

/** Delete a texture. 
 * @param tex A texture previously returned by gl_texture_create()
 */
void gl_texture_delete(gl_tex* const tex);

/** Transfer data from PBO to actual texture 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 * @param tex Dest gl_tex object previously created by gl_texture_create()
 */
void gl_pixelBuffer_updateToTexture(const gl_pbo* const pbo, const gl_tex* const tex);

/** Delete a PBO. 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 */
void gl_pixelBuffer_delete(gl_pbo* const pbo);

/** Create a framebuffe object (FBO) for off-screen rendering. 
 * Attach a list of texture to the frame buffer. 
 * @param count Number of texture to attach
 * @param internalBuffer A list of texture to be attached, texture type must be 2D texture or renderbuffer, at least 1 is required
 * @param type A list of attachment type used for each texture to be attached, can be gl_fboattach_* or any int >= 0 for color attachment, must be less than max number of attachment allowed by GL driver
 * @return FBO
 */
gl_fbo gl_frameBuffer_create(const unsigned int count, const gl_tex internalBuffer[static 1], const gl_fboattach type[static 1]);

/** Check a FBO
 * @param fb A FBO previously returned by gl_frameBuffer_create()
 * @return 1 if good, 0 if not
 */
int gl_frameBuffer_check(const gl_fbo* const fbo);

/** Bind a FBO to current for drawing. 
 * To bind the default buffer (display window), pass NULL. 
 * @param fbo A FBO previously returned by gl_frameBuffer_create(), or NULL for display window
 * @param clear Bit mask used to clear buffers, use gl_frameBuffer_clear*
 */
void gl_frameBuffer_bind(const gl_fbo* const fbo, const int clear);

/** Download a portion of fram buffer from GPU. 
 * @param fbo A FBO previously created by gl_frameBuffer_create()
 * @param dest Where to save the data, the memory space should be enough to hold the download content
 * @param format Format of data downloading
 * @param id Which attached texture to download
 * @param offset Start point of framebuffer to be update {x,y}
 * @param size Size of framebuffer to be download {width,height}
 */
void gl_frameBuffer_download(const gl_fbo* const fbo, void* const dest, const gl_texformat format, const unsigned int attachment, const unsigned int offset[static 2], const unsigned int size[static 2]);

/** Delete a FBO. 
 * @param fb A FBO previously returned by gl_frameBuffer_create()
 */
void gl_frameBuffer_delete(gl_fbo* const fbo);

/** Create a pixel buffer object (PBO) that is used to manually transfer texture data. 
 * @param size Size of the PBO/texture data in bytes (number _of_pixel * bytes_per_pixel)
 * @param type 0 for upload (unpack), 1 for download (pack)
 * @param usage A hint to the driver about the frequency of usage, can be gl_usage_*
 */
gl_pbo gl_pixelBuffer_create(const unsigned int size, const int type, const gl_usage usage);

/** Check a PBO. 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 * @return 1 if good, 0 if not
 */
int gl_pixelBuffer_check(const gl_pbo* const pbo);

/** Start a CPU-to-GPU transfer, obtain the pointer to the GPU memory. 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 * @param size Size of the PBO/texture data in bytes
 */
void* gl_pixelBuffer_updateStart(const gl_pbo* const pbo, const unsigned int size);

/** Finish the data transfer started by gl_pixelBuffer_updateStart(). 
 */
void gl_pixelBuffer_updateFinish();

/** Transfer data from PBO to actual texture 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 * @param tex Dest gl_tex object previously created by gl_texture_create()
 */
void gl_pixelBuffer_updateToTexture(const gl_pbo* const pbo, const gl_tex* const tex);

/** Start a GPU-to-CPU transfer, download texture of an framebuffer to PBO. 
 * No other download operation is allowed between gl_pixelBuffer_downloadStart()-gl_pixelBuffer_downloadFinish()-gl_pixelBuffer_downloadDiscard() calls. 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 * @param fbo A FBO previously returned by gl_frameBuffer_create()
 * @param format Format of data downloading
 * @param attachment Which attached texture to download (0 to MAX_ATTACHMENT), not valid for depth and stencil buffer
 * @param offset Start point of framebuffer to be update {x,y}
 * @param size Size of framebuffer to be download {width,height}
 */
void gl_pixelBuffer_downloadStart(const gl_pbo* pbo, const gl_fbo* const fbo, const gl_texformat format, const unsigned int attachment, const unsigned int offset[static 2], const unsigned int size[static 2]);

/** Obtain the data address after finishing the data transfer started by gl_pixelBuffer_updateStart(). 
 * A synch object can be used to test if the download has been finished. 
 * This function will map the internal buffer to user space, call gl_pixelBuffer_downloadDiscard() to unmap after the data has been processed. 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 * @param size Size of the PBO/texture data in bytes
 * @return Address of the downloaded data
 */
void* gl_pixelBuffer_downloadFinish(const unsigned int size);

/** Discard the pointer returned by gl_pixelBuffer_downloadFinish()
 */
void gl_pixelBuffer_downloadDiscard();

/** Delete a PBO. 
 * @param pbo A PBO previously returned by gl_pixelBuffer_create()
 */
void gl_pixelBuffer_delete(gl_pbo* const pbo);

#endif /* #ifndef INCLUDE_GL_H */