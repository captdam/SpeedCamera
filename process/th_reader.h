struct th_reader_arg {
	unsigned int size; //Number of pixels in one frame
	const char* colorScheme; //Color scheme of the input raw data (numberOfChannel[1,3or4],orderOfChannelRGBA) RGB=3012, RGBA=40123, BGR=3210
};

/** Reader thread init.
 * Prepare semaphores, launch thread. 
 * @param size Size of video in px
 * @param colorScheme A string represents the color format
 * @param statue If not NULL, return error message in case this function fail
 * @param ecode If not NULL, return error code in case this function fail
 * @return If success, return 1; if fail, release all resources and return 0
 */
int th_reader_init(const unsigned int size, const char* colorScheme, char** statue, int* ecode);

/** Call this function when the main thread issue new address for video uploading. 
 * Reader will begin data uploading after this call. 
 * @param addr Address to upload video data
 */
void th_reader_start(void* addr);

/** Call this function to block the main thread until reader thread finishing video reading. 
 */
void th_reader_wait();

/** Terminate reader thread and release associate resources
 */
void th_reader_destroy();