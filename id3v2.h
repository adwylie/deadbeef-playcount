#ifndef PLAYCOUNT_ID3V2_H_
#define PLAYCOUNT_ID3V2_H_

/**
 * Create/allocate a new PCNT frame object on the heap.
 *
 * The counter must be at least 32-bits long to begin with (uint32_t), and
 * increases by one byte when an overflow would otherwise occur.
 *
 * ID3v2 specification uses Big Endian (network byte order) for storage of
 * multi-byte data, whereas host machine may not.
 *
 * Format is the same for both ID3v2.3 and 2.4. Documentation available at:
 *   - http://id3.org/id3v2.3.0
 *   - http://id3.org/id3v2.4.0-structure
 *   - http://id3.org/id3v2.4.0-frames
 *
 * @return  The created frame.
 */
DB_id3v2_frame_t *id3v2_frame_pcnt_create();

/**
 * Increment the play count value of an existing PCNT frame.
 *
 * Do it the easy way, don't care about values larger than 32-bits (uint32_t).
 *
 * @param frame  A pointer to the PCNT frame.
 * @return  Return non-zero if an error occurred, zero otherwise.
 */
uint8_t id3v2_frame_pcnt_inc(DB_id3v2_frame_t *frame);

/**
 * Reset the play count value of an existing PCNT frame.
 *
 * @param frame  A pointer to the PCNT frame.
 * @return  Return non-zero if an error occurred, zero otherwise.
 */
uint8_t id3v2_frame_pcnt_reset(DB_id3v2_frame_t *frame);

/**
 * Add a frame to an ID3v2 tag.
 *
 * @param tag  A pointer to an ID3v2 tag.
 * @param frame  A pointer to an ID3v2 frame.
 */
void id3v2_tag_frame_add(DB_id3v2_tag_t *tag, DB_id3v2_frame_t *frame);

/**
 * Find the PCNT frame in an ID3v2 tag.
 *
 * @param tag  A pointer to an ID3v2 tag.
 * @return  A pointer to the PCNT frame, or NULL if the frame was not found.
 */
DB_id3v2_frame_t *id3v2_tag_frame_get_pcnt(DB_id3v2_tag_t *tag);


/**
 * Remove the PCNT frame from an ID3v2 tag.
 *
 * @param tag  A pointer to an ID3v2 tag.
 * @return  A pointer to the removed frame, or NULL if the frame was not found.
 */
DB_id3v2_frame_t *id3v2_tag_frame_rem_pcnt(DB_id3v2_tag_t *tag);

#endif //PLAYCOUNT_ID3V2_H_
