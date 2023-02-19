#include <stdint.h>

typedef struct Output_Data {
	float rx, ry; //Road- and screen-domain coord of current frame
	uint16_t sx, sy;
	int16_t speed; //Speed (avged by shader)
	int16_t osy; //Change in y-coord 
} output_data;

typedef struct Output_Header {
	int frame; //Frame number
	int count; //Number of data for current frame, use -1 to terminate thread
} output_header;

/** Output thread init.
 * Prepare communication pipe, launch thread. 
 * @return If success, return 1; if fail, release all resources and return 0
 */
int th_output_init();

/** Pass data to output. 
 * @param frame Frame number
 * @param count Number of data, use -1 to terminate
 * @param data An array of @param count data objects
 */
void th_output_write(const int frame, const int count, output_data* data);

/** Terminate Output thread and release associate resources. 
 */
void th_output_destroy();