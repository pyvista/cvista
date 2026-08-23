Description of the textures used
---------------------------------

# BlueNoiseTexture64x64.raw

This single-channel blue noise texture of dimensions 64x64 is part of a
a public domain blue noise textures set on http://momentsingraphics.de.

It ships as raw single-channel bytes (64x64, one byte per texel, row-major)
rather than as a JPEG. cvista decodes it at build time instead of at runtime so
that RenderingOpenGL2 does not depend on the IO tier (vtkJPEGReader); the bytes
are the exact output vtkJPEGReader produced from the original JPEG, so the
texture is unchanged.
