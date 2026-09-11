`exif-thumbnail.jpg` is a synthetic 16x16 solid RGB(120,70,20) JPEG with an
embedded JPEG thumbnail in its EXIF APP1 segment (TIFF IFD1, JPEGInterchangeFormat
and JPEGInterchangeFormatLength tags). It contains no phone captures or metadata.

It is 1330 bytes. The thumbnail EOI occurs at byte 698; searching for the first
FFD9 incorrectly returns a 700-byte prefix instead of the complete outer image.
The pipeline test decodes it with the production stb configuration and verifies
the resulting 256x256 PNG, including its pixel color.
