/*
 *  GQView
 *  (C) 2005 John Ellis
 *
 *  Authors:
 *    Raw NEF jpeg extraction based on nefextract.c by Joseph Heled,
 *        in addition nefextract.c is based on dcraw by Dave Coffin.
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "intl.h"

#include "format_nikon.h"

#include "exif.h"


/*
 *-----------------------------------------------------------------------------
 * Raw NEF embedded jpeg extraction for Nikon
 *-----------------------------------------------------------------------------
 */

static guint nikon_tiff_table(unsigned char *data, const guint len, guint offset, ExifByteOrder bo,
			      gint level,
			      guint *image_offset, guint *jpeg_len);


static void nikon_tiff_entry(unsigned char *data, const guint len, guint offset, ExifByteOrder bo,
			     gint level,
			     guint *image_offset, guint *image_length, guint *jpeg_start, guint *jpeg_len)
{
	guint tag;
	guint type;
	guint count;
	guint segment;
	guint seg_len;

	tag = exif_byte_get_int16(data + offset + EXIF_TIFD_OFFSET_TAG, bo);
	type = exif_byte_get_int16(data + offset + EXIF_TIFD_OFFSET_FORMAT, bo);
	count = exif_byte_get_int32(data + offset + EXIF_TIFD_OFFSET_COUNT, bo);

	/* so far, we only care about tags with type long */
	if (type != EXIF_FORMAT_LONG_UNSIGNED && type != EXIF_FORMAT_LONG) return;

	seg_len = ExifFormatList[type].size * count;
	if (seg_len > 4)
		{
		segment = exif_byte_get_int32(data + offset + EXIF_TIFD_OFFSET_DATA, bo);
		if (segment + seg_len > len) return;
		}
	else
		{
		segment = offset + EXIF_TIFD_OFFSET_DATA;
		}

	if (tag == 0x14a)
		{
		/* sub IFD table */
		gint i;

		for (i = 0; i < count; i++)
			{
			guint subset;

			subset = exif_byte_get_int32(data + segment + i * 4, bo);
			nikon_tiff_table(data, len, subset, bo, level + 1, image_offset, image_length);
			}

		}
	else if (tag == 0x201)
		{
		/* jpeg data start offset */
		*jpeg_start = exif_byte_get_int32(data + segment, bo);
		}
	else if (tag == 0x202)
		{
		/* jpeg data length */
		*jpeg_len = exif_byte_get_int32(data + segment, bo);
		}
}

static guint nikon_tiff_table(unsigned char *data, const guint len, guint offset, ExifByteOrder bo,
			      gint level,
			      guint *image_offset, guint *image_length)
{
	guint count;
	guint i;
	guint jpeg_start = 0;
	guint jpeg_len = 0;

	/* limit damage from infinite loops */
	if (level > EXIF_TIFF_MAX_LEVELS) return 0;

	if (len < offset + 2) return FALSE;

	count = exif_byte_get_int16(data + offset, bo);

	if (len < offset + count * EXIF_TIFD_SIZE + 4) return 0;
	offset += 2;

	for (i = 0; i < count; i++)
		{
		nikon_tiff_entry(data, len, offset + i * EXIF_TIFD_SIZE, bo, level,
				 image_offset, image_length, &jpeg_start, &jpeg_len);
		}

	if (jpeg_start > 0 &&
	    jpeg_len > *image_length)
		{
		*image_offset = jpeg_start;
		*image_length = jpeg_len;
		}

	return exif_byte_get_int32(data + offset + count * EXIF_TIFD_SIZE, bo);
}

