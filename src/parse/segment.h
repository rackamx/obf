/**
 * @file segment.h
 * @brief Source segmentation: code versus preprocessor lines.
 */

#ifndef CFLATTEN_SEGMENT_H
#define CFLATTEN_SEGMENT_H

#include <stddef.h>

/**
 * @brief Source segment.
 */
typedef struct {
	int is_preproc; /**< Non-zero for preprocessor lines. */
	char *text;	/**< Segment text with newlines (owned). */
} Seg;

/**
 * @brief Source segment vector.
 */
typedef struct {
	Seg *items; /**< Segments. */
	size_t len; /**< Item count. */
	size_t cap; /**< Allocated slots. */
} SegVec;

void segvec_push(SegVec *v, int is_pre, char *t);

SegVec split_preproc(const char *src);

#endif /* CFLATTEN_SEGMENT_H */
