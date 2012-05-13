/* Copyright © 2012 euphoria GmbH
 * Author: Lukas Martini <lutoma@phoria.eu>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <wand/MagickWand.h>

#include "global.h"
#include "resize.h"

int resize_image(char* path, char* req_path, size_t size, char* mode)
{
	// Create new image
	MagickWand* magick_wand = NewMagickWand();
	if((uintptr_t)magick_wand < 1)
		error("Could not initialize ImageMagick (magick_wand is NULL)", EXIT_FAILURE);

	// Read the Image
	bool status = MagickReadImage(magick_wand, path);
	if (status == false)
		return -1;

	size_t width = MagickGetImageWidth(magick_wand);
	size_t height = MagickGetImageHeight(magick_wand);

	// If the size is bigger than both height and width, redirect to original
	if(size >= width)
		return 1;

	size_t new_height = size;
	size_t new_width = size;

	// Unless a square image is requested, calculcate proper aspect ratio
	if(size != 1 && strcmp(mode, "square"))
	{
		new_height = height * size / width;
	}

	/* Turn the image into a thumbnail sequence (for animated GIFs)
	 * Automatically switches to a less cpu intense filter for animated GIFs.
	 */
	MagickResetIterator(magick_wand);
	for(int i = 0; MagickNextImage(magick_wand) != false && i < 250; i++)
		MagickResizeImage(magick_wand, new_width, new_height, !i ? LanczosFilter : TriangleFilter, 1);

	// Write the image
	status = MagickWriteImages(magick_wand, req_path, true);
	if (status == MagickFalse)
		error("Could not write image", EXIT_FAILURE)

	DestroyMagickWand(magick_wand);
	return 0;
}