gint format_nikon_raw(unsigned char *data, const guint len,
		      guint *image_offset, guint *exif_offset)
{
	guint i_off = 0;
	guint i_len = 0;
	ExifByteOrder bo;
	guint offset;
	gint level;

	if (!exif_tiff_directory_offset(data, len, &offset, &bo)) return FALSE;

	level = 0;
	while (offset && level < EXIF_TIFF_MAX_LEVELS)
		{
		offset = nikon_tiff_table(data, len, offset, bo, 0, &i_off, &i_len);
		level++;
		}

	if (i_off != 0)
		{
		if (image_offset) *image_offset = i_off;
		return TRUE;
		}

	return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 * EXIF Makernote for Nikon
 *-----------------------------------------------------------------------------
 */

static ExifTextList NikonTagQuality[]= {
	{ 1,	"VGA basic" },
	{ 2,	"VGA normal" },
	{ 3,	"VGA fine" },
	{ 4,	"SXGA basic" },
	{ 5,	"SXGA normal" },
	{ 6,	"SXGA fine" },
	{ 7,	"XGA basic (?)" },
	{ 8,	"XGA normal (?)" },
	{ 9,	"XGA fine (?)" },
	{ 10,	"UXGA basic" },
	{ 11,	"UXGA normal" },
	{ 12,	"UXGA fine" },
	EXIF_TEXT_LIST_END
};

static ExifTextList NikonTagColorMode[]= {
	{ 1,	"color" },
	{ 2,	"monochrome" },
	EXIF_TEXT_LIST_END
};

static ExifTextList NikonTagImgAdjust[]= {
	{ 0,	"normal" },
	{ 1,	"bright+" },
	{ 2,	"bright-" },
	{ 3,	"contrast+" },
	{ 4,	"contrast-" },
	EXIF_TEXT_LIST_END
};

static ExifTextList NikonTagISOSensitivity[]= {
	{ 0,	"80" },
	{ 2,	"160" },
	{ 4,	"320" },
	{ 5,	"100" },
	EXIF_TEXT_LIST_END
};

static ExifTextList NikonTagWhiteBalance[]= {
	{ 0,	"auto" },
	{ 1,	"preset" },
	{ 2,	"daylight" },
	{ 3,	"incandescent" },
	{ 4,	"fluorescence" },
	{ 5,	"cloudy" },
	{ 6,	"speedlight" },
	EXIF_TEXT_LIST_END
};

static ExifTextList NikonTagConverter[]= {
	{ 0,	"none" },
	{ 1,	"Fisheye" },
	EXIF_TEXT_LIST_END
};

#if 0
static ExifTextList NikonTag[]= {
	{ ,	"" },
	{ ,	"" },
	EXIF_TEXT_LIST_END
};
#endif

static ExifMarker NikonExifMarkersList1[] = {
{ 0x0002, EXIF_FORMAT_STRING, 6,		"MkN.Nikon.unknown",	NULL,		NULL },
{ 0x0003, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.Quality",	"Quality",	NikonTagQuality },
{ 0x0004, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.ColorMode",	"Color mode",	NikonTagColorMode },
{ 0x0005, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.ImageAdjustment",
								"Image adjustment",	NikonTagImgAdjust },
{ 0x0006, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.ISOSensitivity",
								"ISO sensitivity",	NikonTagISOSensitivity },
{ 0x0007, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.WhiteBalance",
								"White balance",	NikonTagWhiteBalance },
{ 0x0008, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"MkN.Nikon.Focus",	"Focus",	NULL },
{ 0x000a, EXIF_FORMAT_RATIONAL_UNSIGNED, 1,	"MkN.Nikon.DigitalZoom","Digital zoom", NULL },
{ 0x000b, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.Converter",	"Converter",	NikonTagConverter },
EXIF_MARKER_LIST_END
};

static ExifTextList NikonTag2FlashComp[]= {
	{ 0x06,	"+1.0 EV" },
	{ 0x04,	"+0.7 EV" },
	{ 0x03,	"+0.5 EV" },
	{ 0x02,	"+0.3 EV" },
	{ 0x00,	"0.0 EV" },
	{ 0xfe,	"-0.3 EV" },
	{ 0xfd,	"-0.5 EV" },
	{ 0xfc,	"-0.7 EV" },
	{ 0xfa,	"-1.0 EV" },
	{ 0xf8,	"-1.3 EV" },
	{ 0xf7,	"-1.5 EV" },
	{ 0xf6,	"-1.7 EV" },
	{ 0xf4,	"-2.0 EV" },
	{ 0xf2,	"-2.3 EV" },
	{ 0xf1,	"-2.5 EV" },
	{ 0xf0,	"-2.7 EV" },
	{ 0xee,	"-3.0 EV" },
	EXIF_TEXT_LIST_END
};

static ExifTextList NikonTag2FlashUsed[]= {
	{ 0,	"no" },
	{ 9,	"yes" },
	EXIF_TEXT_LIST_END
};

#if 0
static ExifTextList NikonTagi2Saturation[]= {
	{ -3,	"black and white" },
	{ -2,	"-2" },
	{ -1,	"-1" },
	{ 0,	"normal" },
	{ 1,	"+1" },
	{ 2,	"+2" },
	EXIF_TEXT_LIST_END
};
#endif

static ExifMarker NikonExifMarkersList2[] = {
{ 0x0002, EXIF_FORMAT_SHORT_UNSIGNED, 2,	"MkN.Nikon.ISOSpeed",	"ISO speed",	NULL },
{ 0x0003, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.ColorMode",	"Color mode",	NULL },
{ 0x0004, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.Quality",	"Quality",	NULL },
{ 0x0005, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.WhiteBalance",
								"White balance",	NULL },
{ 0x0006, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.Sharpening",	"Sharpening",	NULL },
{ 0x0007, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.FocusMode",	"Focus mode",	NULL },
{ 0x0008, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.FlashSetting",
								"Flash setting",	NULL },
{ 0x0009, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.AutoFlashMode","Auto flash mode",NULL },
{ 0x000b, EXIF_FORMAT_SHORT, 1,			"MkN.Nikon.WhiteBalanceBias",
							"White balance bias value",	NULL },
/* { 0x000c, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.WhiteBalanceCoeff",
						"White balance red/blue coefficents",	NULL }, */
/* { 0x000f, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.ISOSelect",	"ISO selection",NULL }, */
{ 0x0012, EXIF_FORMAT_UNDEFINED, 4,		"MkN.Nikon.FlashCompensation",
								"Flash compensation",	NikonTag2FlashComp },
{ 0x0013, EXIF_FORMAT_SHORT_UNSIGNED, 2,	"MkN.Nikon.ISOSpeedRequest",
								"ISO speed requested",	NULL },
{ 0x0016, EXIF_FORMAT_SHORT_UNSIGNED, 4,	"MkN.Nikon.CornerCoord",
								"Corner coordinates",	NULL },
{ 0x0018, EXIF_FORMAT_UNDEFINED, 4,		"MkN.Nikon.FlashBracketCompensation",
							"Flash bracket compensation",	NikonTag2FlashComp },
{ 0x0019, EXIF_FORMAT_RATIONAL, 1,		"MkN.Nikon.AEBracketCompensation",
							"AE bracket compensation",	NULL },
{ 0x0080, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.ImageAdjustment",
								"Image adjustment",	NULL },
{ 0x0081, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.Contrast",	"Contrast",	NULL },
{ 0x0082, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.AuxLens","Aux lens adapter", NULL },
{ 0x0083, EXIF_FORMAT_BYTE_UNSIGNED, -1,	"MkN.Nikon.LensType",	"Lens type",	NULL },
{ 0x0084, EXIF_FORMAT_RATIONAL_UNSIGNED, -1,	"MkN.Nikon.LensFocalLength",
							"Lens min/max focal length and aperture", NULL },
{ 0x0085, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.ManualFocusDistance",
							"Manual focus distance", 	NULL },
{ 0x0086, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.DigitalZoomFactor",
							"Digital zoom facotr",		NULL },
{ 0x0087, EXIF_FORMAT_BYTE_UNSIGNED, 1,		"MkN.Nikon.FlashUsed",	"Flash used",	NikonTag2FlashUsed },
{ 0x0088, EXIF_FORMAT_UNDEFINED, -1,		"MkN.Nikon.AutoFocusArea", NULL,	NULL },
/* { 0x0089, EXIF_FORMAT_SHORT_UNSIGNED, -1,	"MkN.Nikon.Bracket/ShootingMode", NULL,	NULL }, */
{ 0x008d, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.ColorMode",	"Color mode",	NULL },
{ 0x008f, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.SceneMode",	NULL,		NULL },
{ 0x0090, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.LightingType", "Lighting type", NULL },
{ 0x0092, EXIF_FORMAT_SHORT, 1,			"MkN.Nikon.HueAdjust",	"Hue adjustment", NULL },
/* { 0x0094, EXIF_FORMAT_SHORT_UNSIGNED, 1,	"MkN.Nikon.Saturation",	"Saturation",	NikonTag2Saturation }, */
{ 0x0095, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.NoiseReduction", "Noise reduction", NULL },
{ 0x00a7, EXIF_FORMAT_LONG_UNSIGNED, 1,		"MkN.Nikon.ShutterCount", "Shutter release count", NULL },
{ 0x00a9, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.ImageOptimization", "Image optimization", NULL },
{ 0x00aa, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.Saturation", "Saturation",	NULL },
{ 0x00ab, EXIF_FORMAT_STRING, -1,		"MkN.Nikon.DigitalVariProg", "Digital Vari-program", NULL },
EXIF_MARKER_LIST_END
};


gint format_nikon_makernote(ExifData *exif, unsigned char *tiff, guint offset,
			    guint size, ExifByteOrder bo)
{
	unsigned char *data;

	if (offset + 8 + 4 >= size) return FALSE;

	data = tiff + offset;

	/* Nikon tag format 1 */
	if (memcmp(data, "Nikon\x00\x01\x00", 8) == 0)
		{
		if (exif_parse_IFD_table(exif, tiff, offset + 8, size,
					 bo, 0, NikonExifMarkersList1) != 0)
			{
			return FALSE;
			}
		return TRUE;
		}

	/* Nikon tag format 2 uses Embedded tiff header */
	if (memcmp(data, "Nikon\x00\x02\x00\x00\x00", 10) == 0 ||
	    memcmp(data, "Nikon\x00\x02\x10\x00\x00", 10) == 0)
		{
		guint tiff_header;

		tiff_header = offset + 10;
		if (exif_tiff_parse(exif, tiff + tiff_header, size - tiff_header,
		    NikonExifMarkersList2) != 0)
			{
			return FALSE;
			}
		return TRUE;
		}

	/* Nikon tag format 3 uses format 2 tags without "Nikon" and tiff header */
	if (exif_parse_IFD_table(exif, tiff, offset, size,
				 bo, 0, NikonExifMarkersList2) != 0)
		{
		return FALSE;
		}

	return FALSE;
}

