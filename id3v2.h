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
 * Also, there is no fixed order of a frame's appearance in the tag. So we'll
 * just throw the PCNT frame at the end of the tag if we need to create it.
 *
 * Format is the same for both ID3v2.3 and 2.4. Documentation available at:
 *   - http://id3.org/id3v2.3.0
 *   - http://id3.org/id3v2.4.0-structure
 *   - http://id3.org/id3v2.4.0-frames
 *
 * @return  A pointer to the created frame.
 */
DB_id3v2_frame_t *id3v2_create_pcnt_frame(void);

/**
 * Increment the play count value of an existing PCNT frame.
 *
 * A new frame will be allocated when the play count value is too large to be
 * stored in the current frame. It is up to the caller to test if this has
 * occurred and act accordingly.
 *
 * @param frame  A pointer to the PCNT frame.
 * @return  A pointer to the updated frame.
 */
DB_id3v2_frame_t *id3v2_pcnt_frame_inc_count(DB_id3v2_frame_t *frame);

/**
 * Get the play count value of an existing PCNT frame.
 *
 * @param frame  A pointer to the PCNT frame.
 * @return  The play count.
 */
uintmax_t id3v2_pcnt_frame_get_count(DB_id3v2_frame_t *frame);

/**
 * Set the play count value of an existing PCNT frame.
 *
 * @param frame  A pointer to the PCNT frame.
 * @param count  The play count to set.
 * @return  A pointer to the updated frame.
 */
DB_id3v2_frame_t *id3v2_pcnt_frame_set_count(DB_id3v2_frame_t *frame, uintmax_t count);

/**
 * Add a frame to an ID3v2 tag.
 *
 * @param tag  A pointer to an ID3v2 tag.
 * @param frame  A pointer to an ID3v2 frame.
 */
void id3v2_tag_add_frame(DB_id3v2_tag_t *tag, DB_id3v2_frame_t *frame);

/**
 * Find the PCNT frame in an ID3v2 tag.
 *
 * @param tag  A pointer to an ID3v2 tag.
 * @return  A pointer to the PCNT frame, or NULL if the frame was not found.
 */
DB_id3v2_frame_t *id3v2_tag_get_pcnt_frame(DB_id3v2_tag_t *tag);


/**
 * Remove the PCNT frame from an ID3v2 tag.
 *
 * @param tag  A pointer to an ID3v2 tag.
 * @return  A pointer to the removed frame, or NULL if the frame was not found.
 */
DB_id3v2_frame_t *id3v2_tag_rem_pcnt_frame(DB_id3v2_tag_t *tag);

#endif //PLAYCOUNT_ID3V2_H_
