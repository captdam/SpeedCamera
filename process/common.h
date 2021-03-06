/** Common config and define 
 */

#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H

#include <inttypes.h>

/** Size of 2D object (image): width and height in pixel 
 */
typedef struct Size2D_t {
	union { size_t width; size_t x; };
	union { size_t height; size_t y; };
} size2d_t;

/** Size of 3D object (image): width, height and depth
 */
typedef struct Size3D_t {
	union { size_t width; size_t x; };
	union { size_t height; size_t y; };
	union { size_t depth; size_t z; };
} size3d_t;

/** Float/Int vectors (GPU types)
 */
typedef struct IntVec1 {
	union { int width; int x; int r; };
} ivec1;
typedef struct IntVec2 {
	union { int width; int x; int r; };
	union { int height; int y; int g; };
} ivec2;
typedef struct IntVec3 {
	union { int width; int x; int r; };
	union { int height; int y; int g; };
	union { int depth; int z; int b; };
} ivec3;
typedef struct IntVec4 {
	union { int width; int x; int r; };
	union { int height; int y; int g; };
	union { int depth; int z; int b; };
	union { int time; int w; int a; };
} ivec4;
typedef struct Vec1 {
	union { float width; float x; float r; };
} vec1;
typedef struct Vec2 {
	union { float width; float x; float r; };
	union { float height; float y; float g; };
} vec2;
typedef struct Vec3 {
	union { float width; float x; float r; };
	union { float height; float y; float g; };
	union { float depth; float z; float b; };
} vec3;
typedef struct Vec4 {
	union { float width; float x; float r; };
	union { float height; float y; float g; };
	union { float depth; float z; float b; };
	union { float time; float w; float a; };
} vec4;

/** Get length of an array (static allocated)
 * @param array The array
 * @return Number of elements
 */
#define arrayLength(array) ( sizeof(array) / sizeof(array[0]) )

/** Get the current machine time in nanosecond
 * @return Current time
 */
uint64_t nanotime();

/** Check if a point is inside a box. 
 * @param x Test point x-coord
 * @param y Test poiny y-coord
 * @param left Left x-coord of the box
 * @param right Right x-coord of the box
 * @param top Top y-coord of the box
 * @param bottom Bottom y-coord of the box
 * @param strict If 0, point is considered inside if on border; if positive, ptr is outside if on border; if negative, ptr is inside if on left-top border but outside if on right-bottom border (similar to 2D for loop checking)
 * @return True-equivalent (in most case 1) if point is inside; false-equivalent (0) if point is outside
 */
int inBox(const int x, const int y, const int left, const int right, const int top, const int bottom, const int strict);

#endif /* #ifndef INCLUDE_COMMON_H */